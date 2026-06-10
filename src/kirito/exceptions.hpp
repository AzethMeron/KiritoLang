#ifndef KIRITO_EXCEPTIONS_HPP
#define KIRITO_EXCEPTIONS_HPP

#include "common.hpp"
#include "handle.hpp"

namespace kirito {

// Thrown by `throw` to unwind the C++ stack carrying a Kirito value (the exception object).
// Distinct from KiritoError, which is an internal/runtime diagnostic; a Kirito `try` catches both
// (internal errors are surfaced to Kirito code as an Error value). `span` records the throw/assert
// site so an uncaught exception can report file:line:col.
struct KiritoThrow {
    KiritoThrow(Handle v, SourceSpan s = {}) : value(v), span(s) {}
    Handle value;
    SourceSpan span{};
    std::string file;  // defining chunk of the function that threw (set on escape; "" = entry chunk)
};

}  // namespace kirito

#endif
