#ifndef KIRITO_VM_HPP
#define KIRITO_VM_HPP

#include <memory>
#include <string>
#include <string_view>

#include "arena.hpp"
#include "builtins.hpp"
#include "handle.hpp"
#include "object.hpp"

namespace kirito {

// One KiritoVM == one fully-encapsulated Kirito process. It owns the whole process state by
// composing its sub-objects (the value arena now; global environment and modules as the
// language grows). No global/static mutable state, so independent VMs never interfere.
class KiritoVM {
public:
    KiritoVM() {
        none_ = arena_.alloc(std::make_unique<NoneVal>());
    }

    ObjectArena& arena() { return arena_; }
    const ObjectArena& arena() const { return arena_; }

    // Interned singleton — every None is this one slot.
    Handle none() const { return none_; }

    // Value-construction helpers, also the embedding surface for C++ callers.
    Handle makeInt(int64_t v) { return arena_.alloc(std::make_unique<IntVal>(v)); }
    Handle makeFloat(double v) { return arena_.alloc(std::make_unique<FloatVal>(v)); }

    std::string stringify(Handle h) const {
        StringifyCtx ctx{arena_, {}};
        return arena_.deref(h).str(ctx);
    }

    // Lex, parse, and evaluate a chunk of Kirito source; returns the handle of the last
    // expression's value (or None). Defined in runtime.hpp once the front end is visible.
    Handle runSource(std::string_view source, std::string_view chunkName = "<main>");

private:
    ObjectArena arena_;
    Handle none_;
};

}  // namespace kirito

#endif
