#ifndef KIRITO_EVALUATOR_HPP
#define KIRITO_EVALUATOR_HPP

#include <variant>

#include "ast.hpp"
#include "object.hpp"
#include "vm.hpp"

namespace kirito {

// Tree-walking evaluator: an AST visitor that yields a Handle per expression. Because visit()
// returns void (to keep the AST free of value-layer types), the result is stashed in result_
// and read back by eval().
class Evaluator : public ast::ExprVisitor, public ast::StmtVisitor {
public:
    explicit Evaluator(KiritoVM& vm) : vm_(vm) {}

    // Returns the value of the last statement's expression, or None for an empty program.
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

    void visit(const ast::ExprStmt& s) override { result_ = eval(*s.expr); }

    void visit(const ast::LiteralExpr& e) override {
        if (std::holds_alternative<int64_t>(e.value))
            result_ = vm_.makeInt(std::get<int64_t>(e.value));
        else if (std::holds_alternative<double>(e.value))
            result_ = vm_.makeFloat(std::get<double>(e.value));
        else
            result_ = vm_.none();
    }

    void visit(const ast::UnaryExpr& e) override {
        Handle operand = eval(*e.operand);
        result_ = located(e.span, [&] { return vm_.arena().deref(operand).unary(vm_, e.op, operand); });
    }

    void visit(const ast::BinaryExpr& e) override {
        Handle lhs = eval(*e.lhs);
        Handle rhs = eval(*e.rhs);
        result_ = located(e.span, [&] { return vm_.arena().deref(lhs).binary(vm_, e.op, lhs, rhs); });
    }

private:
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
    Handle result_{};
};

}  // namespace kirito

#endif
