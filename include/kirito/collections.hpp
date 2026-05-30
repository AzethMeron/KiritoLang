#ifndef KIRITO_COLLECTIONS_HPP
#define KIRITO_COLLECTIONS_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "arena.hpp"
#include "object.hpp"

namespace kirito {

// Containers store element handles, so aliasing (and, later, cycles) work naturally. Methods that
// need the VM (getItem/setItem/getAttr) are declared here and defined in runtime.hpp once
// KiritoVM is complete; everything else is inline.

// Render a sequence/mapping with cycle guarding via the StringifyCtx active set (keyed on `this`).
template <typename F>
inline std::string stringifyGuarded(const Object* self, StringifyCtx& ctx,
                                    const char* open, const char* close, F emit) {
    if (ctx.active.count(self)) return std::string(open) + "..." + close;
    ctx.active.insert(self);
    std::string out = open;
    out += emit();
    out += close;
    ctx.active.erase(self);
    return out;
}

class ListVal : public Object {
public:
    std::vector<Handle> elems;

    ValueKind kind() const override { return ValueKind::List; }
    std::string typeName() const override { return "List"; }
    bool truthy() const override { return !elems.empty(); }

    std::string str(StringifyCtx& ctx) const override {
        return stringifyGuarded(this, ctx, "[", "]", [&] {
            std::string s;
            for (std::size_t i = 0; i < elems.size(); ++i) {
                if (i) s += ", ";
                s += ctx.arena.deref(elems[i]).str(ctx);
            }
            return s;
        });
    }
    bool equals(const ObjectArena& arena, const Object& other) const override {
        if (other.kind() != ValueKind::List) return false;
        const auto& o = static_cast<const ListVal&>(other);
        if (o.elems.size() != elems.size()) return false;
        for (std::size_t i = 0; i < elems.size(); ++i)
            if (!arena.deref(elems[i]).equals(arena, arena.deref(o.elems[i]))) return false;
        return true;
    }
    void children(std::vector<Handle>& out) const override {
        out.insert(out.end(), elems.begin(), elems.end());
    }
    std::optional<std::vector<Handle>> iterate(KiritoVM&) override { return elems; }
    std::optional<int64_t> length(KiritoVM&) override { return static_cast<int64_t>(elems.size()); }

    Handle getItem(KiritoVM&, Handle key) override;
    void setItem(KiritoVM&, Handle key, Handle value) override;
    Handle slice(KiritoVM&, Handle start, Handle stop, Handle step) override;
    bool contains(KiritoVM&, Handle value) override;
    Handle getAttr(KiritoVM&, Handle self, std::string_view name) override;
};

// Hash-bucketed mapping. Keys must be hashable; lookup hashes then compares with the value
// protocol's equals within the bucket.
class DictVal : public Object {
public:
    std::unordered_map<std::size_t, std::vector<std::pair<Handle, Handle>>> buckets;
    std::size_t count = 0;

    ValueKind kind() const override { return ValueKind::Dict; }
    std::string typeName() const override { return "Dict"; }
    bool truthy() const override { return count > 0; }

    void set(ObjectArena& arena, Handle key, Handle value) {
        const Object& k = arena.deref(key);
        if (!k.hashable()) throw KiritoError("unhashable type '" + k.typeName() + "'");
        auto& bucket = buckets[k.hash()];
        for (auto& [ek, ev] : bucket)
            if (arena.deref(ek).equals(arena, k)) { ev = value; return; }
        bucket.emplace_back(key, value);
        ++count;
    }
    const Handle* find(const ObjectArena& arena, Handle key) const {
        const Object& k = arena.deref(key);
        if (!k.hashable()) throw KiritoError("unhashable type '" + k.typeName() + "'");
        auto it = buckets.find(k.hash());
        if (it == buckets.end()) return nullptr;
        for (const auto& [ek, ev] : it->second)
            if (arena.deref(ek).equals(arena, k)) return &ev;
        return nullptr;
    }
    std::vector<Handle> keys() const {
        std::vector<Handle> out;
        out.reserve(count);
        for (const auto& [h, bucket] : buckets)
            for (const auto& [k, v] : bucket) out.push_back(k);
        return out;
    }

