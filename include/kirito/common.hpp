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
