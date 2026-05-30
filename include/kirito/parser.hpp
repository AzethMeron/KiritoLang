#ifndef KIRITO_PARSER_HPP
#define KIRITO_PARSER_HPP

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "common.hpp"
#include "lexer.hpp"

namespace kirito {

// Recursive descent: statements at the top, expressions by precedence level
// (comparison < add < mul < unary < pow < primary), with ** right-associative — so -2**2 == -4
// and 2**3**2 == 512, matching Python.
class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : toks_(std::move(tokens)) {}

    ast::Program parseProgram() {
        ast::Program prog;
        while (!at(TokenType::EndOfFile)) {
            if (at(TokenType::Newline)) { advance(); continue; }
            prog.stmts.push_back(parseStatement());
        }
        return prog;
    }

private:
    const Token& peek() const { return toks_[pos_]; }
    bool at(TokenType t) const { return peek().type == t; }
    const Token& advance() { return toks_[pos_++]; }

    const Token& expect(TokenType t, const char* what) {
        if (!at(t)) throw KiritoError(std::string("expected ") + what, peek().span);
        return advance();
    }
    void expectStatementEnd() {
        if (at(TokenType::Newline)) { advance(); return; }
        if (at(TokenType::EndOfFile)) return;
        throw KiritoError("expected end of statement", peek().span);
    }

    // A simple statement (var/assign/expr/return) is normally newline-terminated, but when its
    // expression ended in an indented block (a Function literal), the closing dedent already
    // terminated it — so no trailing newline is required.
    void endSimpleStatement() {
        if (blockJustClosed_) { blockJustClosed_ = false; return; }
        expectStatementEnd();
    }

    ast::StmtPtr parseStatement() {
        blockJustClosed_ = false;
        switch (peek().type) {
            case TokenType::KwVar: return parseVarDecl();
            case TokenType::KwIf: return parseIf();
            case TokenType::KwWhile: return parseWhile();
            case TokenType::KwFor: return parseFor();
            case TokenType::KwTry: return parseTry();
            case TokenType::KwRaise: return parseRaise();
            case TokenType::KwClass: return parseClass();
            case TokenType::KwWith: return parseWith();
            case TokenType::KwReturn: return parseReturn();
            case TokenType::KwBreak: {
                auto node = std::make_unique<ast::BreakStmt>();
                node->span = advance().span;
                expectStatementEnd();
                return node;
            }
            case TokenType::KwContinue: {
                auto node = std::make_unique<ast::ContinueStmt>();
                node->span = advance().span;
                expectStatementEnd();
                return node;
            }
            default: break;
        }

        SourceSpan span = peek().span;
        auto expr = parseExpr();
        if (at(TokenType::Assign)) {
            advance();
            auto value = parseExpr();
            endSimpleStatement();
            auto node = std::make_unique<ast::AssignStmt>();
            node->span = span;
            node->target = std::move(expr);
            node->value = std::move(value);
            return node;
        }
        endSimpleStatement();
        auto node = std::make_unique<ast::ExprStmt>();
        node->span = span;
        node->expr = std::move(expr);
        return node;
    }

    ast::StmtPtr parseTry() {
        auto node = std::make_unique<ast::TryStmt>();
        node->span = advance().span;  // 'try'
        node->body = parseBlock();
        while (at(TokenType::KwExcept)) {
            advance();
            ast::ExceptClause clause;
            if (!at(TokenType::Colon) && !at(TokenType::KwAs)) clause.type = parseExpr();
            if (at(TokenType::KwAs)) {
                advance();
                clause.name = expect(TokenType::Identifier, "a name after 'as'").text;
            }
            clause.body = parseBlock();
            node->handlers.push_back(std::move(clause));
        }
        if (at(TokenType::KwFinally)) {
            advance();
            node->hasFinally = true;
            node->finallyBody = parseBlock();
        }
        if (node->handlers.empty() && !node->hasFinally)
            throw KiritoError("'try' needs at least one 'except' or a 'finally'", node->span);
        return node;
    }

