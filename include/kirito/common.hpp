#ifndef KIRITO_COMMON_HPP
#define KIRITO_COMMON_HPP

#include <cstdint>
#include <stdexcept>
#include <string>

namespace kirito {

// Position of a token / node in source, for diagnostics. 1-based line/col.
struct SourceSpan {
    uint32_t line = 0;
    uint32_t col = 0;
    uint32_t length = 0;
};

// Shared operator vocabulary used by the AST, the value protocol, and the evaluator alike, so
// none of them needs to know the others' enums.
enum class BinOp { Add, Sub, Mul, Div, FloorDiv, Mod, Pow, Eq, Ne, Lt, Le, Gt, Ge };
enum class UnOp { Neg, Not };

// Every Kirito error (lexing, parsing, runtime) carries the span it occurred at so
// users get an actionable line:col. A zero span means "no specific location".
class KiritoError : public std::runtime_error {
public:
    explicit KiritoError(std::string message, SourceSpan span = {})
        : std::runtime_error(std::move(message)), span(span) {}

    SourceSpan span;
};

}  // namespace kirito

#endif