    std::string str(StringifyCtx& ctx) const override {
        return stringifyGuarded(this, ctx, "{", "}", [&] {
            std::string s;
            bool first = true;
            for (const auto& [h, bucket] : buckets)
                for (const auto& [k, v] : bucket) {
                    if (!first) s += ", ";
                    first = false;
                    s += ctx.arena.deref(k).str(ctx);
                    s += ": ";
                    s += ctx.arena.deref(v).str(ctx);
                }
            return s;
        });
    }
    bool equals(const ObjectArena& arena, const Object& other) const override {
        if (other.kind() != ValueKind::Dict) return false;
        const auto& o = static_cast<const DictVal&>(other);
        if (o.count != count) return false;
        for (const auto& [h, bucket] : buckets)
            for (const auto& [k, v] : bucket) {
                const Handle* ov = o.find(arena, k);
                if (!ov || !arena.deref(v).equals(arena, arena.deref(*ov))) return false;
            }
        return true;
    }
    void children(std::vector<Handle>& out) const override {
        for (const auto& [h, bucket] : buckets)
            for (const auto& [k, v] : bucket) { out.push_back(k); out.push_back(v); }
    }
    std::optional<std::vector<Handle>> iterate(KiritoVM&) override { return keys(); }
    std::optional<int64_t> length(KiritoVM&) override { return static_cast<int64_t>(count); }

    bool remove(ObjectArena& arena, Handle key) {
        const Object& k = arena.deref(key);
        if (!k.hashable()) return false;
        auto it = buckets.find(k.hash());
        if (it == buckets.end()) return false;
        auto& bucket = it->second;
        for (std::size_t i = 0; i < bucket.size(); ++i)
            if (arena.deref(bucket[i].first).equals(arena, k)) {
                bucket.erase(bucket.begin() + i);
                --count;
                return true;
            }
        return false;
    }

    Handle getItem(KiritoVM&, Handle key) override;
    void setItem(KiritoVM&, Handle key, Handle value) override;
    bool contains(KiritoVM&, Handle key) override;  // key membership
    Handle getAttr(KiritoVM&, Handle self, std::string_view name) override;
};

// Hash-bucketed set of unique values.
class SetVal : public Object {
public:
    std::unordered_map<std::size_t, std::vector<Handle>> buckets;
    std::size_t count = 0;

    ValueKind kind() const override { return ValueKind::Set; }
    std::string typeName() const override { return "Set"; }
    bool truthy() const override { return count > 0; }

    bool add(ObjectArena& arena, Handle value) {
        const Object& v = arena.deref(value);
        if (!v.hashable()) throw KiritoError("unhashable type '" + v.typeName() + "'");
        auto& bucket = buckets[v.hash()];
        for (Handle e : bucket)
            if (arena.deref(e).equals(arena, v)) return false;
        bucket.push_back(value);
        ++count;
        return true;
    }
    bool contains(const ObjectArena& arena, Handle value) const {
        const Object& v = arena.deref(value);
        if (!v.hashable()) return false;
        auto it = buckets.find(v.hash());
        if (it == buckets.end()) return false;
        for (Handle e : it->second)
            if (arena.deref(e).equals(arena, v)) return true;
        return false;
    }
    std::vector<Handle> items() const {
        std::vector<Handle> out;
        out.reserve(count);
        for (const auto& [h, bucket] : buckets)
            for (Handle e : bucket) out.push_back(e);
        return out;
    }

    std::string str(StringifyCtx& ctx) const override {
        return stringifyGuarded(this, ctx, "{", "}", [&] {
            std::string s;
            bool first = true;
            for (const auto& [h, bucket] : buckets)
                for (Handle e : bucket) {
                    if (!first) s += ", ";
                    first = false;
                    s += ctx.arena.deref(e).str(ctx);
                }
            return s;
        });
    }
    bool equals(const ObjectArena& arena, const Object& other) const override {
        if (other.kind() != ValueKind::Set) return false;
        const auto& o = static_cast<const SetVal&>(other);
        if (o.count != count) return false;
        for (const auto& [h, bucket] : buckets)
            for (Handle e : bucket)
                if (!o.contains(arena, e)) return false;
        return true;
    }
    void children(std::vector<Handle>& out) const override {
        for (const auto& [h, bucket] : buckets)
            out.insert(out.end(), bucket.begin(), bucket.end());
    }
    std::optional<std::vector<Handle>> iterate(KiritoVM&) override { return items(); }
    std::optional<int64_t> length(KiritoVM&) override { return static_cast<int64_t>(count); }
    bool contains(KiritoVM&, Handle value) override;

    Handle getAttr(KiritoVM&, Handle self, std::string_view name) override;
};

}  // namespace kirito

#endif