    ast::StmtPtr parseClass() {
        auto node = std::make_unique<ast::ClassStmt>();
        node->span = advance().span;  // 'class'
        node->name = expect(TokenType::Identifier, "a class name").text;
        if (at(TokenType::LParen)) {
            advance();
            node->base = parseExpr();
            expect(TokenType::RParen, "')' after base class");
        }
        node->body = parseBlock();
        return node;
    }

    ast::StmtPtr parseWith() {
        auto node = std::make_unique<ast::WithStmt>();
        node->span = advance().span;  // 'with'
        node->context = parseExpr();
        if (at(TokenType::KwAs)) {
            advance();
            node->name = expect(TokenType::Identifier, "a name after 'as'").text;
        }
        node->body = parseBlock();
        return node;
    }

    ast::StmtPtr parseRaise() {
        auto node = std::make_unique<ast::RaiseStmt>();
        node->span = advance().span;  // 'raise'
        node->value = parseExpr();
        endSimpleStatement();
        return node;
    }

    ast::StmtPtr parseReturn() {
        auto node = std::make_unique<ast::ReturnStmt>();
        node->span = advance().span;  // 'return'
        if (!at(TokenType::Newline) && !at(TokenType::EndOfFile))
            node->value = parseExpr();
        endSimpleStatement();
        return node;
    }

    ast::StmtPtr parseVarDecl() {
        SourceSpan span = advance().span;  // 'var'
        const Token& name = expect(TokenType::Identifier, "a name after 'var'");
        expect(TokenType::Assign, "'=' in var declaration");
        auto init = parseExpr();
        endSimpleStatement();
        auto node = std::make_unique<ast::VarDeclStmt>();
        node->span = span;
        node->name = name.text;
        node->init = std::move(init);
        return node;
    }

    ast::Block parseBlock() {
        expect(TokenType::Colon, "':'");
        expect(TokenType::Newline, "a newline before an indented block");
        expect(TokenType::Indent, "an indented block");
        ast::Block body;
        while (!at(TokenType::Dedent) && !at(TokenType::EndOfFile)) {
            if (at(TokenType::Newline)) { advance(); continue; }
            body.push_back(parseStatement());
        }
        expect(TokenType::Dedent, "a dedent to close the block");
        blockJustClosed_ = true;  // the dedent self-terminates an enclosing simple statement
        return body;
    }

    ast::StmtPtr parseIf() {
        auto node = std::make_unique<ast::IfStmt>();
        node->span = advance().span;  // 'if'
        {
            auto cond = parseExpr();
            auto body = parseBlock();
            node->branches.emplace_back(std::move(cond), std::move(body));
        }
        while (at(TokenType::KwElif)) {
            advance();
            auto cond = parseExpr();
            auto body = parseBlock();
            node->branches.emplace_back(std::move(cond), std::move(body));
        }
        if (at(TokenType::KwElse)) {
            advance();
            node->orelse = parseBlock();
        }
        return node;
    }

    ast::StmtPtr parseWhile() {
        auto node = std::make_unique<ast::WhileStmt>();
        node->span = advance().span;  // 'while'
        node->cond = parseExpr();
        node->body = parseBlock();
        return node;
    }

    ast::StmtPtr parseFor() {
        auto node = std::make_unique<ast::ForStmt>();
        node->span = advance().span;  // 'for'
        node->var = expect(TokenType::Identifier, "a loop variable").text;
        expect(TokenType::KwIn, "'in'");
        node->iterable = parseExpr();
        node->body = parseBlock();
        return node;
    }

    ast::ExprPtr parseExpr() { return parseOr(); }

    ast::ExprPtr parseOr() {
        auto left = parseAnd();
        while (at(TokenType::KwOr)) {
            SourceSpan span = advance().span;
            left = logical(std::move(left), /*isAnd=*/false, parseAnd(), span);
        }
        return left;
    }

    ast::ExprPtr parseAnd() {
        auto left = parseNot();
        while (at(TokenType::KwAnd)) {
            SourceSpan span = advance().span;
            left = logical(std::move(left), /*isAnd=*/true, parseNot(), span);
        }
        return left;
    }

    ast::ExprPtr parseNot() {
        if (at(TokenType::KwNot)) {
            SourceSpan span = advance().span;
            auto node = std::make_unique<ast::UnaryExpr>();
            node->span = span;
            node->op = UnOp::Not;
            node->operand = parseNot();
            return node;
        }
        return parseComparison();
    }

