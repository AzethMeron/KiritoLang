#ifndef KIRITO_ARENA_HPP
#define KIRITO_ARENA_HPP

#include <memory>
#include <utility>
#include <vector>

#include "common.hpp"
#include "handle.hpp"
#include "object.hpp"

namespace kirito {

// The single owner of every shared Kirito value. A slot's unique_ptr is the sole owner of its
// Object; everything else refers to it by Handle. Reclamation (deferred mark-sweep GC) frees
// slots and bumps their generation but never moves live objects, so handles stay stable.
class ObjectArena {
public:
    Handle alloc(std::unique_ptr<Object> obj) {
        if (!free_.empty()) {
            uint32_t slot = free_.back();
            free_.pop_back();
            slots_[slot].obj = std::move(obj);
            slots_[slot].occupied = true;
            return Handle{slot, slots_[slot].generation};
        }
        uint32_t slot = static_cast<uint32_t>(slots_.size());
        slots_.push_back(Slot{std::move(obj), 0, true, false});
        return Handle{slot, 0};
    }

    Object& deref(Handle h) { return *at(h).obj; }
    const Object& deref(Handle h) const { return *at(h).obj; }

private:
    struct Slot {
        std::unique_ptr<Object> obj;
        uint32_t generation = 0;
        bool occupied = false;
        bool marked = false;  // reserved for GC
    };

    const Slot& at(Handle h) const {
        if (h.slot >= slots_.size()) throw KiritoError("dangling handle (slot out of range)");
        const Slot& slot = slots_[h.slot];
        if (!slot.occupied || slot.generation != h.generation)
            throw KiritoError("dangling handle (stale generation)");
        return slot;
    }
    Slot& at(Handle h) { return const_cast<Slot&>(std::as_const(*this).at(h)); }

    std::vector<Slot> slots_;
    std::vector<uint32_t> free_;
};

}  // namespace kirito

#endif
