#ifndef KIRITO_LEXER_HPP
#define KIRITO_LEXER_HPP

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "common.hpp"

namespace kirito {

enum class TokenType {
    Integer, Float, String, FString, Identifier,
    KwVar, KwTrue, KwFalse, KwNone,
    KwIf, KwElif, KwElse, KwWhile, KwBreak, KwContinue,
    KwAnd, KwOr, KwNot, KwFunction, KwReturn, KwFor, KwIn,
    KwTry, KwCatch, KwFinally, KwThrow, KwAs, KwClass, KwWith, KwPass, KwAssert, KwDiscard,
    Plus, Minus, Star, Slash, SlashSlash, Percent, StarStar, Arrow,
    Assign, EqEq, NotEq, Lt, Le, Gt, Ge,
    LParen, RParen, LBracket, RBracket, LBrace, RBrace,
    Colon, Comma, Dot,
    Newline, Indent, Dedent, EndOfFile,
};

struct Token {
    TokenType type;
    std::string text;  // literal text for numbers; empty otherwise
    SourceSpan span;
};

// Turns source text into a flat token stream. Tracks 1-based line/col so every token (and any
// error) can point at an exact location. Indentation handling arrives in a later milestone.
class Lexer {
public:
    explicit Lexer(std::string_view source) : src_(source) {}

    std::vector<Token> tokenize() {
        std::vector<Token> out;
        indent_ = {{0, 0}};
        bool lineStart = true;
        while (pos_ < src_.size()) {
            if (lineStart && parenDepth_ == 0) {
                if (!handleIndentation(out)) continue;  // blank/comment line: nothing emitted
                lineStart = false;
                continue;
            }
            char c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\r') {
                advance();
            } else if (c == '#') {
                while (pos_ < src_.size() && src_[pos_] != '\n') advance();
            } else if (c == '\n') {
                if (parenDepth_ == 0) {
                    out.push_back(make(TokenType::Newline, line_, col_));
                    advance();
                    lineStart = true;
                } else {
                    advance();  // newline is insignificant inside (), line continuation
                }
            } else if (std::isdigit(static_cast<unsigned char>(c))) {
                out.push_back(number());
            } else if (c == '"') {
                out.push_back(string());
            } else if (c == 'f' && peek(1) == '"') {
                out.push_back(fstring());
            } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                out.push_back(identifier());
            } else {
                out.push_back(op());
            }
        }
        // Close the final logical line, then unwind any open indentation, then EOF.
        if (!out.empty() && out.back().type != TokenType::Newline)
            out.push_back(make(TokenType::Newline, line_, col_));
        while (indent_.size() > 1) {
            indent_.pop_back();
            out.push_back(make(TokenType::Dedent, line_, col_));
        }
        out.push_back(make(TokenType::EndOfFile, line_, col_));
        return out;
    }