    ast::ExprPtr parseComparison() {
        auto left = parseAdd();
        while (true) {
            BinOp op;
            switch (peek().type) {
                case TokenType::EqEq: op = BinOp::Eq; break;
                case TokenType::NotEq: op = BinOp::Ne; break;
                case TokenType::Lt: op = BinOp::Lt; break;
                case TokenType::Le: op = BinOp::Le; break;
                case TokenType::Gt: op = BinOp::Gt; break;
                case TokenType::Ge: op = BinOp::Ge; break;
                default: return left;
            }
            SourceSpan span = advance().span;
            left = binary(std::move(left), op, parseAdd(), span);
        }
    }

    ast::ExprPtr parseAdd() {
        auto left = parseMul();
        while (at(TokenType::Plus) || at(TokenType::Minus)) {
            BinOp op = at(TokenType::Plus) ? BinOp::Add : BinOp::Sub;
            SourceSpan span = advance().span;
            left = binary(std::move(left), op, parseMul(), span);
        }
        return left;
    }

    ast::ExprPtr parseMul() {
        auto left = parseUnary();
        while (at(TokenType::Star) || at(TokenType::Slash) ||
               at(TokenType::SlashSlash) || at(TokenType::Percent)) {
            BinOp op = at(TokenType::Star)       ? BinOp::Mul
                     : at(TokenType::Slash)      ? BinOp::Div
                     : at(TokenType::SlashSlash) ? BinOp::FloorDiv
                                                 : BinOp::Mod;
            SourceSpan span = advance().span;
            left = binary(std::move(left), op, parseUnary(), span);
        }
        return left;
    }

    ast::ExprPtr parseUnary() {
        if (at(TokenType::Minus)) {
            SourceSpan span = advance().span;
            auto node = std::make_unique<ast::UnaryExpr>();
            node->span = span;
            node->op = UnOp::Neg;
            node->operand = parseUnary();
            return node;
        }
        return parsePow();
    }

    ast::ExprPtr parsePow() {
        auto base = parsePostfix();
        if (at(TokenType::StarStar)) {
            SourceSpan span = advance().span;
            // Right-associative, and the exponent may itself be unary (2 ** -1).
            return binary(std::move(base), BinOp::Pow, parseUnary(), span);
        }
        return base;
    }

    // Postfix (call, index, member) binds tighter than **: f()**2 == (f())**2.
    ast::ExprPtr parsePostfix() {
        auto expr = parsePrimary();
        while (true) {
            if (at(TokenType::LParen)) {
                SourceSpan span = advance().span;
                auto call = std::make_unique<ast::CallExpr>();
                call->span = span;
                call->callee = std::move(expr);
                if (!at(TokenType::RParen)) {
                    call->args.push_back(parseExpr());
                    while (at(TokenType::Comma)) { advance(); call->args.push_back(parseExpr()); }
                }
                expect(TokenType::RParen, "')' to close the call");
                expr = std::move(call);
            } else if (at(TokenType::LBracket)) {
                SourceSpan span = advance().span;
                auto node = std::make_unique<ast::IndexExpr>();
                node->span = span;
                node->object = std::move(expr);
                node->index = parseExpr();
                expect(TokenType::RBracket, "']' to close the index");
                expr = std::move(node);
            } else if (at(TokenType::Dot)) {
                SourceSpan span = advance().span;
                auto node = std::make_unique<ast::MemberExpr>();
                node->span = span;
                node->object = std::move(expr);
                node->name = expect(TokenType::Identifier, "a member name after '.'").text;
                expr = std::move(node);
            } else {
                break;
            }
        }
        return expr;
    }

    ast::ExprPtr parseFunction() {
        auto node = std::make_unique<ast::FunctionExpr>();
        node->span = advance().span;  // 'Function'
        expect(TokenType::LParen, "'(' after Function");
        if (!at(TokenType::RParen)) {
            node->params.push_back(expect(TokenType::Identifier, "a parameter name").text);
            while (at(TokenType::Comma)) {
                advance();
                node->params.push_back(expect(TokenType::Identifier, "a parameter name").text);
            }
        }
        expect(TokenType::RParen, "')' after parameters");
        node->body = parseBlock();
        return node;
    }

