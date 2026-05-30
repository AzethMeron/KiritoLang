#ifndef KIRITO_STDLIB_DUMP_HPP
#define KIRITO_STDLIB_DUMP_HPP

#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"

namespace kirito {

// The `dump` module: compact BINARY serialization that preserves shared references and cycles.
// dump.dumps(value) -> Dump (an object wrapping the binary bytes); dump.loads(Dump|String) ->
// value. A Dump can also be saved to / loaded from a file. The format is a tagged, length-prefixed
// object table: every reachable object gets an id by identity (so aliasing and cycles round-trip).
//
// Wire format (little-endian):
//   magic  "KDMP" (4 bytes), version u8 = 1
//   u32 objectCount
//   then objectCount records, each: u8 tag + payload
//       0 None
//       1 Bool   : u8
//       2 Integer: i64
//       3 Float  : f64 (raw bits)
//       4 String : u32 len + bytes
//       5 List   : u32 count + count * u32 child-id
//       6 Dict   : u32 count + count * (u32 key-id, u32 val-id)
//       7 Set    : u32 count + count * u32 elem-id
//   u32 rootId

namespace dumpfmt {

// --- little-endian byte writers -----------------------------------------------------------------
inline void putU8(std::string& b, uint8_t v) { b.push_back(static_cast<char>(v)); }
inline void putU32(std::string& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}
inline void putI64(std::string& b, int64_t v) {
    uint64_t u = static_cast<uint64_t>(v);
    for (int i = 0; i < 8; ++i) b.push_back(static_cast<char>((u >> (8 * i)) & 0xFF));
}
inline void putF64(std::string& b, double d) {
    uint64_t u;
    std::memcpy(&u, &d, sizeof(u));
    for (int i = 0; i < 8; ++i) b.push_back(static_cast<char>((u >> (8 * i)) & 0xFF));
}

class Reader {
public:
    Reader(const std::string& s) : s_(s) {}
    bool eof() const { return pos_ >= s_.size(); }
    uint8_t u8() { need(1); return static_cast<uint8_t>(s_[pos_++]); }
    uint32_t u32() {
        need(4);
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(static_cast<uint8_t>(s_[pos_++])) << (8 * i);
        return v;
    }
    int64_t i64() {
        need(8);
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(static_cast<uint8_t>(s_[pos_++])) << (8 * i);
        return static_cast<int64_t>(v);
    }
    double f64() {
        uint64_t u = static_cast<uint64_t>(i64());
        double d;
        std::memcpy(&d, &u, sizeof(d));
        return d;
    }
    std::string bytes(uint32_t n) {
        need(n);
        std::string out = s_.substr(pos_, n);
        pos_ += n;
        return out;
    }
    void expect(const char* magic, std::size_t n) {
        need(n);
        if (s_.compare(pos_, n, magic, n) != 0) throw KiritoError("bad dump header");
        pos_ += n;
    }

private:
    void need(std::size_t n) { if (pos_ + n > s_.size()) throw KiritoError("truncated dump data"); }
    const std::string& s_;
    std::size_t pos_ = 0;
};

// Serialize the object graph rooted at `root` into compact binary bytes.
inline std::string write(KiritoVM& vm, Handle root) {
    std::unordered_map<const Object*, uint32_t> ids;
    std::vector<Handle> order;
    // assign ids by identity (id reserved before recursing -> cycles terminate)
    std::function<uint32_t(Handle)> assign = [&](Handle h) -> uint32_t {
        const Object* o = &vm.arena().deref(h);
        auto it = ids.find(o);
        if (it != ids.end()) return it->second;
        uint32_t id = static_cast<uint32_t>(order.size());
        ids[o] = id;
        order.push_back(h);
        std::vector<Handle> kids;
        o->children(kids);
        for (Handle k : kids) assign(k);
        return id;
    };
    uint32_t rootId = assign(root);
    auto idOf = [&](Handle h) { return ids.at(&vm.arena().deref(h)); };

    std::string b;
    b.append("KDMP");
    putU8(b, 1);
    putU32(b, static_cast<uint32_t>(order.size()));
    for (Handle h : order) {
        const Object& o = vm.arena().deref(h);
        switch (o.kind()) {
            case ValueKind::None: putU8(b, 0); break;
            case ValueKind::Bool: putU8(b, 1); putU8(b, static_cast<const BoolVal&>(o).value() ? 1 : 0); break;
            case ValueKind::Integer: putU8(b, 2); putI64(b, static_cast<const IntVal&>(o).value()); break;
            case ValueKind::Float: putU8(b, 3); putF64(b, static_cast<const FloatVal&>(o).value()); break;
            case ValueKind::String: {
                putU8(b, 4);
                const std::string& s = static_cast<const StrVal&>(o).value();
                putU32(b, static_cast<uint32_t>(s.size()));
                b.append(s);
                break;
            }
            case ValueKind::List: {
                putU8(b, 5);
                const auto& l = static_cast<const ListVal&>(o);
                putU32(b, static_cast<uint32_t>(l.elems.size()));
                for (Handle e : l.elems) putU32(b, idOf(e));
                break;
            }
            case ValueKind::Dict: {
                putU8(b, 6);
                const auto& d = static_cast<const DictVal&>(o);
                putU32(b, static_cast<uint32_t>(d.count));
                for (Handle k : d.keys()) { putU32(b, idOf(k)); putU32(b, idOf(*d.find(vm.arena(), k))); }
                break;
            }
            case ValueKind::Set: {
                putU8(b, 7);
                const auto& s = static_cast<const SetVal&>(o);
                putU32(b, static_cast<uint32_t>(s.count));
                for (Handle e : s.items()) putU32(b, idOf(e));
                break;
            }
            default:
                throw KiritoError("cannot dump type '" + o.typeName() + "'");
        }
    }
    putU32(b, rootId);
    return b;
}

// Deserialize binary bytes back into an object graph (two-pass: create all objects, then wire
// links — so references and cycles resolve).
inline Handle read(KiritoVM& vm, const std::string& data) {
    Reader r(data);
    r.expect("KDMP", 4);
    if (r.u8() != 1) throw KiritoError("unsupported dump version");
    uint32_t n = r.u32();
    // Each object record is at least one byte (its tag), so a claimed count exceeding the remaining
    // input is corrupt — reject before allocating to avoid a huge/bad allocation.
    if (n > data.size()) throw KiritoError("corrupt dump: object count exceeds data size");
    RootScope roots(vm);
    std::vector<Handle> objs(n);
    std::vector<uint8_t> tag(n);
    std::vector<std::vector<uint32_t>> links(n);
    for (uint32_t i = 0; i < n; ++i) {
        uint8_t t = r.u8();
        tag[i] = t;
        switch (t) {
            case 0: objs[i] = vm.none(); break;
            case 1: objs[i] = vm.makeBool(r.u8() != 0); break;
            case 2: objs[i] = roots.add(vm.makeInt(r.i64())); break;
            case 3: objs[i] = roots.add(vm.makeFloat(r.f64())); break;
            case 4: objs[i] = roots.add(vm.makeString(r.bytes(r.u32()))); break;
            case 5: {
                objs[i] = roots.add(vm.alloc(std::make_unique<ListVal>()));
                uint32_t c = r.u32();
                for (uint32_t k = 0; k < c; ++k) links[i].push_back(r.u32());
                break;
            }
            case 6: {
                objs[i] = roots.add(vm.alloc(std::make_unique<DictVal>()));
                uint32_t c = r.u32();
                for (uint32_t k = 0; k < c * 2; ++k) links[i].push_back(r.u32());
                break;
            }
            case 7: {
                objs[i] = roots.add(vm.alloc(std::make_unique<SetVal>()));
                uint32_t c = r.u32();
                for (uint32_t k = 0; k < c; ++k) links[i].push_back(r.u32());
                break;
            }
            default: throw KiritoError("bad dump tag");
        }
    }
    uint32_t rootId = r.u32();
    if (rootId >= n) throw KiritoError("dump root id out of range");
    for (uint32_t i = 0; i < n; ++i) {
        auto checkId = [&](uint32_t id) { if (id >= n) throw KiritoError("dump child id out of range"); return id; };
        if (tag[i] == 5) {
            auto& l = static_cast<ListVal&>(vm.arena().deref(objs[i]));
            for (uint32_t id : links[i]) l.elems.push_back(objs[checkId(id)]);
        } else if (tag[i] == 7) {
            auto& s = static_cast<SetVal&>(vm.arena().deref(objs[i]));
            for (uint32_t id : links[i]) s.add(vm.arena(), objs[checkId(id)]);
        } else if (tag[i] == 6) {
            auto& d = static_cast<DictVal&>(vm.arena().deref(objs[i]));
            for (std::size_t k = 0; k + 1 < links[i].size(); k += 2)
                d.set(vm.arena(), objs[checkId(links[i][k])], objs[checkId(links[i][k + 1])]);
        }
    }
    return objs[rootId];
}

}  // namespace dumpfmt

// The Dump value: a binary blob wrapping serialized bytes. Holds the raw bytes (as a std::string
// so arbitrary binary is safe), exposes size(), bytes() (-> String), and is itself stringifiable
// for debugging.
class DumpVal : public NativeClass<DumpVal> {
public:
    static constexpr const char* kTypeName = "Dump";
    std::string data;

