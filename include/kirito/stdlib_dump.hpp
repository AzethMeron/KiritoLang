#ifndef KIRITO_STDLIB_DUMP_HPP
#define KIRITO_STDLIB_DUMP_HPP

#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "native.hpp"
#include "stdlib_serde.hpp"

namespace kirito {

// The `dump` module: compact BINARY serialization that preserves shared references and cycles (like
// a portable `pickle`). The graph walk and reconstruction are shared with the text `serialize` module
// via serde::flatten / serde::rebuild (stdlib_serde.hpp); this file is only the binary codec plus the
// `Dump` blob value.
//
// Wire format (little-endian): magic "KDMP" (4 bytes), version u8 = 1, u32 objectCount, then
// objectCount records each `u8 tag + payload` (tags match serde::Tag: 0 None, 1 Bool u8, 2 Integer
// i64, 3 Float f64-bits, 4 String u32 len + bytes, 5 List u32 count + ids, 6 Dict u32 count + (k,v)
// id pairs, 7 Set u32 count + ids), then u32 rootId.
namespace dumpfmt {

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
    explicit Reader(const std::string& s) : s_(s) {}
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

inline std::string encode(const std::vector<serde::Node>& nodes, uint32_t rootId) {
    std::string b;
    b.append("KDMP");
    putU8(b, 1);
    putU32(b, static_cast<uint32_t>(nodes.size()));
    for (const serde::Node& n : nodes) {
        putU8(b, static_cast<uint8_t>(n.tag));
        switch (n.tag) {
            case serde::Tag::None: break;
            case serde::Tag::Bool: putU8(b, n.b ? 1 : 0); break;
            case serde::Tag::Integer: putI64(b, n.i); break;
            case serde::Tag::Float: putF64(b, n.f); break;
            case serde::Tag::String: putU32(b, static_cast<uint32_t>(n.s.size())); b.append(n.s); break;
            case serde::Tag::List:
            case serde::Tag::Set:
                putU32(b, static_cast<uint32_t>(n.links.size()));
                for (uint32_t id : n.links) putU32(b, id);
                break;
            case serde::Tag::Dict:
                putU32(b, static_cast<uint32_t>(n.links.size() / 2));
                for (uint32_t id : n.links) putU32(b, id);
                break;
            case serde::Tag::Object:  // class name + key/val id pairs
                putU32(b, static_cast<uint32_t>(n.s.size())); b.append(n.s);
                putU32(b, static_cast<uint32_t>(n.links.size() / 2));
                for (uint32_t id : n.links) putU32(b, id);
                break;
            case serde::Tag::Stateful:  // class name + single state id
                putU32(b, static_cast<uint32_t>(n.s.size())); b.append(n.s);
                putU32(b, n.links.empty() ? 0u : n.links[0]);
                break;
        }
    }
    putU32(b, rootId);
    return b;
}

inline std::pair<std::vector<serde::Node>, uint32_t> decode(const std::string& data) {
    Reader r(data);
    r.expect("KDMP", 4);
    if (r.u8() != 1) throw KiritoError("unsupported dump version");
    uint32_t n = r.u32();
    // Each record is at least one tag byte, so a count exceeding the input is corrupt — reject before
    // allocating to avoid a huge/bad allocation.
    if (n > data.size()) throw KiritoError("corrupt dump: object count exceeds data size");
    std::vector<serde::Node> nodes(n);
    for (uint32_t i = 0; i < n; ++i) {
        serde::Node& nd = nodes[i];
        uint8_t t = r.u8();
        switch (t) {
            case 0: nd.tag = serde::Tag::None; break;
            case 1: nd.tag = serde::Tag::Bool; nd.b = r.u8() != 0; break;
            case 2: nd.tag = serde::Tag::Integer; nd.i = r.i64(); break;
            case 3: nd.tag = serde::Tag::Float; nd.f = r.f64(); break;
            case 4: nd.tag = serde::Tag::String; nd.s = r.bytes(r.u32()); break;
            case 5: { nd.tag = serde::Tag::List; uint32_t c = r.u32(); for (uint32_t k = 0; k < c; ++k) nd.links.push_back(r.u32()); break; }
            case 6: { nd.tag = serde::Tag::Dict; uint32_t c = r.u32(); for (uint32_t k = 0; k < c * 2; ++k) nd.links.push_back(r.u32()); break; }
            case 7: { nd.tag = serde::Tag::Set; uint32_t c = r.u32(); for (uint32_t k = 0; k < c; ++k) nd.links.push_back(r.u32()); break; }
            case 8: { nd.tag = serde::Tag::Object; nd.s = r.bytes(r.u32()); uint32_t c = r.u32(); for (uint32_t k = 0; k < c * 2; ++k) nd.links.push_back(r.u32()); break; }
            case 9: { nd.tag = serde::Tag::Stateful; nd.s = r.bytes(r.u32()); nd.links.push_back(r.u32()); break; }
            default: throw KiritoError("bad dump tag");
        }
    }
    uint32_t rootId = r.u32();
    return {std::move(nodes), rootId};
}

inline std::string write(KiritoVM& vm, Handle root) {
    auto [nodes, rootId] = serde::flatten(vm, root, "dump");
    return encode(nodes, rootId);
}

inline Handle read(KiritoVM& vm, const std::string& data) {
    auto [nodes, rootId] = decode(data);
    return serde::rebuild(vm, nodes, rootId);
}

}  // namespace dumpfmt

// The Dump value: a binary blob wrapping serialized bytes. Holds the raw bytes (as a std::string so
// arbitrary binary is safe), exposes size(), bytes() (-> String), save(path), and is stringifiable
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
            return makeMethod(vm,
                "size", {}, [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
                    return vm.makeInt(static_cast<int64_t>(static_cast<DumpVal&>(vm.arena().deref(self)).data.size()));
                }, std::vector<Handle>{self});
        if (name == "bytes")
            return makeMethod(vm,
                "bytes", {}, [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
                    return vm.makeString(static_cast<DumpVal&>(vm.arena().deref(self)).data);
                }, std::vector<Handle>{self});
        if (name == "save")
            return makeMethod(vm,
                "save", {"path"}, [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                    const Object& o = vm.arena().deref(a[0]);
                    if (o.kind() != ValueKind::String) throw KiritoError("save expects a path String");
                    std::ofstream f(static_cast<const StrVal&>(o).value(), std::ios::binary);
                    if (!f) throw KiritoError("could not open file for saving");
                    f << static_cast<DumpVal&>(vm.arena().deref(self)).data;
                    return vm.none();
                }, std::vector<Handle>{self});
        return Object::getAttr(vm, self, name);
    }
};

