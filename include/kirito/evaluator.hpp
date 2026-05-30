#ifndef KIRITO_EVALUATOR_HPP
#define KIRITO_EVALUATOR_HPP

#include <string>
#include <variant>

#include <memory>
#include <vector>

#include "ast.hpp"
#include "class_value.hpp"
#include "collections.hpp"
#include "control.hpp"
#include "environment.hpp"
#include "exceptions.hpp"
#include "function.hpp"
#include "object.hpp"
#include "vm.hpp"

namespace kirito {

// Tree-walking evaluator: an AST visitor that yields a Handle per expression. Because visit()
// returns void (to keep the AST free of value-layer types), the result is stashed in result_
// and read back by eval(). Statements run against the current scope handle (env_).
class Evaluator : public ast::ExprVisitor, public ast::StmtVisitor {
public:
    Evaluator(KiritoVM& vm, Handle env) : vm_(vm), env_(env), rootBase_(vm.tempMark()) {
        vm_.pushTemp(env_);  // this frame's scope is a GC root for the evaluator's lifetime
    }
    ~Evaluator() override { vm_.popTempTo(rootBase_); }
    Evaluator(const Evaluator&) = delete;
    Evaluator& operator=(const Evaluator&) = delete;

    // Returns the value of the last statement's expression, or None.
    Handle run(const ast::Program& prog) {
        result_ = vm_.none();
        execBlock(prog.stmts);
        return result_;
    }

    // Execute statements until one signals non-Normal control flow (break/continue/return).
    void execBlock(const ast::Block& stmts) {
        for (const auto& stmt : stmts) {
            stmt->accept(*this);
            if (flow_ != Flow::Normal) return;
        }
    }

    // Run a function body in this evaluator's scope; returns the explicit return value or None.
    Handle callBody(const ast::Block& body) {
        flow_ = Flow::Normal;
        result_ = vm_.none();
        execBlock(body);
        Handle ret = (flow_ == Flow::Return) ? returnValue_ : vm_.none();
        flow_ = Flow::Normal;
        return ret;
    }

    Handle eval(const ast::Expr& e) {
        e.accept(*this);
        return result_;
    }

    // --- statements ---
    void visit(const ast::ExprStmt& s) override { result_ = eval(*s.expr); }

    void visit(const ast::VarDeclStmt& s) override {
        Handle value = eval(*s.init);
        scope().define(s.name, value);
        result_ = vm_.none();
    }

    void visit(const ast::AssignStmt& s) override {
        RootScope rs(vm_);
        Handle value = rs.add(eval(*s.value));
        switch (s.target->exprKind()) {
            case ast::ExprKind::Name: {
                const auto& name = static_cast<const ast::NameExpr&>(*s.target);
                if (!envAssign(vm_.arena(), env_, name.name, value))
                    throw KiritoError("name '" + name.name + "' is not defined", s.span);
                break;
            }
            case ast::ExprKind::Index: {
                const auto& idx = static_cast<const ast::IndexExpr&>(*s.target);
                Handle obj = rs.add(eval(*idx.object));
                Handle key = rs.add(eval(*idx.index));
                located(s.span, [&] {
                    vm_.arena().deref(obj).setItem(vm_, key, value);
                    return vm_.none();
                });
                break;
            }
            case ast::ExprKind::Member: {
                const auto& mem = static_cast<const ast::MemberExpr&>(*s.target);
                Handle obj = rs.add(eval(*mem.object));
                located(s.span, [&] {
                    vm_.arena().deref(obj).setAttr(vm_, mem.name, value);
                    return vm_.none();
                });
                break;
            }
            default:
                throw KiritoError("invalid assignment target", s.span);
        }
        result_ = vm_.none();
    }

    void visit(const ast::IfStmt& s) override {
        for (const auto& [cond, body] : s.branches) {
            if (truthy(eval(*cond))) { execBlock(body); return; }
        }
        if (s.orelse) execBlock(*s.orelse);
        result_ = vm_.none();
    }

    void visit(const ast::WhileStmt& s) override {
        while (truthy(eval(*s.cond))) {
            execBlock(s.body);
            if (flow_ == Flow::Break) { flow_ = Flow::Normal; break; }
            if (flow_ == Flow::Continue) { flow_ = Flow::Normal; continue; }
            if (flow_ == Flow::Return) break;  // propagate to the enclosing function
        }
        result_ = vm_.none();
    }

