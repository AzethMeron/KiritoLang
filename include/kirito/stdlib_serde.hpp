#ifndef KIRITO_STDLIB_SERDE_HPP
#define KIRITO_STDLIB_SERDE_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"

namespace kirito {

// Shared object-graph (de)serialization core for the `serialize` (human-readable text) and `dump`
// (compact binary) modules. Both preserve shared references and cycles in exactly the same way —
// they differ ONLY in how the flat object table below is encoded to / decoded from bytes. The graph
// walk (identity ids, cycle termination, supported-type knowledge) and the two-pass reconstruction
// live here once; each module supplies just its byte format.
namespace serde {

// Format-neutral view of one serialized object: a kind tag, the scalar payload (only the field
// matching `tag` is meaningful), and — for containers — the ids of referenced objects (List/Set: the
// elements; Dict: key0, val0, key1, val1, …). Tag values are the binary wire tags too.
enum class Tag : uint8_t { None = 0, Bool = 1, Integer = 2, Float = 3, String = 4, List = 5, Dict = 6, Set = 7 };

struct Node {
    Tag tag = Tag::None;
    bool b = false;
    int64_t i = 0;
    double f = 0.0;
    std::string s;
    std::vector<uint32_t> links;
};

// Walk the graph rooted at `root`, giving every reachable object an id by identity (a value reachable
// by two paths is recorded once; an id is reserved before recursing so cycles terminate) and flatten
// it into a Node table. `verb` ("serialize" / "dump") names the operation in error messages. Returns
// the table and the root's id. Supported kinds: None/Bool/Integer/Float/String/List/Dict/Set.
inline std::pair<std::vector<Node>, uint32_t> flatten(KiritoVM& vm, Handle root, const char* verb) {
    std::unordered_map<const Object*, uint32_t> ids;
    std::vector<Handle> order;
    int depth = 0;
    std::function<uint32_t(Handle)> assign = [&](Handle h) -> uint32_t {
        const Object* o = &vm.arena().deref(h);
        auto it = ids.find(o);
        if (it != ids.end()) return it->second;
        uint32_t id = static_cast<uint32_t>(order.size());
        ids[o] = id;
        order.push_back(h);
        if (++depth > 10000) throw KiritoError(std::string("structure too deeply nested to ") + verb);
        std::vector<Handle> kids;
        o->children(kids);
        for (Handle k : kids) assign(k);
        --depth;
        return id;
    };
    uint32_t rootId = assign(root);
    auto idOf = [&](Handle h) { return ids.at(&vm.arena().deref(h)); };

    std::vector<Node> nodes;
    nodes.reserve(order.size());
    for (Handle h : order) {
        const Object& o = vm.arena().deref(h);
        Node n;
        switch (o.kind()) {
            case ValueKind::None: n.tag = Tag::None; break;
            case ValueKind::Bool: n.tag = Tag::Bool; n.b = static_cast<const BoolVal&>(o).value(); break;
            case ValueKind::Integer: n.tag = Tag::Integer; n.i = static_cast<const IntVal&>(o).value(); break;
            case ValueKind::Float: n.tag = Tag::Float; n.f = static_cast<const FloatVal&>(o).value(); break;
            case ValueKind::String: n.tag = Tag::String; n.s = static_cast<const StrVal&>(o).value(); break;
            case ValueKind::List:
                n.tag = Tag::List;
                for (Handle e : static_cast<const ListVal&>(o).elems) n.links.push_back(idOf(e));
                break;
            case ValueKind::Dict: {
                n.tag = Tag::Dict;
                const auto& d = static_cast<const DictVal&>(o);
                for (Handle k : d.keys()) { n.links.push_back(idOf(k)); n.links.push_back(idOf(*d.find(vm.arena(), k))); }
                break;
            }
            case ValueKind::Set:
                n.tag = Tag::Set;
                for (Handle e : static_cast<const SetVal&>(o).items()) n.links.push_back(idOf(e));
                break;
            default:
                throw KiritoError(std::string("cannot ") + verb + " type '" + o.typeName() + "'");
        }
        nodes.push_back(std::move(n));
    }
    return {std::move(nodes), rootId};
}

// Rebuild the graph from a Node table in two passes: create every object (scalars valued, containers
// empty) so ids resolve, then wire the container links — restoring shared references and cycles. The
// root id and every link id are bounds-checked against the table.
inline Handle rebuild(KiritoVM& vm, const std::vector<Node>& nodes, uint32_t rootId) {
    uint32_t n = static_cast<uint32_t>(nodes.size());
    if (rootId >= n) throw KiritoError("serialized root id out of range");
    RootScope roots(vm);
    std::vector<Handle> objs(n);
    for (uint32_t i = 0; i < n; ++i) {
        const Node& nd = nodes[i];
        switch (nd.tag) {
            case Tag::None: objs[i] = vm.none(); break;
            case Tag::Bool: objs[i] = vm.makeBool(nd.b); break;
            case Tag::Integer: objs[i] = roots.add(vm.makeInt(nd.i)); break;
            case Tag::Float: objs[i] = roots.add(vm.makeFloat(nd.f)); break;
            case Tag::String: objs[i] = roots.add(vm.makeString(nd.s)); break;
            case Tag::List: objs[i] = roots.add(vm.alloc(std::make_unique<ListVal>())); break;
            case Tag::Dict: objs[i] = roots.add(vm.alloc(std::make_unique<DictVal>())); break;
            case Tag::Set: objs[i] = roots.add(vm.alloc(std::make_unique<SetVal>())); break;
        }
    }
    auto checkId = [&](uint32_t id) -> uint32_t {
        if (id >= n) throw KiritoError("serialized child id out of range");
        return id;
    };
    for (uint32_t i = 0; i < n; ++i) {
        const Node& nd = nodes[i];
        if (nd.tag == Tag::List) {
            auto& l = static_cast<ListVal&>(vm.arena().deref(objs[i]));
            for (uint32_t id : nd.links) l.elems.push_back(objs[checkId(id)]);
        } else if (nd.tag == Tag::Set) {
            auto& s = static_cast<SetVal&>(vm.arena().deref(objs[i]));
            for (uint32_t id : nd.links) s.add(vm.arena(), objs[checkId(id)]);
        } else if (nd.tag == Tag::Dict) {
            auto& d = static_cast<DictVal&>(vm.arena().deref(objs[i]));
            for (std::size_t k = 0; k + 1 < nd.links.size(); k += 2)
                d.set(vm.arena(), objs[checkId(nd.links[k])], objs[checkId(nd.links[k + 1])]);
        }
    }
    return objs[rootId];
}

}  // namespace serde
}  // namespace kirito

#endif
