#ifndef KIRITO_EXCEPTIONS_HPP
#define KIRITO_EXCEPTIONS_HPP

#include "handle.hpp"

namespace kirito {

// Thrown by `throw` to unwind the C++ stack carrying a Kirito value (the exception object).
// Distinct from KiritoError, which is an internal/runtime diagnostic; a Kirito `try` catches both
// (internal errors are surfaced to Kirito code as an Error value).
struct KiritoThrow {
    Handle value;
};

}  // namespace kirito

#endif
