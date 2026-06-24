#ifndef KIRITO_COMMON_HPP
#define KIRITO_COMMON_HPP

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

// AddressSanitizer/ThreadSanitizer detection (GCC defines __SANITIZE_ADDRESS__; Clang exposes
// __has_feature). Sanitizer builds use much larger native frames (redzones + shadow), so recursive
// descents (the parser, the compiler) overflow the stack at a far shallower depth — the depth guards
// below pick a lower bound under a sanitizer so they raise a clean error instead of crashing. Defined
// here, in the first-included header, so every translation unit (parser, vm, ...) sees it.
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)  // GCC: asan / tsan
#  define KIRITO_SANITIZER_BUILD 1
#elif defined(__has_feature)                                       // Clang
#  if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#    define KIRITO_SANITIZER_BUILD 1
#  endif
#endif

namespace kirito {

// Position of a token / node in source, for diagnostics. 1-based line/col.
struct SourceSpan {
    uint32_t line = 0;
    uint32_t col = 0;
    uint32_t length = 0;
};

// Shared operator vocabulary used by the AST, the value protocol, and the evaluator alike, so
// none of them needs to know the others' enums.
enum class BinOp { Add, Sub, Mul, Div, FloorDiv, Mod, Pow, Eq, Ne, Lt, Le, Gt, Ge, In, NotIn };
enum class UnOp { Neg, Not };

// Every Kirito error (lexing, parsing, runtime) carries the span it occurred at so
// users get an actionable line:col. A zero span means "no specific location".
class KiritoError : public std::runtime_error {
public:
    explicit KiritoError(std::string message, SourceSpan sp = {})
        : std::runtime_error(std::move(message)), span(sp) {}

    SourceSpan span;
    // The source file the error occurred in. Empty until a chunk/module loader tags it (the span's
    // line:col are meaningless without knowing *which* file), so a parse error inside an imported
    // module reports the module's path, not the entry script's.
    std::string file;
};

// Parse a double from `s` without std::stod's underflow trap: std::stod throws std::out_of_range
// when a value underflows to a subnormal/zero (errno==ERANGE), which would crash the lexer and the
// serializers on a perfectly representable tiny literal like `5e-324`. std::strtod instead RETURNS
// the (subnormal/zero) value and merely sets errno, so we accept underflow and only reject a
// genuine non-parse (no digits consumed) or a true overflow to ±inf. `consumed`, if non-null,
// receives the number of characters parsed (like std::stod's `pos`), for trailing-garbage checks.
inline double parseDouble(const std::string& s, std::size_t* consumed = nullptr) {
    const char* begin = s.c_str();
    char* end = nullptr;
    errno = 0;
    double v = std::strtod(begin, &end);
    if (end == begin) throw std::invalid_argument("parseDouble: no conversion");
    if (errno == ERANGE && (v == HUGE_VAL || v == -HUGE_VAL))
        throw std::out_of_range("parseDouble: overflow");   // ±inf — genuine out-of-range
    if (consumed) *consumed = static_cast<std::size_t>(end - begin);
    return v;   // underflow (subnormal/zero) is accepted, not thrown
}

}  // namespace kirito

#endif
