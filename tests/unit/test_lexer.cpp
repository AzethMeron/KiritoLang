#include <vector>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

int main() {
    {
        Lexer lex("42");
        auto toks = lex.tokenize();
        // Integer, Newline, EndOfFile
        CHECK(toks.size() == 3);
        CHECK(toks[0].type == TokenType::Integer);
        CHECK(toks[0].text == "42");
        CHECK(toks[1].type == TokenType::Newline);
        CHECK(toks[2].type == TokenType::EndOfFile);
    }
    {
        Lexer lex("3.5 + 1");
        auto toks = lex.tokenize();
        CHECK(toks[0].type == TokenType::Float);
        CHECK(toks[0].text == "3.5");
        CHECK(toks[1].type == TokenType::Plus);
        CHECK(toks[2].type == TokenType::Integer);
    }
    {
        // operators, comments skipped, line/col tracked
        Lexer lex("1 // 2 ** 3  # comment\n");
        auto toks = lex.tokenize();
        CHECK(toks[0].type == TokenType::Integer);
        CHECK(toks[1].type == TokenType::SlashSlash);
        CHECK(toks[2].type == TokenType::Integer);
        CHECK(toks[3].type == TokenType::StarStar);
        CHECK(toks[4].type == TokenType::Integer);
        CHECK(toks[5].type == TokenType::Newline);
        CHECK(toks[0].span.line == 1);
        CHECK(toks[0].span.col == 1);
    }
    {
        // unexpected character reports a location
        Lexer lex("1 $ 2");
        CHECK_THROWS(lex.tokenize());
    }
    return RUN_TESTS();
}