    void visit(const ast::ForStmt& s) override {
        RootScope rs(vm_);
        Handle iterable = rs.add(eval(*s.iterable));
        auto items = located(s.span, [&] { return vm_.arena().deref(iterable).iterate(vm_); });
        // Iterables may yield freshly-allocated values (e.g. a String's characters) that aren't
        // reachable from the iterable itself, so root them all for the loop's duration.
        for (Handle it : items.value()) rs.add(it);
        for (Handle item : items.value()) {
            scope().define(s.var, item);
            execBlock(s.body);
            if (flow_ == Flow::Break) { flow_ = Flow::Normal; break; }
            if (flow_ == Flow::Continue) { flow_ = Flow::Normal; continue; }
            if (flow_ == Flow::Return) break;
        }
        result_ = vm_.none();
    }

    void visit(const ast::BreakStmt&) override { flow_ = Flow::Break; result_ = vm_.none(); }
    void visit(const ast::ContinueStmt&) override { flow_ = Flow::Continue; result_ = vm_.none(); }

    void visit(const ast::ReturnStmt& s) override {
        returnValue_ = s.value ? eval(*s.value) : vm_.none();
        flow_ = Flow::Return;
        result_ = returnValue_;
    }

    void visit(const ast::RaiseStmt& s) override {
        throw KiritoThrow{eval(*s.value)};
    }

    void visit(const ast::WithStmt& s) override {
        RootScope rs(vm_);
        Handle mgr = rs.add(eval(*s.context));
        Handle value = rs.add(callMethod(s.span, mgr, "enter", {}));
        if (!s.name.empty()) scope().define(s.name, value);

        bool pendingThrow = false;
        Handle excValue = vm_.none();
        bool pendingError = false;
        std::string errMsg;
        Flow flowAfter = Flow::Normal;
        try {
            execBlock(s.body);
            flowAfter = flow_;
        } catch (const KiritoThrow& t) {
            pendingThrow = true;
            excValue = rs.add(t.value);
        } catch (const KiritoError& e) {
            pendingError = true;
            errMsg = e.what();
        }
        flow_ = Flow::Normal;
        callMethod(s.span, mgr, "exit", {});  // always run cleanup
        if (pendingThrow) throw KiritoThrow{excValue};
        if (pendingError) throw KiritoError(errMsg);
        flow_ = flowAfter;
        result_ = vm_.none();
    }

    void visit(const ast::ClassStmt& s) override {
        RootScope rs(vm_);
        Handle base{};
        bool hasBase = false;
        if (s.base) { base = rs.add(eval(*s.base)); hasBase = true; }
        // Run the class body in a fresh scope; the names it defines become the class's methods.
        Handle classScope = rs.add(vm_.newScope(env_));
        { Evaluator sub(vm_, classScope); sub.execBlock(s.body); }
        auto cls = std::make_unique<ClassValue>();
        cls->name = s.name;
        cls->base = base;
        cls->hasBase = hasBase;
        cls->methods = static_cast<EnvValue&>(vm_.arena().deref(classScope)).locals();
        Handle clsHandle = vm_.alloc(std::move(cls));
        static_cast<ClassValue&>(vm_.arena().deref(clsHandle)).selfHandle = clsHandle;
        scope().define(s.name, clsHandle);
        result_ = vm_.none();
    }

