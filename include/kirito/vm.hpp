#ifndef KIRITO_VM_HPP
#define KIRITO_VM_HPP

#include <memory>
#include <string>

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

    std::string stringify(Handle h) const {
        StringifyCtx ctx{arena_, {}};
        return arena_.deref(h).str(ctx);
    }

private:
    ObjectArena arena_;
    Handle none_;
};

}  // namespace kirito

#endif