private:
    void advance() {
        if (src_[pos_] == '\n') { ++line_; col_ = 1; } else { ++col_; }
        ++pos_;
    }
    char peek(size_t ahead = 0) const {
        size_t i = pos_ + ahead;
        return i < src_.size() ? src_[i] : '\0';
    }
    Token make(TokenType t, uint32_t line, uint32_t col, std::string text = {}) const {
        return Token{t, std::move(text), SourceSpan{line, col, 0}};
    }

    // At a logical line start: measure indentation and emit Indent / Dedent(s). Blank lines and
    // comment-only lines yield no layout tokens (returns false after consuming the line).
    //
    // Tabs and spaces are both allowed, but indentation must be UNAMBIGUOUS: every indent is
    // measured two ways — tabs as 8 columns ("wide") and tabs as 1 column ("narrow") — and the two
    // measures must agree on the relation (deeper/same/shallower) to each enclosing level. This is
    // Python 3's rule and rejects e.g. a tab where the surrounding block used 8 spaces.
    bool handleIndentation(std::vector<Token>& out) {
        int wide = 0, narrow = 0;
        size_t scan = pos_;
        while (scan < src_.size()) {
            char c = src_[scan];
            if (c == ' ') { ++wide; ++narrow; ++scan; }
            else if (c == '\t') { wide += 8 - (wide % 8); ++narrow; ++scan; }
            else break;
        }
        // Blank or comment-only line: skip to (and over) the newline, emit nothing.
        if (scan >= src_.size() || src_[scan] == '\n' || src_[scan] == '#') {
            while (pos_ < src_.size() && src_[pos_] != '\n') advance();
            if (pos_ < src_.size()) advance();
            return false;
        }
        while (pos_ < scan) advance();  // step over the leading whitespace

        auto ambiguous = [&] {
            throw KiritoError("inconsistent use of tabs and spaces in indentation",
                              SourceSpan{line_, 1, 0});
        };
        Indent cur{wide, narrow};
        const Indent top = indent_.back();
        if (cur.wide == top.wide) {
            if (cur.narrow != top.narrow) ambiguous();  // same wide width, different tab usage
        } else if (cur.wide > top.wide) {
            if (cur.narrow <= top.narrow) ambiguous();
            indent_.push_back(cur);
            out.push_back(make(TokenType::Indent, line_, col_));
        } else {
            if (cur.narrow >= top.narrow) ambiguous();
            while (cur.wide < indent_.back().wide) {
                indent_.pop_back();
                out.push_back(make(TokenType::Dedent, line_, col_));
            }
            if (cur.wide != indent_.back().wide) throw KiritoError("inconsistent dedent", SourceSpan{line_, 1, 0});
            if (cur.narrow != indent_.back().narrow) ambiguous();
        }
        return true;
    }

    Token number() {
        uint32_t line = line_, col = col_;
        std::string text;
        bool isFloat = false;
        while (std::isdigit(static_cast<unsigned char>(peek()))) { text += peek(); advance(); }
        if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
            isFloat = true;
            text += peek(); advance();
            while (std::isdigit(static_cast<unsigned char>(peek()))) { text += peek(); advance(); }
        }
        return make(isFloat ? TokenType::Float : TokenType::Integer, line, col, std::move(text));
    }

    Token identifier() {
        uint32_t line = line_, col = col_;
        std::string text;
        while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
            text += peek();
            advance();
        }
        TokenType type = TokenType::Identifier;
        if (text == "var") type = TokenType::KwVar;
        else if (text == "True") type = TokenType::KwTrue;
        else if (text == "False") type = TokenType::KwFalse;
        else if (text == "None") type = TokenType::KwNone;
        else if (text == "if") type = TokenType::KwIf;
        else if (text == "elif") type = TokenType::KwElif;
        else if (text == "else") type = TokenType::KwElse;
        else if (text == "while") type = TokenType::KwWhile;
        else if (text == "break") type = TokenType::KwBreak;
        else if (text == "continue") type = TokenType::KwContinue;
        else if (text == "and") type = TokenType::KwAnd;
        else if (text == "or") type = TokenType::KwOr;
        else if (text == "not") type = TokenType::KwNot;
        else if (text == "Function") type = TokenType::KwFunction;
        else if (text == "return") type = TokenType::KwReturn;
        else if (text == "for") type = TokenType::KwFor;
        else if (text == "in") type = TokenType::KwIn;
        else if (text == "try") type = TokenType::KwTry;
        else if (text == "catch") type = TokenType::KwCatch;
        else if (text == "finally") type = TokenType::KwFinally;
        else if (text == "throw") type = TokenType::KwThrow;
        else if (text == "as") type = TokenType::KwAs;
        else if (text == "class") type = TokenType::KwClass;
        else if (text == "with") type = TokenType::KwWith;
        else if (text == "pass") type = TokenType::KwPass;
        else if (text == "assert") type = TokenType::KwAssert;
        else if (text == "discard") type = TokenType::KwDiscard;
        return make(type, line, col, std::move(text));
    }

    Token string() {
        uint32_t line = line_, col = col_;
        advance();  // opening quote
        std::string text;
        while (peek() != '"') {
            char c = peek();
            if (c == '\0' || c == '\n')
                throw KiritoError("unterminated string", SourceSpan{line, col, 1});
            if (c == '\\') {
                advance();
                char e = peek();
                if (e == 'x') {
                    // \xHH — a byte from two hex digits.
                    advance();  // consume 'x'
                    auto hex = [&](char d) -> int {
                        if (d >= '0' && d <= '9') return d - '0';
                        if (d >= 'a' && d <= 'f') return d - 'a' + 10;
                        if (d >= 'A' && d <= 'F') return d - 'A' + 10;
                        return -1;
                    };
                    int hi = hex(peek());
                    if (hi < 0) throw KiritoError("invalid \\x escape (expected hex digit)", SourceSpan{line_, col_, 1});
                    advance();
                    int lo = hex(peek());
                    if (lo < 0) throw KiritoError("invalid \\x escape (expected hex digit)", SourceSpan{line_, col_, 1});
                    text += static_cast<char>(hi * 16 + lo);
                    advance();
                    continue;
                }
                switch (e) {
                    case 'n': text += '\n'; break;
                    case 't': text += '\t'; break;
                    case 'r': text += '\r'; break;
                    case '0': text += '\0'; break;
                    case '\\': text += '\\'; break;
                    case '"': text += '"'; break;
                    default:
                        throw KiritoError(std::string("invalid escape '\\") + e + "'",
                                          SourceSpan{line_, col_, 1});
                }
                advance();
            } else {
                text += c;
                advance();
            }
        }
        advance();  // closing quote
        return make(TokenType::String, line, col, std::move(text));
    }

    // f-string: keep the raw inner text (braces and escapes preserved); the parser splits it into
    // literal segments and embedded {expression} parts.
    Token fstring() {
        uint32_t line = line_, col = col_;
        advance();  // 'f'
        advance();  // opening quote
        std::string raw;
        while (peek() != '"') {
            char c = peek();
            if (c == '\0' || c == '\n')
                throw KiritoError("unterminated f-string", SourceSpan{line, col, 1});
            if (c == '\\') { raw += c; advance(); raw += peek(); advance(); }
            else { raw += c; advance(); }
        }
        advance();  // closing quote
        return make(TokenType::FString, line, col, std::move(raw));
    }

    Token op() {
        uint32_t line = line_, col = col_;
        char c = peek();
        switch (c) {
            case '+': advance(); return make(TokenType::Plus, line, col);
            case '-':
                advance();
                if (peek() == '>') { advance(); return make(TokenType::Arrow, line, col); }
                return make(TokenType::Minus, line, col);
            case '*':
                advance();
                if (peek() == '*') { advance(); return make(TokenType::StarStar, line, col); }
                return make(TokenType::Star, line, col);
            case '/':
                advance();
                if (peek() == '/') { advance(); return make(TokenType::SlashSlash, line, col); }
                return make(TokenType::Slash, line, col);
            case '%': advance(); return make(TokenType::Percent, line, col);
            case ':': advance(); return make(TokenType::Colon, line, col);
            case ',': advance(); return make(TokenType::Comma, line, col);
            case '.': advance(); return make(TokenType::Dot, line, col);
            case '(': advance(); ++parenDepth_; return make(TokenType::LParen, line, col);
            case ')':
                advance();
                if (parenDepth_ > 0) --parenDepth_;
                return make(TokenType::RParen, line, col);
            case '[': advance(); ++parenDepth_; return make(TokenType::LBracket, line, col);
            case ']':
                advance();
                if (parenDepth_ > 0) --parenDepth_;
                return make(TokenType::RBracket, line, col);
            case '{': advance(); ++parenDepth_; return make(TokenType::LBrace, line, col);
            case '}':
                advance();
                if (parenDepth_ > 0) --parenDepth_;
                return make(TokenType::RBrace, line, col);
            case '=':
                advance();
                if (peek() == '=') { advance(); return make(TokenType::EqEq, line, col); }
                return make(TokenType::Assign, line, col);
            case '!':
                advance();
                if (peek() == '=') { advance(); return make(TokenType::NotEq, line, col); }
                throw KiritoError("unexpected '!' (did you mean '!=' or 'not'?)",
                                  SourceSpan{line, col, 1});
            case '<':
                advance();
                if (peek() == '=') { advance(); return make(TokenType::Le, line, col); }
                return make(TokenType::Lt, line, col);
            case '>':
                advance();
                if (peek() == '=') { advance(); return make(TokenType::Ge, line, col); }
                return make(TokenType::Gt, line, col);
            default:
                throw KiritoError(std::string("unexpected character '") + c + "'",
                                  SourceSpan{line, col, 1});
        }
    }

    std::string_view src_;
    size_t pos_ = 0;
    uint32_t line_ = 1;
    uint32_t col_ = 1;
    struct Indent { int wide; int narrow; };  // tab-as-8 and tab-as-1 column measures
    std::vector<Indent> indent_{{0, 0}};
    int parenDepth_ = 0;
};

}  // namespace kirito

#endif
