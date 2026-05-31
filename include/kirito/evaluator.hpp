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

    // The class whose method this evaluator is running (for private-member access) and the name of
    // that method (for self._super_()). Set by KiFunction::callFull for method bodies.
    void setCurrentClass(Handle cls) { currentClass_ = cls; hasCurrentClass_ = true; }

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
    void visit(const ast::DiscardStmt& s) override { eval(*s.expr); result_ = vm_.none(); }

    void visit(const ast::VarDeclStmt& s) override {
        RootScope rs(vm_);
        Handle value = rs.add(eval(*s.init));
        if (s.names.size() == 1 && s.starIndex == -1) {
            scope().define(s.names[0], value);
        } else {
            std::vector<Handle> slots = spreadValues(value, s.names.size(), s.starIndex, s.span);
            for (Handle h : slots) rs.add(h);
            for (std::size_t i = 0; i < s.names.size(); ++i) scope().define(s.names[i], slots[i]);
        }
        result_ = vm_.none();
    }

    void visit(const ast::AssignStmt& s) override {
        RootScope rs(vm_);
        Handle value = rs.add(eval(*s.value));
        if (s.target->exprKind() == ast::ExprKind::Tuple) {
            const auto& tup = static_cast<const ast::TupleExpr&>(*s.target);
            int starIndex = -1;
            for (std::size_t i = 0; i < tup.elems.size(); ++i)
                if (tup.elems[i]->exprKind() == ast::ExprKind::Star) {
                    if (starIndex != -1) throw KiritoError("two starred targets in assignment", s.span);
                    starIndex = static_cast<int>(i);
                }
            std::vector<Handle> slots = spreadValues(value, tup.elems.size(), starIndex, s.span);
            for (Handle h : slots) rs.add(h);
            for (std::size_t i = 0; i < tup.elems.size(); ++i) {
                const ast::Expr* tgt = tup.elems[i].get();
                if (tgt->exprKind() == ast::ExprKind::Star)
                    tgt = static_cast<const ast::StarExpr&>(*tgt).inner.get();
                assignSingle(*tgt, slots[i], rs, s.span);
            }
        } else {
            assignSingle(*s.target, value, rs, s.span);
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
        if (!items)
            throw KiritoError("type '" + vm_.arena().deref(iterable).typeName() + "' is not iterable", s.span);
        // Iterables may yield freshly-allocated values (e.g. a String's characters) that aren't
        // reachable from the iterable itself, so root them all for the loop's duration.
        for (Handle it : items.value()) rs.add(it);
        bool single = s.vars.size() == 1 && s.starIndex == -1;
        for (Handle item : items.value()) {
            if (single) {
                scope().define(s.vars[0], item);
            } else {
                RootScope itemRoots(vm_);
                std::vector<Handle> slots = spreadValues(item, s.vars.size(), s.starIndex, s.span);
                for (Handle h : slots) itemRoots.add(h);
                for (std::size_t i = 0; i < s.vars.size(); ++i) scope().define(s.vars[i], slots[i]);
            }
            execBlock(s.body);
            if (flow_ == Flow::Break) { flow_ = Flow::Normal; break; }
            if (flow_ == Flow::Continue) { flow_ = Flow::Normal; continue; }
            if (flow_ == Flow::Return) break;
        }
        result_ = vm_.none();
    }

    void visit(const ast::BreakStmt&) override { flow_ = Flow::Break; result_ = vm_.none(); }
    void visit(const ast::ContinueStmt&) override { flow_ = Flow::Continue; result_ = vm_.none(); }
    void visit(const ast::PassStmt&) override { result_ = vm_.none(); }

    void visit(const ast::AssertStmt& s) override {
        if (!truthy(eval(*s.cond))) {
            Handle msg = s.message ? eval(*s.message) : vm_.makeString("assertion failed");
            throw KiritoThrow{msg};
        }
        result_ = vm_.none();
    }

    void visit(const ast::ReturnStmt& s) override {
        returnValue_ = s.value ? eval(*s.value) : vm_.none();
        flow_ = Flow::Return;
        result_ = returnValue_;
    }

    void visit(const ast::ThrowStmt& s) override {
        throw KiritoThrow{eval(*s.value)};
    }

    void visit(const ast::WithStmt& s) override {
        RootScope rs(vm_);
        Handle mgr = rs.add(eval(*s.context));
        Handle value = rs.add(callMethod(s.span, mgr, "_enter_", {}));
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
        } catch (const std::exception& ex) {
            // Any other native exception still triggers _exit_, then propagates as a KiritoError.
            pendingError = true;
            errMsg = ex.what();
        }
        flow_ = Flow::Normal;
        callMethod(s.span, mgr, "_exit_", {});  // always run cleanup
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
        for (const auto& [k, v] : static_cast<EnvValue&>(vm_.arena().deref(classScope)).locals())
            cls->methods[k] = v;
        Handle clsHandle = vm_.alloc(std::move(cls));
        auto& klass = static_cast<ClassValue&>(vm_.arena().deref(clsHandle));
        klass.selfHandle = clsHandle;
        // Tag each method with its owning class so its body may access private members.
        for (const auto& [mname, mh] : klass.methods) {
            Object& mo = vm_.arena().deref(mh);
            if (mo.kind() == ValueKind::Function) {
                auto& fn = static_cast<KiFunction&>(mo);
                fn.ownerClass = clsHandle;
                fn.hasOwner = true;
            }
        }
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
        } catch (const std::exception& ex) {
            // Any other std::exception from native code (a C++-authored module, std::bad_alloc, ...)
            // is also catchable as a String, so `try`/`catch` can guard the whole boundary.
            flow_ = Flow::Normal;
            pending = true;
            excValue = rs.add(vm_.makeString(ex.what()));
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
        std::vector<Handle> positional;
        std::vector<KiFunction::NamedArg> named;
        for (const auto& arg : e.args) {
            Handle v = rs.add(eval(*arg.value));
            if (arg.name.empty()) {
                if (!named.empty())
                    throw KiritoError("positional argument follows keyword argument", e.span);
                positional.push_back(v);
            } else {
                named.push_back({arg.name, v});
            }
        }
        // Kirito functions support named args, defaults, and annotation enforcement; other callables
        // (native functions, classes) accept positional only.
        if (auto* fn = dynamic_cast<KiFunction*>(&vm_.arena().deref(callee))) {
            result_ = located(e.span, [&] { return fn->callFull(vm_, positional, named); });
        } else {
            if (!named.empty())
                throw KiritoError("this callable does not accept keyword arguments", e.span);
            result_ = located(e.span, [&] { return vm_.arena().deref(callee).call(vm_, positional); });
        }
    }

    void visit(const ast::BinaryExpr& e) override {
        RootScope rs(vm_);
        Handle lhs = rs.add(eval(*e.lhs));
        Handle rhs = rs.add(eval(*e.rhs));
        // Equality never raises on type mismatch (1 == "x" is False); it uses the value protocol's
        // structural equals. Ordering and arithmetic dispatch through binary().
        if (e.op == BinOp::Eq || e.op == BinOp::Ne) {
            // A user class may override equality via _eq_ / _ne_; otherwise use structural equals.
            // Only an Instance can carry an _eq_ method, so the (relatively costly) dynamic_cast is
            // guarded by a kind() check — the scalar fast path never pays for it.
            Object& l = vm_.arena().deref(lhs);
            if (l.kind() == ValueKind::Instance) {
                if (auto* inst = dynamic_cast<InstanceValue*>(&l);
                    inst && inst->findMethod(vm_.arena(), binOpMethod(e.op))) {
                    result_ = located(e.span, [&] { return l.binary(vm_, e.op, lhs, rhs); });
                    return;
                }
            }
            bool eq = l.equals(vm_.arena(), vm_.arena().deref(rhs));
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
        // `self._super_()` (the super operator): unless a class explicitly defines `_super_`, accessing
        // it yields a builder that returns a SuperValue — a parent view of `obj` whose method lookup
        // starts at the base of the CURRENTLY-EXECUTING method's class (so multi-level chains walk up
        // one level per call). Only meaningful inside a method; a baseless class raises when called.
        if (e.name == "_super_" && hasCurrentClass_) {
            Object& o = vm_.arena().deref(obj);
            if (o.kind() == ValueKind::Instance && dynamic_cast<InstanceValue*>(&o)) {
                const auto* inst = static_cast<InstanceValue*>(&o);
                const auto& ownerCls = static_cast<const ClassValue&>(vm_.arena().deref(currentClass_));
                if (!ownerCls.findMethod(vm_.arena(), "_super_")) {  // not overridden
                    Handle objH = obj, ownerH = currentClass_;
                    result_ = vm_.alloc(std::make_unique<NativeFunction>(
                        "_super_",
                        [objH, ownerH](KiritoVM& vm, std::span<const Handle>) -> Handle {
                            const auto& oc = static_cast<const ClassValue&>(vm.arena().deref(ownerH));
                            if (!oc.hasBase)
                                throw KiritoError("_super_() called in '" + oc.name +
                                                  "', which does not inherit from any class");
                            auto sup = std::make_unique<SuperValue>();
                            sup->instance = objH;
                            sup->startClass = oc.base;
                            return vm.alloc(std::move(sup));
                        },
                        std::vector<Handle>{objH, ownerH}));
                    return;
                }
            }
        }
        checkPrivateAccess(obj, e.name, e.span);
        result_ = located(e.span, [&] { return vm_.arena().deref(obj).getAttr(vm_, obj, e.name); });
    }

    void visit(const ast::IndexExpr& e) override {
        RootScope rs(vm_);
        Handle obj = rs.add(eval(*e.object));
        std::vector<Handle> keys;
        keys.reserve(e.indices.size());
        for (const auto& ix : e.indices) keys.push_back(rs.add(eval(*ix)));
        result_ = located(e.span, [&] { return vm_.arena().deref(obj).getItem(vm_, keys); });
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

    // Packing: a bare comma sequence `a, b` evaluates to a List.
    void visit(const ast::TupleExpr& e) override {
        RootScope rs(vm_);
        auto list = std::make_unique<ListVal>();
        list->elems.reserve(e.elems.size());
        for (const auto& elem : e.elems) {
            if (elem->exprKind() == ast::ExprKind::Star)
                throw KiritoError("starred expression is only valid as an assignment target", elem->span);
            list->elems.push_back(rs.add(eval(*elem)));
        }
        result_ = vm_.alloc(std::move(list));
    }

    void visit(const ast::StarExpr& e) override {
        throw KiritoError("starred expression is only valid as an assignment target", e.span);
    }

    // Bind one (non-tuple) target — a name, an index, or a member — to `value`.
    void assignSingle(const ast::Expr& target, Handle value, RootScope& rs, SourceSpan span) {
        switch (target.exprKind()) {
            case ast::ExprKind::Name: {
                const auto& name = static_cast<const ast::NameExpr&>(target);
                if (!envAssign(vm_.arena(), env_, name.name, value))
                    throw KiritoError("name '" + name.name + "' is not defined", span);
                break;
            }
            case ast::ExprKind::Index: {
                const auto& idx = static_cast<const ast::IndexExpr&>(target);
                Handle obj = rs.add(eval(*idx.object));
                std::vector<Handle> keys;
                for (const auto& ix : idx.indices) keys.push_back(rs.add(eval(*ix)));
                located(span, [&] { vm_.arena().deref(obj).setItem(vm_, keys, value); return vm_.none(); });
                break;
            }
            case ast::ExprKind::Member: {
                const auto& mem = static_cast<const ast::MemberExpr&>(target);
                Handle obj = rs.add(eval(*mem.object));
                checkPrivateAccess(obj, mem.name, span);
                located(span, [&] { vm_.arena().deref(obj).setAttr(vm_, mem.name, value); return vm_.none(); });
                break;
            }
            default:
                throw KiritoError("invalid assignment target", span);
        }
    }

    // Spread an iterable `value` across `n` unpack slots, with an optional starred slot at
    // `starIndex` (-1 if none) that absorbs the surplus into a List. Returns the n slot values.
    std::vector<Handle> spreadValues(Handle value, std::size_t n, int starIndex, SourceSpan span) {
        auto items = located(span, [&] { return vm_.arena().deref(value).iterate(vm_); });
        if (!items)
            throw KiritoError("cannot unpack non-iterable '" + vm_.arena().deref(value).typeName() + "'", span);
        RootScope rs(vm_);  // keep iterated (possibly freshly-allocated) items alive during alloc below
        for (Handle it : items.value()) rs.add(it);
        std::vector<Handle>& v = items.value();
        std::vector<Handle> slots(n);
        if (starIndex == -1) {
            if (v.size() != n)
                throw KiritoError("expected " + std::to_string(n) + " values to unpack, got " +
                                  std::to_string(v.size()), span);
            for (std::size_t i = 0; i < n; ++i) slots[i] = v[i];
        } else {
            std::size_t before = static_cast<std::size_t>(starIndex), after = n - 1 - before;
            if (v.size() < before + after)
                throw KiritoError("expected at least " + std::to_string(before + after) +
                                  " values to unpack, got " + std::to_string(v.size()), span);
            for (std::size_t i = 0; i < before; ++i) slots[i] = v[i];
            auto mid = std::make_unique<ListVal>();
            for (std::size_t i = before; i < v.size() - after; ++i) mid->elems.push_back(v[i]);
            slots[before] = vm_.alloc(std::move(mid));
            for (std::size_t j = 0; j < after; ++j) slots[n - 1 - j] = v[v.size() - 1 - j];
        }
        return slots;
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

    void visit(const ast::FStringExpr& e) override {
        RootScope rs(vm_);
        std::string out;
        for (const auto& part : e.parts) {
            if (part.isExpr) out += vm_.stringify(rs.add(eval(*part.expr)));
            else out += part.literal;
        }
        result_ = vm_.makeString(std::move(out));
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

    // Whether a catch clause catches this exception. A clause with no type is a catch-all;
    // typed matching against user classes is wired up in the classes layer (isInstanceOf).
    bool exceptionMatches(const ast::CatchClause& clause, Handle excValue) {
        if (!clause.type) return true;
        Handle typeH = eval(*clause.type);
        return isInstanceOf(excValue, typeH);
    }

    // True if `value` is an instance of class `typeH` (walking the base chain). Only user-defined
    // InstanceValues carry a class handle; C++ NativeClass objects (Matrix/Socket/...) report
    // ValueKind::Instance too, so use dynamic_cast to tell them apart.
    bool isInstanceOf(Handle value, Handle typeH) {
        if (vm_.arena().deref(typeH).kind() != ValueKind::Class) return false;
        const Object& v = vm_.arena().deref(value);
        const auto* inst = dynamic_cast<const InstanceValue*>(&v);
        if (!inst) return false;
        Handle cur = inst->cls;
        while (true) {
            if (cur == typeH) return true;
            const auto& c = static_cast<const ClassValue&>(vm_.arena().deref(cur));
            if (!c.hasBase) return false;
            cur = c.base;
        }
    }

    // A private member (_name, no trailing underscore) may only be touched from within a method
    // whose class the receiver is an instance of (i.e. while running such a method).
    void checkPrivateAccess(Handle obj, const std::string& name, SourceSpan span) {
        if (!isPrivateName(name)) return;
        if (!dynamic_cast<const InstanceValue*>(&vm_.arena().deref(obj))) return;  // user classes only
        if (hasCurrentClass_ && isInstanceOf(obj, currentClass_)) return;
        throw KiritoError("cannot access private member '" + name + "' of '" +
                          vm_.arena().deref(obj).typeName() + "' outside its class", span);
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
    Handle currentClass_{};
    bool hasCurrentClass_ = false;
};

}  // namespace kirito

#endif
