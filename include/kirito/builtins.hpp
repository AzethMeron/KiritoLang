#ifndef KIRITO_BUILTINS_HPP
#define KIRITO_BUILTINS_HPP

#include <string>

#include "arena.hpp"
#include "object.hpp"

namespace kirito {

// The unit value. Interned once per VM, so every `None` shares one arena slot.
class NoneVal : public Object {
public:
    ValueKind kind() const override { return ValueKind::None; }
    std::string typeName() const override { return "None"; }
    bool truthy() const override { return false; }
    std::string str(StringifyCtx&) const override { return "None"; }
    bool equals(const ObjectArena&, const Object& other) const override {
        return other.kind() == ValueKind::None;
    }
    bool hashable() const override { return true; }
    std::size_t hash() const override { return 0; }
};

}  // namespace kirito

#endif