    explicit DumpVal(std::string d) : data(std::move(d)) {}

    std::string str(StringifyCtx&) const override {
        return "Dump(" + std::to_string(data.size()) + " bytes)";
    }
    bool hashable() const override { return true; }
    std::size_t hash() const override { return std::hash<std::string>{}(data); }
    bool equals(const ObjectArena&, const Object& other) const override {
        const auto* d = dynamic_cast<const DumpVal*>(&other);
        return d && d->data == data;
    }

    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override {
        if (name == "size")
            return vm.alloc(std::make_unique<NativeFunction>(
                "size", [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
                    return vm.makeInt(static_cast<int64_t>(static_cast<DumpVal&>(vm.arena().deref(self)).data.size()));
                }, std::vector<Handle>{self}));
        if (name == "bytes")
            return vm.alloc(std::make_unique<NativeFunction>(
                "bytes", [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
                    return vm.makeString(static_cast<DumpVal&>(vm.arena().deref(self)).data);
                }, std::vector<Handle>{self}));
        if (name == "save")
            return vm.alloc(std::make_unique<NativeFunction>(
                "save", [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                    const Object& o = vm.arena().deref(a[0]);
                    if (o.kind() != ValueKind::String) throw KiritoError("save expects a path String");
                    std::ofstream f(static_cast<const StrVal&>(o).value(), std::ios::binary);
                    if (!f) throw KiritoError("could not open file for saving");
                    f << static_cast<DumpVal&>(vm.arena().deref(self)).data;
                    return vm.none();
                }, std::vector<Handle>{self}));
        return Object::getAttr(vm, self, name);
    }
};

class DumpModule : public NativeModule {
public:
    std::string name() const override { return "dump"; }
    void setup(ModuleBuilder& m) override {
        m.fn("dumps", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.alloc(std::make_unique<DumpVal>(dumpfmt::write(vm, a[0])));
        });
        m.fn("loads", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const Object& o = vm.arena().deref(a[0]);
            const auto* d = dynamic_cast<const DumpVal*>(&o);
            if (d) return dumpfmt::read(vm, d->data);
            if (o.kind() == ValueKind::String) return dumpfmt::read(vm, static_cast<const StrVal&>(o).value());
            throw KiritoError("loads expects a Dump or a String of bytes");
        });
        m.fn("Dump", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            // Dump(bytes) wraps an existing byte String as a Dump value.
            const Object& o = vm.arena().deref(a[0]);
            if (o.kind() != ValueKind::String) throw KiritoError("Dump expects a byte String");
            return vm.alloc(std::make_unique<DumpVal>(static_cast<const StrVal&>(o).value()));
        });
        m.fn("load", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const Object& o = vm.arena().deref(a[0]);
            if (o.kind() != ValueKind::String) throw KiritoError("load expects a path String");
            std::ifstream f(static_cast<const StrVal&>(o).value(), std::ios::binary);
            if (!f) throw KiritoError("could not open file for loading");
            std::stringstream ss;
            ss << f.rdbuf();
            return dumpfmt::read(vm, ss.str());
        });
    }
};

}  // namespace kirito

#endif