    void visit(const ast::TryStmt& s) override {
        RootScope rs(vm_);
        bool pending = false;
        Handle excValue = vm_.none();
        try {
            execBlock(s.body);
        } catch (const KiritoThrow& t) {
            pending = true;
            excValue = rs.add(t.value);
        } catch (const KiritoError& err) {
            // Internal/runtime errors are surfaced to Kirito code as a (String) exception value.
            flow_ = Flow::Normal;
            pending = true;
            excValue = rs.add(vm_.makeString(err.what()));
        }
        Flow flowAfter = flow_;  // control flow from the body (when no exception escaped)
        if (pending) {
            flow_ = Flow::Normal;
            for (const auto& h : s.handlers) {
                if (exceptionMatches(h, excValue)) {
                    if (!h.name.empty()) scope().define(h.name, excValue);
                    execBlock(h.body);
                    pending = false;
                    flowAfter = flow_;
                    break;
                }
            }
        }
        if (s.hasFinally) {
            RootScope frs(vm_);
            Handle savedReturn = frs.add(returnValue_);
            flow_ = Flow::Normal;
            execBlock(s.finallyBody);
            if (flow_ != Flow::Normal)  // a return/break/continue in finally overrides everything
                return;
            flow_ = flowAfter;
            returnValue_ = savedReturn;
        }
        if (pending) throw KiritoThrow{excValue};
        // result_ carries the value of the last expression in the executed body/handler.
    }

    // --- expressions ---
    void visit(const ast::LiteralExpr& e) override {
        if (std::holds_alternative<int64_t>(e.value))
            result_ = vm_.makeInt(std::get<int64_t>(e.value));
        else if (std::holds_alternative<double>(e.value))
            result_ = vm_.makeFloat(std::get<double>(e.value));
        else if (std::holds_alternative<bool>(e.value))
            result_ = vm_.makeBool(std::get<bool>(e.value));
        else if (std::holds_alternative<std::string>(e.value))
            result_ = vm_.makeString(std::get<std::string>(e.value));
        else
            result_ = vm_.none();
    }

    void visit(const ast::NameExpr& e) override {
        auto found = envLookup(vm_.arena(), env_, e.name);
        if (!found) throw KiritoError("name '" + e.name + "' is not defined", e.span);
        result_ = *found;
    }

    void visit(const ast::UnaryExpr& e) override {
        if (e.op == UnOp::Not) {
            result_ = vm_.makeBool(!truthy(eval(*e.operand)));
            return;
        }
        RootScope rs(vm_);
        Handle operand = rs.add(eval(*e.operand));
        result_ = located(e.span, [&] { return vm_.arena().deref(operand).unary(vm_, e.op, operand); });
    }

    void visit(const ast::LogicalExpr& e) override {
        // Short-circuit, Python-style: yield the operand that decides the result.
        RootScope rs(vm_);
        Handle lhs = rs.add(eval(*e.lhs));
        bool lt = truthy(lhs);
        if (e.isAnd) result_ = lt ? eval(*e.rhs) : lhs;
        else result_ = lt ? lhs : eval(*e.rhs);
    }

    void visit(const ast::FunctionExpr& e) override {
        // Capture the current scope -> closure.
        result_ = vm_.alloc(std::make_unique<KiFunction>(&e, env_));
    }

    void visit(const ast::CallExpr& e) override {
        RootScope rs(vm_);
        Handle callee = rs.add(eval(*e.callee));
        std::vector<Handle> args;
        args.reserve(e.args.size());
        for (const auto& arg : e.args) args.push_back(rs.add(eval(*arg)));
        result_ = located(e.span, [&] { return vm_.arena().deref(callee).call(vm_, args); });
    }

    void visit(const ast::BinaryExpr& e) override {
        RootScope rs(vm_);
        Handle lhs = rs.add(eval(*e.lhs));
        Handle rhs = rs.add(eval(*e.rhs));
        // Equality never raises on type mismatch (1 == "x" is False); it uses the value protocol's
        // structural equals. Ordering and arithmetic dispatch through binary().
        if (e.op == BinOp::Eq || e.op == BinOp::Ne) {
            bool eq = vm_.arena().deref(lhs).equals(vm_.arena(), vm_.arena().deref(rhs));
            result_ = vm_.makeBool(e.op == BinOp::Eq ? eq : !eq);
            return;
        }
        if (e.op == BinOp::In || e.op == BinOp::NotIn) {
            bool c = located(e.span, [&] { return vm_.arena().deref(rhs).contains(vm_, lhs); });
            result_ = vm_.makeBool(e.op == BinOp::In ? c : !c);
            return;
        }
        result_ = located(e.span, [&] { return vm_.arena().deref(lhs).binary(vm_, e.op, lhs, rhs); });
    }

