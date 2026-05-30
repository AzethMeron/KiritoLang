#ifndef KIRITO_ENVIRONMENT_HPP
#define KIRITO_ENVIRONMENT_HPP

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "arena.hpp"
#include "object.hpp"

namespace kirito {

// A lexical scope: name -> Handle, plus a handle to the enclosing scope. Environments are heap
// objects (addressed by handle) so a closure can capture and outlive the frame that made it, and
// so the whole scope chain stays GC-traceable and serializable. The chain is global -> module ->
// (later) function-local.
class EnvValue : public Object {
public:
    EnvValue() : hasParent_(false) {}
    explicit EnvValue(Handle parent) : parent_(parent), hasParent_(true) {}

    ValueKind kind() const override { return ValueKind::Environment; }
    std::string typeName() const override { return "Environment"; }
    bool truthy() const override { return true; }
    std::string str(StringifyCtx&) const override { return "<environment>"; }
    bool equals(const ObjectArena&, const Object& other) const override { return this == &other; }
    void children(std::vector<Handle>& out) const override {
        for (const auto& [name, h] : vars_) out.push_back(h);
        if (hasParent_) out.push_back(parent_);
    }

    bool hasParent() const { return hasParent_; }
    Handle parent() const { return parent_; }

    void define(const std::string& name, Handle h) { vars_[name] = h; }
    bool assignLocal(const std::string& name, Handle h) {
        auto it = vars_.find(name);
        if (it == vars_.end()) return false;
        it->second = h;
        return true;
    }
    const Handle* findLocal(const std::string& name) const {
        auto it = vars_.find(name);
        return it == vars_.end() ? nullptr : &it->second;
    }

private:
    std::unordered_map<std::string, Handle> vars_;
    Handle parent_{};
    bool hasParent_;
};

// Resolve a name innermost-first along the scope chain.
inline std::optional<Handle> envLookup(const ObjectArena& arena, Handle env, const std::string& name) {
    Handle cur = env;
    while (true) {
        const auto& e = static_cast<const EnvValue&>(arena.deref(cur));
        if (const Handle* h = e.findLocal(name)) return *h;
        if (!e.hasParent()) return std::nullopt;
        cur = e.parent();
    }
}

// Rebind the nearest existing binding; false if the name is undefined anywhere in the chain.
inline bool envAssign(ObjectArena& arena, Handle env, const std::string& name, Handle value) {
    Handle cur = env;
    while (true) {
        auto& e = static_cast<EnvValue&>(arena.deref(cur));
        if (e.assignLocal(name, value)) return true;
        if (!e.hasParent()) return false;
        cur = e.parent();
    }
}

}  // namespace kirito

#endif
