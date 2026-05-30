#ifndef KIRITO_PARSER_HPP
#define KIRITO_PARSER_HPP

#include <memory>
#include <string>
#include <vector>

#include "ast.hpp"
#include "common.hpp"
#include "lexer.hpp"

namespace kirito {

// Recursive descent: statements at the top, expressions by precedence level
// (add < mul < unary < pow < primary), with ** right-associative — so -2**2 == -4 and
// 2**3**2 == 512, matching Python.
class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : toks_(std::move(tokens)) {}

    ast::Program parseProgram() {
        ast::Program prog;
        while (!at(TokenType::EndOfFile)) {
            if (at(TokenType::Newline)) { advance(); continue; }
            auto stmt = std::make_unique<ast::ExprStmt>();
            stmt->span = peek().span;
            stmt->expr = parseAdd();
            expectStatementEnd();
            prog.stmts.push_back(std::move(stmt));
        }
        return prog;
    }

private:
    const Token& peek() const { return toks_[pos_]; }
    bool at(TokenType t) const { return peek().type == t; }
    const Token& advance() { return toks_[pos_++]; }

    void expectStatementEnd() {
        if (at(TokenType::Newline)) { advance(); return; }
        if (at(TokenType::EndOfFile)) return;
        throw KiritoError("expected end of statement", peek().span);
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
        if (at(TokenType::Integer)) {
            const Token& t = advance();
            auto node = std::make_unique<ast::LiteralExpr>();
            node->span = t.span;
            node->value = static_cast<int64_t>(std::stoll(t.text));
            return node;
        }
        if (at(TokenType::Float)) {
            const Token& t = advance();
            auto node = std::make_unique<ast::LiteralExpr>();
            node->span = t.span;
            node->value = std::stod(t.text);
            return node;
        }
        if (at(TokenType::LParen)) {
            advance();
            auto e = parseAdd();
            if (!at(TokenType::RParen)) throw KiritoError("expected ')'", peek().span);
            advance();
            return e;
        }
        throw KiritoError("expected an expression", peek().span);
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
