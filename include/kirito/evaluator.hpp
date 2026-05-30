#ifndef KIRITO_EVALUATOR_HPP
#define KIRITO_EVALUATOR_HPP

#include <string>
#include <variant>

#include "ast.hpp"
#include "environment.hpp"
#include "object.hpp"
#include "vm.hpp"

namespace kirito {

// Tree-walking evaluator: an AST visitor that yields a Handle per expression. Because visit()
// returns void (to keep the AST free of value-layer types), the result is stashed in result_
// and read back by eval(). Statements run against the current scope handle (env_).
class Evaluator : public ast::ExprVisitor, public ast::StmtVisitor {
public:
    Evaluator(KiritoVM& vm, Handle env) : vm_(vm), env_(env) {}

    // Returns the value of the last statement's expression, or None.
    Handle run(const ast::Program& prog) {
        Handle last = vm_.none();
        for (const auto& stmt : prog.stmts) {
            stmt->accept(*this);
            last = result_;
        }
        return last;
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
        const auto* target = dynamic_cast<const ast::NameExpr*>(s.target.get());
        if (!target) throw KiritoError("invalid assignment target", s.span);
        Handle value = eval(*s.value);
        if (!envAssign(vm_.arena(), env_, target->name, value))
            throw KiritoError("name '" + target->name + "' is not defined", s.span);
        result_ = vm_.none();
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
        Handle operand = eval(*e.operand);
        result_ = located(e.span, [&] { return vm_.arena().deref(operand).unary(vm_, e.op, operand); });
    }

    void visit(const ast::BinaryExpr& e) override {
        Handle lhs = eval(*e.lhs);
        Handle rhs = eval(*e.rhs);
        // Equality never raises on type mismatch (1 == "x" is False); it uses the value protocol's
        // structural equals. Ordering and arithmetic dispatch through binary().
        if (e.op == BinOp::Eq || e.op == BinOp::Ne) {
            bool eq = vm_.arena().deref(lhs).equals(vm_.arena(), vm_.arena().deref(rhs));
            result_ = vm_.makeBool(e.op == BinOp::Eq ? eq : !eq);
            return;
        }
        result_ = located(e.span, [&] { return vm_.arena().deref(lhs).binary(vm_, e.op, lhs, rhs); });
    }

private:
    EnvValue& scope() { return static_cast<EnvValue&>(vm_.arena().deref(env_)); }

    // Run an operation, tagging any location-less runtime error with this node's span.
    template <typename F>
    Handle located(SourceSpan span, F&& fn) {
        try {
            return fn();
        } catch (KiritoError& err) {
            if (err.span.line == 0) err.span = span;
            throw;
        }
    }

    KiritoVM& vm_;
    Handle env_;
    Handle result_{};
};

}  // namespace kirito

#endif
