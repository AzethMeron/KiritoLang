#ifndef KIRITO_OBJECT_HPP
#define KIRITO_OBJECT_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

#include "common.hpp"
#include "handle.hpp"

namespace kirito {

class ObjectArena;
class KiritoVM;

enum class ValueKind {
    None, Bool, Integer, Float, String,
    Array, List, Set, Dict,
    Function, NativeFunction, Module, Class, Instance,
};

enum class BinOp { Add, Sub, Mul, Div, FloorDiv, Mod, Pow, Eq, Ne, Lt, Le, Gt, Ge };
enum class UnOp { Neg, Not };

// Threaded through str() so containers can detect reference cycles (a value already being
// stringified) and emit an ellipsis instead of recursing forever.
struct StringifyCtx {
    const ObjectArena& arena;
    std::unordered_set<uint32_t> active;
};

// The one uniform protocol every value implements — built-in scalars, collections,
// C++-authored types, and (later) Kirito `class` instances. The evaluator only ever talks
// to this interface, so it cannot tell built-ins from user-defined objects apart.
//
// Operation slots (binary/unary/call/...) default to a clear "unsupported" error; concrete
// types override only what they support. Slots receive the KiritoVM so they can allocate
// results and dereference operands, and the caller's own Handle so they can reference self.
class Object {
public:
    virtual ~Object() = default;

    virtual ValueKind kind() const = 0;
    virtual std::string typeName() const = 0;

    virtual bool truthy() const = 0;
    virtual std::string str(StringifyCtx&) const = 0;
    virtual bool equals(const ObjectArena&, const Object& other) const = 0;

    virtual bool hashable() const { return false; }
    virtual std::size_t hash() const { throw KiritoError("unhashable type '" + typeName() + "'"); }

    // Enumerate contained handles for the (future) mark-sweep GC and for serialization.
    virtual void children(std::vector<Handle>&) const {}

    virtual Handle binary(KiritoVM&, BinOp, Handle self, Handle rhs);
    virtual Handle unary(KiritoVM&, UnOp, Handle self);
    virtual Handle call(KiritoVM&, std::span<const Handle> args);
    virtual Handle getAttr(KiritoVM&, std::string_view name);
    virtual void setAttr(KiritoVM&, std::string_view name, Handle value);
    virtual Handle getItem(KiritoVM&, Handle key);
    virtual void setItem(KiritoVM&, Handle key, Handle value);
    virtual std::optional<std::vector<Handle>> iterate(KiritoVM&);
    virtual std::optional<int64_t> length(KiritoVM&);
};

inline Handle Object::binary(KiritoVM&, BinOp, Handle, Handle) {
    throw KiritoError("type '" + typeName() + "' does not support this binary operator");
}
inline Handle Object::unary(KiritoVM&, UnOp, Handle) {
    throw KiritoError("type '" + typeName() + "' does not support this unary operator");
}
inline Handle Object::call(KiritoVM&, std::span<const Handle>) {
    throw KiritoError("type '" + typeName() + "' is not callable");
}
inline Handle Object::getAttr(KiritoVM&, std::string_view name) {
    throw KiritoError("type '" + typeName() + "' has no attribute '" + std::string(name) + "'");
}
inline void Object::setAttr(KiritoVM&, std::string_view name, Handle) {
    throw KiritoError("type '" + typeName() + "' has no attribute '" + std::string(name) + "'");
}
inline Handle Object::getItem(KiritoVM&, Handle) {
    throw KiritoError("type '" + typeName() + "' is not indexable");
}
inline void Object::setItem(KiritoVM&, Handle, Handle) {
    throw KiritoError("type '" + typeName() + "' does not support item assignment");
}
inline std::optional<std::vector<Handle>> Object::iterate(KiritoVM&) {
    throw KiritoError("type '" + typeName() + "' is not iterable");
}
inline std::optional<int64_t> Object::length(KiritoVM&) {
    throw KiritoError("type '" + typeName() + "' has no length");
}

}  // namespace kirito

#endif
