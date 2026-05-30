#ifndef KIRITO_HANDLE_HPP
#define KIRITO_HANDLE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>

namespace kirito {

// A reference to a value living in a KiritoVM's ObjectArena. Trivially copyable; the
// generation distinguishes a live slot from a reused one, so a stale handle is detectable.
// Handles are how reference-assignment semantics work: two bindings holding the same Handle
// alias the same value.
struct Handle {
    uint32_t slot = 0;
    uint32_t generation = 0;
    bool operator==(const Handle&) const = default;
};

}  // namespace kirito

template <>
struct std::hash<kirito::Handle> {
    std::size_t operator()(const kirito::Handle& h) const noexcept {
        return (static_cast<std::size_t>(h.generation) << 32) ^ h.slot;
    }
};

#endif
