#ifndef KIRITO_CLASS_VALUE_HPP
#define KIRITO_CLASS_VALUE_HPP

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "arena.hpp"
#include "object.hpp"

namespace kirito {

// A user-defined class: a bag of methods (Functions whose first param is the receiver) plus an
// optional base class. Calling it constructs an instance and runs its `init` method. A class is a
// first-class value living in the same model as built-ins — exactly the unified-object goal.
class ClassValue : public Object {
public:
    std::string name;
    std::unordered_map<std::string, Handle> methods;
    Handle base{};
    bool hasBase = false;
    Handle selfHandle{};  // the class's own arena handle (set by the evaluator after allocation)

    ValueKind kind() const override { return ValueKind::Class; }
    std::string typeName() const override { return name; }
    bool truthy() const override { return true; }
    std::string str(StringifyCtx&) const override { return "<class " + name + ">"; }
    bool equals(const ObjectArena&, const Object& other) const override { return this == &other; }
    void children(std::vector<Handle>& out) const override {
        for (const auto& [k, v] : methods) out.push_back(v);
        if (hasBase) out.push_back(base);
    }

    // Method resolution walks the base chain.
    const Handle* findMethod(const ObjectArena& arena, const std::string& n) const {
        auto it = methods.find(n);
        if (it != methods.end()) return &it->second;
        if (hasBase) return static_cast<const ClassValue&>(arena.deref(base)).findMethod(arena, n);
        return nullptr;
    }

    Handle call(KiritoVM&, std::span<const Handle> args) override;  // instantiate (runtime.hpp)
};

// An instance of a user class: its own attribute table plus a handle to its class for method lookup.
class InstanceValue : public Object {
public:
    Handle cls{};
    std::string className;  // copied from the class so typeName()/str() need no arena
    std::unordered_map<std::string, Handle> attrs;

    ValueKind kind() const override { return ValueKind::Instance; }
    std::string typeName() const override { return className; }
    bool truthy() const override { return true; }
    std::string str(StringifyCtx&) const override { return "<" + className + " object>"; }
    bool equals(const ObjectArena&, const Object& other) const override { return this == &other; }
    void children(std::vector<Handle>& out) const override {
        out.push_back(cls);
        for (const auto& [k, v] : attrs) out.push_back(v);
    }
    void setAttr(KiritoVM&, std::string_view name, Handle value) override {
        attrs[std::string(name)] = value;
    }
    Handle getAttr(KiritoVM&, Handle self, std::string_view name) override;  // runtime.hpp
};

}  // namespace kirito

#endif
