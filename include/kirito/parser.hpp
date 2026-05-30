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

    ast::StmtPtr parseStatement() {
        if (at(TokenType::KwVar)) return parseVarDecl();

        SourceSpan span = peek().span;
        auto expr = parseExpr();
        if (at(TokenType::Assign)) {
            advance();
            auto value = parseExpr();
            expectStatementEnd();
            auto node = std::make_unique<ast::AssignStmt>();
            node->span = span;
            node->target = std::move(expr);
            node->value = std::move(value);
            return node;
        }
        expectStatementEnd();
        auto node = std::make_unique<ast::ExprStmt>();
        node->span = span;
        node->expr = std::move(expr);
        return node;
    }

    ast::StmtPtr parseVarDecl() {
        SourceSpan span = advance().span;  // 'var'
        const Token& name = expect(TokenType::Identifier, "a name after 'var'");
        expect(TokenType::Assign, "'=' in var declaration");
        auto init = parseExpr();
        expectStatementEnd();
        auto node = std::make_unique<ast::VarDeclStmt>();
        node->span = span;
        node->name = name.text;
        node->init = std::move(init);
        return node;
    }

    ast::ExprPtr parseExpr() { return parseComparison(); }

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
        auto base = parsePrimary();
        if (at(TokenType::StarStar)) {
            SourceSpan span = advance().span;
            // Right-associative, and the exponent may itself be unary (2 ** -1).
            return binary(std::move(base), BinOp::Pow, parseUnary(), span);
        }
        return base;
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
            default:
                throw KiritoError("expected an expression", t.span);
        }
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

    std::vector<Token> toks_;
    size_t pos_ = 0;
};

}  // namespace kirito

#endif