class DumpModule : public NativeModule {
public:
    std::string name() const override { return "dump"; }
    void setup(ModuleBuilder& m) override {
        m.fn("dumps", {{"value"}}, "Dump", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.alloc(std::make_unique<DumpVal>(dumpfmt::write(vm, Args(vm, a, "dumps")[0])));
        });
        m.fn("loads", {{"data"}}, "", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Value v = Args(vm, a, "loads")[0];
            const Object& o = vm.arena().deref(v.handle());
            const auto* d = dynamic_cast<const DumpVal*>(&o);
            if (d) return dumpfmt::read(vm, d->data);
            if (v.isString()) return dumpfmt::read(vm, v.asString());
            throw KiritoError("loads expects a Dump or a String of bytes");
        });
        m.fn("Dump", {{"bytes", "String"}}, "Dump", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            // Dump(bytes) wraps an existing byte String as a Dump value.
            return vm.alloc(std::make_unique<DumpVal>(Args(vm, a, "Dump")[0].asString("Dump bytes")));
        });
        m.fn("load", {{"path", "String"}}, "", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::ifstream f(Args(vm, a, "load")[0].asString("load path"), std::ios::binary);
            if (!f) throw KiritoError("could not open file for loading");
            std::stringstream ss;
            ss << f.rdbuf();
            return dumpfmt::read(vm, ss.str());
        });
    }
};

}  // namespace kirito

#endif
