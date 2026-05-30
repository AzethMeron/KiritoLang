#ifndef KIRITO_AST_HPP
#define KIRITO_AST_HPP

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include "common.hpp"

namespace kirito::ast {

// The AST is the stable contract between the front end and the evaluator. Nodes carry only
// literal payloads, child pointers, and a source span — nothing from the value/VM layer. The
// evaluator visits via accept(); a future bytecode compiler could be a second visitor.

struct LiteralExpr;
struct UnaryExpr;
struct BinaryExpr;

struct ExprVisitor {
    virtual ~ExprVisitor() = default;
    virtual void visit(const LiteralExpr&) = 0;
    virtual void visit(const UnaryExpr&) = 0;
    virtual void visit(const BinaryExpr&) = 0;
};

struct Expr {
    SourceSpan span;
    virtual ~Expr() = default;
    virtual void accept(ExprVisitor&) const = 0;
};
using ExprPtr = std::unique_ptr<Expr>;

// std::monostate == the None literal.
struct LiteralExpr : Expr {
    std::variant<std::monostate, int64_t, double> value;
    void accept(ExprVisitor& v) const override { v.visit(*this); }
};

struct UnaryExpr : Expr {
    UnOp op;
    ExprPtr operand;
    void accept(ExprVisitor& v) const override { v.visit(*this); }
};

struct BinaryExpr : Expr {
    BinOp op;
    ExprPtr lhs;
    ExprPtr rhs;
    void accept(ExprVisitor& v) const override { v.visit(*this); }
};

struct ExprStmt;

struct StmtVisitor {
    virtual ~StmtVisitor() = default;
    virtual void visit(const ExprStmt&) = 0;
};

struct Stmt {
    SourceSpan span;
    virtual ~Stmt() = default;
    virtual void accept(StmtVisitor&) const = 0;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct ExprStmt : Stmt {
    ExprPtr expr;
    void accept(StmtVisitor& v) const override { v.visit(*this); }
};

struct Program {
    std::vector<StmtPtr> stmts;
};

}  // namespace kirito::ast

#endif