    ast::ExprPtr parsePrimary() {
        const Token& t = peek();
        switch (t.type) {
            case TokenType::Integer: {
                advance();
                return literal(static_cast<int64_t>(std::stoll(t.text)), t.span);
            }
            case TokenType::Float: {
                advance();
                return literal(std::stod(t.text), t.span);
            }
            case TokenType::String: {
                advance();
                return literal(t.text, t.span);
            }
            case TokenType::KwTrue: advance(); return literal(true, t.span);
            case TokenType::KwFalse: advance(); return literal(false, t.span);
            case TokenType::KwNone: advance(); return literal(std::monostate{}, t.span);
            case TokenType::KwFunction: return parseFunction();
            case TokenType::Identifier: {
                advance();
                auto node = std::make_unique<ast::NameExpr>();
                node->span = t.span;
                node->name = t.text;
                return node;
            }
            case TokenType::LParen: {
                advance();
                auto e = parseExpr();
                expect(TokenType::RParen, "')'");
                return e;
            }
            case TokenType::LBracket: return parseListLiteral();
            case TokenType::LBrace: return parseBraceLiteral();
            default:
                throw KiritoError("expected an expression", t.span);
        }
    }

    ast::ExprPtr parseListLiteral() {
        auto node = std::make_unique<ast::ListLiteral>();
        node->span = advance().span;  // '['
        while (!at(TokenType::RBracket)) {
            node->elems.push_back(parseExpr());
            if (!at(TokenType::Comma)) break;
            advance();
        }
        expect(TokenType::RBracket, "']' to close the list");
        return node;
    }

    // `{}` and `{k: v, ...}` are Dicts; `{a, b, ...}` is a Set.
    ast::ExprPtr parseBraceLiteral() {
        SourceSpan span = advance().span;  // '{'
        if (at(TokenType::RBrace)) {
            advance();
            auto dict = std::make_unique<ast::DictLiteral>();
            dict->span = span;
            return dict;
        }
        auto first = parseExpr();
        if (at(TokenType::Colon)) {
            auto dict = std::make_unique<ast::DictLiteral>();
            dict->span = span;
            advance();
            dict->entries.emplace_back(std::move(first), parseExpr());
            while (at(TokenType::Comma)) {
                advance();
                if (at(TokenType::RBrace)) break;
                auto key = parseExpr();
                expect(TokenType::Colon, "':' in dict entry");
                dict->entries.emplace_back(std::move(key), parseExpr());
            }
            expect(TokenType::RBrace, "'}' to close the dict");
            return dict;
        }
        auto set = std::make_unique<ast::SetLiteral>();
        set->span = span;
        set->elems.push_back(std::move(first));
        while (at(TokenType::Comma)) {
            advance();
            if (at(TokenType::RBrace)) break;
            set->elems.push_back(parseExpr());
        }
        expect(TokenType::RBrace, "'}' to close the set");
        return set;
    }

    template <typename T>
    static ast::ExprPtr literal(T value, SourceSpan span) {
        auto node = std::make_unique<ast::LiteralExpr>();
        node->span = span;
        node->value = std::move(value);
        return node;
    }

    static ast::ExprPtr binary(ast::ExprPtr lhs, BinOp op, ast::ExprPtr rhs, SourceSpan span) {
        auto node = std::make_unique<ast::BinaryExpr>();
        node->span = span;
        node->op = op;
        node->lhs = std::move(lhs);
        node->rhs = std::move(rhs);
        return node;
    }

    static ast::ExprPtr logical(ast::ExprPtr lhs, bool isAnd, ast::ExprPtr rhs, SourceSpan span) {
        auto node = std::make_unique<ast::LogicalExpr>();
        node->span = span;
        node->isAnd = isAnd;
        node->lhs = std::move(lhs);
        node->rhs = std::move(rhs);
        return node;
    }

    std::vector<Token> toks_;
    size_t pos_ = 0;
    bool blockJustClosed_ = false;
};

}  // namespace kirito

#endif
