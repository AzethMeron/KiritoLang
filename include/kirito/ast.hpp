#ifndef KIRITO_AST_HPP
#define KIRITO_AST_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "common.hpp"

namespace kirito::ast {

// The AST is the stable contract between the front end and the evaluator. Nodes carry only
// literal payloads, child pointers, and a source span — nothing from the value/VM layer. The
// evaluator visits via accept(); a future bytecode compiler could be a second visitor.

struct LiteralExpr;
struct NameExpr;
struct UnaryExpr;
struct BinaryExpr;
struct LogicalExpr;

struct ExprVisitor {
    virtual ~ExprVisitor() = default;
    virtual void visit(const LiteralExpr&) = 0;
    virtual void visit(const NameExpr&) = 0;
    virtual void visit(const UnaryExpr&) = 0;
    virtual void visit(const BinaryExpr&) = 0;
    virtual void visit(const LogicalExpr&) = 0;
};

struct Expr {
    SourceSpan span;
    virtual ~Expr() = default;
    virtual void accept(ExprVisitor&) const = 0;
};
using ExprPtr = std::unique_ptr<Expr>;

// std::monostate == the None literal.
struct LiteralExpr : Expr {
    std::variant<std::monostate, int64_t, double, bool, std::string> value;
    void accept(ExprVisitor& v) const override { v.visit(*this); }
};

struct NameExpr : Expr {
    std::string name;
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

// `and` / `or` — separate from BinaryExpr because they short-circuit and yield an operand.
struct LogicalExpr : Expr {
    bool isAnd;  // true == `and`, false == `or`
    ExprPtr lhs;
    ExprPtr rhs;
    void accept(ExprVisitor& v) const override { v.visit(*this); }
};

struct ExprStmt;
struct VarDeclStmt;
struct AssignStmt;
struct IfStmt;
struct WhileStmt;
struct BreakStmt;
struct ContinueStmt;

struct StmtVisitor {
    virtual ~StmtVisitor() = default;
    virtual void visit(const ExprStmt&) = 0;
    virtual void visit(const VarDeclStmt&) = 0;
    virtual void visit(const AssignStmt&) = 0;
    virtual void visit(const IfStmt&) = 0;
    virtual void visit(const WhileStmt&) = 0;
    virtual void visit(const BreakStmt&) = 0;
    virtual void visit(const ContinueStmt&) = 0;
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

// `var NAME = init` — declares NAME in the current scope.
struct VarDeclStmt : Stmt {
    std::string name;
    ExprPtr init;
    void accept(StmtVisitor& v) const override { v.visit(*this); }
};

// `target = value` — rebinds an existing name (target is a NameExpr for now).
struct AssignStmt : Stmt {
    ExprPtr target;
    ExprPtr value;
    void accept(StmtVisitor& v) const override { v.visit(*this); }
};

using Block = std::vector<StmtPtr>;

// `if cond: ... [elif cond: ...] [else: ...]` — one entry per if/elif branch, plus optional else.
struct IfStmt : Stmt {
    std::vector<std::pair<ExprPtr, Block>> branches;
    std::optional<Block> orelse;
    void accept(StmtVisitor& v) const override { v.visit(*this); }
};

struct WhileStmt : Stmt {
    ExprPtr cond;
    Block body;
    void accept(StmtVisitor& v) const override { v.visit(*this); }
};

struct BreakStmt : Stmt {
    void accept(StmtVisitor& v) const override { v.visit(*this); }
};

struct ContinueStmt : Stmt {
    void accept(StmtVisitor& v) const override { v.visit(*this); }
};

struct Program {
    Block stmts;
};

}  // namespace kirito::ast

#endif