    void visit(const ast::MemberExpr& e) override {
        RootScope rs(vm_);
        Handle obj = rs.add(eval(*e.object));
        result_ = located(e.span, [&] { return vm_.arena().deref(obj).getAttr(vm_, obj, e.name); });
    }

    void visit(const ast::IndexExpr& e) override {
        RootScope rs(vm_);
        Handle obj = rs.add(eval(*e.object));
        Handle key = rs.add(eval(*e.index));
        result_ = located(e.span, [&] { return vm_.arena().deref(obj).getItem(vm_, key); });
    }

    void visit(const ast::SliceExpr& e) override {
        RootScope rs(vm_);
        Handle obj = rs.add(eval(*e.object));
        Handle start = rs.add(e.start ? eval(*e.start) : vm_.none());
        Handle stop = rs.add(e.stop ? eval(*e.stop) : vm_.none());
        Handle step = rs.add(e.step ? eval(*e.step) : vm_.none());
        result_ = located(e.span, [&] { return vm_.arena().deref(obj).slice(vm_, start, stop, step); });
    }

    void visit(const ast::ListLiteral& e) override {
        RootScope rs(vm_);
        auto list = std::make_unique<ListVal>();
        list->elems.reserve(e.elems.size());
        for (const auto& elem : e.elems) list->elems.push_back(rs.add(eval(*elem)));
        result_ = vm_.alloc(std::move(list));
    }

    void visit(const ast::SetLiteral& e) override {
        RootScope rs(vm_);
        auto set = std::make_unique<SetVal>();
        for (const auto& elem : e.elems) {
            Handle h = rs.add(eval(*elem));
            located(e.span, [&] { set->add(vm_.arena(), h); return vm_.none(); });
        }
        result_ = vm_.alloc(std::move(set));
    }

    void visit(const ast::DictLiteral& e) override {
        RootScope rs(vm_);
        auto dict = std::make_unique<DictVal>();
        for (const auto& [key, value] : e.entries) {
            Handle kh = rs.add(eval(*key));
            Handle vh = rs.add(eval(*value));
            located(e.span, [&] { dict->set(vm_.arena(), kh, vh); return vm_.none(); });
        }
        result_ = vm_.alloc(std::move(dict));
    }

private:
    EnvValue& scope() { return static_cast<EnvValue&>(vm_.arena().deref(env_)); }
    bool truthy(Handle h) { return vm_.arena().deref(h).truthy(); }

    // Look up obj.name and call it (e.g. a context manager's enter/exit).
    Handle callMethod(SourceSpan span, Handle obj, const char* name, std::vector<Handle> args) {
        return located(span, [&] {
            Handle method = vm_.arena().deref(obj).getAttr(vm_, obj, name);
            return vm_.arena().deref(method).call(vm_, args);
        });
    }

    // Whether an except clause catches this exception. A clause with no type is a catch-all;
    // typed matching against user classes is wired up in the classes layer (isInstanceOf).
    bool exceptionMatches(const ast::ExceptClause& clause, Handle excValue) {
        if (!clause.type) return true;
        Handle typeH = eval(*clause.type);
        return isInstanceOf(excValue, typeH);
    }

    // True if `value` is an instance of class `typeH` (walking the base chain).
    bool isInstanceOf(Handle value, Handle typeH) {
        if (vm_.arena().deref(typeH).kind() != ValueKind::Class) return false;
        const Object& v = vm_.arena().deref(value);
        if (v.kind() != ValueKind::Instance) return false;
        Handle cur = static_cast<const InstanceValue&>(v).cls;
        while (true) {
            if (cur == typeH) return true;
            const auto& c = static_cast<const ClassValue&>(vm_.arena().deref(cur));
            if (!c.hasBase) return false;
            cur = c.base;
        }
    }

    // Run an operation, tagging any location-less runtime error with this node's span.
    template <typename F>
    auto located(SourceSpan span, F&& fn) -> decltype(fn()) {
        try {
            return fn();
        } catch (KiritoError& err) {
            if (err.span.line == 0) err.span = span;
            throw;
        }
    }

    KiritoVM& vm_;
    Handle env_;
    std::size_t rootBase_;
    Handle result_{};
    Handle returnValue_{};
    Flow flow_ = Flow::Normal;
};

}  // namespace kirito

#endif
