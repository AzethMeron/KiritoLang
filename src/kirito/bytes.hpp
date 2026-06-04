#ifndef KIRITO_BYTES_HPP
#define KIRITO_BYTES_HPP

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "builtins.hpp"   // utf8Encode / utf8DecodeAt / utf8Starts, StrVal
#include "native.hpp"     // NativeClass, makeMethod, value.hpp

namespace kirito {

// An immutable sequence of raw bytes (0–255), like Python's `bytes`. Distinct from String, which is
// Unicode (code-point) text: a String holds UTF-8 and indexes by code point, so it cannot losslessly
// hold or address arbitrary binary. Bytes indexes by byte (b[i] -> Integer 0–255), so it is the right
// type for binary I/O — network downloads, compressed data, file contents. Convert with
// `s.encode([enc])` (String -> Bytes) and `b.decode([enc])` (Bytes -> String); `latin-1` maps each
// byte to one code point and back losslessly.
class BytesVal : public NativeClass<BytesVal> {
public:
    static constexpr const char* kTypeName = "Bytes";
    std::string data;

    BytesVal() = default;
    explicit BytesVal(std::string d) : data(std::move(d)) {}

    bool truthy() const override { return !data.empty(); }

    // repr like Python: b'...' with printable ASCII verbatim and \xHH / \n \t \r \\ \' for the rest.
    std::string str(StringifyCtx&) const override {
        std::string out = "b'";
        for (unsigned char c : data) {
            switch (c) {
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                case '\r': out += "\\r"; break;
                case '\\': out += "\\\\"; break;
                case '\'': out += "\\'"; break;
                default:
                    if (c >= 0x20 && c < 0x7f) out += static_cast<char>(c);
                    else {
                        static const char* hex = "0123456789abcdef";
                        out += "\\x";
                        out += hex[c >> 4];
                        out += hex[c & 0xf];
                    }
            }
        }
        out += "'";
        return out;
    }

    bool equals(const ObjectArena&, const Object& other) const override {
        const auto* b = dynamic_cast<const BytesVal*>(&other);
        return b && b->data == data;
    }
    bool hashable() const override { return true; }
    std::size_t hash() const override { return std::hash<std::string>{}(data); }

    std::optional<int64_t> length(KiritoVM&) override { return static_cast<int64_t>(data.size()); }

    // b[i] -> the byte at i as an Integer (Python negative indexing).
    Handle getItem(KiritoVM& vm, std::span<const Handle> keys) override {
        const Object& k = vm.arena().deref(singleKey(*this, keys));
        if (k.kind() != ValueKind::Integer)
            throw KiritoError("Bytes index must be Integer, not '" + k.typeName() + "'");
        int64_t i = static_cast<const IntVal&>(k).value();
        int64_t n = static_cast<int64_t>(data.size());
        if (i < 0) i += n;
        if (i < 0 || i >= n) throw KiritoError("Bytes index out of range");
        return vm.makeInt(static_cast<unsigned char>(data[static_cast<std::size_t>(i)]));
    }

    // b[a:b:c] -> a Bytes slice.
    Handle slice(KiritoVM& vm, Handle sH, Handle eH, Handle stH) override {
        auto idx = sliceIndices(vm, static_cast<int64_t>(data.size()), sH, eH, stH);
        std::string out;
        out.reserve(idx.size());
        for (int64_t i : idx) out += data[static_cast<std::size_t>(i)];
        return vm.alloc(std::make_unique<BytesVal>(std::move(out)));
    }

    std::optional<std::vector<Handle>> iterate(KiritoVM& vm) override {
        std::vector<Handle> out;
        out.reserve(data.size());
        for (unsigned char c : data) out.push_back(vm.makeInt(c));
        return out;
    }

    // `x in b`: an Integer byte value, or a Bytes subsequence.
    bool contains(KiritoVM& vm, Handle value) override {
        const Object& o = vm.arena().deref(value);
        if (o.kind() == ValueKind::Integer) {
            int64_t v = static_cast<const IntVal&>(o).value();
            if (v < 0 || v > 255) return false;
            return data.find(static_cast<char>(static_cast<unsigned char>(v))) != std::string::npos;
        }
        if (const auto* b = dynamic_cast<const BytesVal*>(&o))
            return data.find(b->data) != std::string::npos;
        throw KiritoError("'in <Bytes>' requires an Integer byte or a Bytes, not '" + o.typeName() + "'");
    }

    Handle binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) override {
        const Object& b = vm.arena().deref(rhs);
        if (op == BinOp::Add) {
            const auto* o = dynamic_cast<const BytesVal*>(&b);
            if (!o) throw KiritoError("can only concatenate Bytes to Bytes, not '" + b.typeName() + "'");
            return vm.alloc(std::make_unique<BytesVal>(data + o->data));
        }
        if (op == BinOp::Mul) {
            if (b.kind() != ValueKind::Integer) throw KiritoError("can only repeat Bytes by an Integer");
            int64_t nrep = static_cast<const IntVal&>(b).value();
            if (nrep <= 0 || data.empty()) return vm.alloc(std::make_unique<BytesVal>());
            if (static_cast<uint64_t>(nrep) > (256ull * 1024 * 1024) / data.size())
                throw KiritoError("repeated Bytes too large");
            std::string out;
            out.reserve(data.size() * static_cast<std::size_t>(nrep));
            for (int64_t i = 0; i < nrep; ++i) out += data;
            return vm.alloc(std::make_unique<BytesVal>(std::move(out)));
        }
        if (const auto* o = dynamic_cast<const BytesVal*>(&b)) {  // lexicographic ordering (Python-like)
            switch (op) {
                case BinOp::Lt: return vm.makeBool(data < o->data);
                case BinOp::Le: return vm.makeBool(data <= o->data);
                case BinOp::Gt: return vm.makeBool(data > o->data);
                case BinOp::Ge: return vm.makeBool(data >= o->data);
                default: break;
            }
        }
        return Object::binary(vm, op, self, rhs);
    }

    std::vector<std::string> inspectMembers() const override {
        return {"decode(encoding) -> String", "hex() -> String"};
    }

    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override;

private:
    // Concrete indices for a Python slice over [0,len) (negative indices, clamping, negative step).
    static std::vector<int64_t> sliceIndices(KiritoVM& vm, int64_t len, Handle sH, Handle eH, Handle stH) {
        auto opt = [&](Handle h) -> std::optional<int64_t> {
            const Object& o = vm.arena().deref(h);
            if (o.kind() == ValueKind::None) return std::nullopt;
            if (o.kind() != ValueKind::Integer) throw KiritoError("slice indices must be Integer or None");
            return static_cast<const IntVal&>(o).value();
        };
        std::optional<int64_t> so = opt(sH), eo = opt(eH), sto = opt(stH);
        int64_t step = sto.value_or(1);
        if (step == 0) throw KiritoError("slice step cannot be zero");
        int64_t lower = step < 0 ? -1 : 0, upper = step < 0 ? len - 1 : len, start, stop;
        if (!so) start = step < 0 ? upper : lower;
        else { start = *so; if (start < 0) { start += len; if (start < lower) start = lower; }
               else if (start > upper) start = upper; }
        if (!eo) stop = step < 0 ? lower : upper;
        else { stop = *eo; if (stop < 0) { stop += len; if (stop < lower) stop = lower; }
               else if (stop > upper) stop = upper; }
        std::vector<int64_t> idx;
        if (step > 0) for (int64_t i = start; i < stop; i += step) idx.push_back(i);
        else for (int64_t i = start; i > stop; i += step) idx.push_back(i);
        return idx;
    }
};

namespace bytesutil {

// Normalise an encoding name: lowercase, '-'/'_' removed (so "UTF-8", "utf_8", "utf8" all match).
inline std::string normEnc(const std::string& e) {
    std::string out;
    for (char c : e) {
        if (c == '-' || c == '_') continue;
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// Encode a String's code points to bytes under `enc`. utf-8 keeps the String's bytes as-is; latin-1
// maps each code point (must be 0–255) to one byte; ascii requires code points < 128.
inline std::string encode(const std::string& s, const std::string& enc) {
    std::string e = normEnc(enc);
    if (e == "utf8") return s;  // a Kirito String already stores UTF-8 bytes
    if (e == "latin1" || e == "iso88591" || e == "ascii") {
        unsigned cap = (e == "ascii") ? 0x80u : 0x100u;
        std::string out;
        for (std::size_t st : utf8Starts(s)) {
            unsigned cp = utf8DecodeAt(s, st);
            if (cp >= cap)
                throw KiritoError("'" + enc + "' codec can't encode code point U+" + std::to_string(cp));
            out += static_cast<char>(static_cast<unsigned char>(cp));
        }
        return out;
    }
    throw KiritoError("unknown encoding: '" + enc + "'");
}

// Decode bytes to a String (UTF-8 text) under `enc`. utf-8 keeps the bytes (they are already the
// String's storage); latin-1/ascii map each byte to a code point, then UTF-8-encode it.
inline std::string decode(const std::string& data, const std::string& enc) {
    std::string e = normEnc(enc);
    if (e == "utf8") return data;
    if (e == "latin1" || e == "iso88591" || e == "ascii") {
        unsigned cap = (e == "ascii") ? 0x80u : 0x100u;
        std::string out;
        for (unsigned char c : data) {
            if (c >= cap) throw KiritoError("'" + enc + "' codec can't decode byte 0x" + std::to_string(c));
            utf8Encode(c, out);
        }
        return out;
    }
    throw KiritoError("unknown encoding: '" + enc + "'");
}

inline std::string toHex(const std::string& data) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (unsigned char c : data) { out += hex[c >> 4]; out += hex[c & 0xf]; }
    return out;
}

inline std::string fromHex(const std::string& s) {
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    std::size_t i = 0;
    while (i < s.size()) {
        if (std::isspace(static_cast<unsigned char>(s[i]))) { ++i; continue; }
        if (i + 1 >= s.size()) throw KiritoError("fromhex: odd-length hex string");
        int hi = nib(s[i]), lo = nib(s[i + 1]);
        if (hi < 0 || lo < 0) throw KiritoError("fromhex: non-hex digit");
        out += static_cast<char>(static_cast<unsigned char>((hi << 4) | lo));
        i += 2;
    }
    return out;
}

}  // namespace bytesutil

inline Handle BytesVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    auto self_b = [](KiritoVM& vm, Handle h) -> BytesVal& { return static_cast<BytesVal&>(vm.arena().deref(h)); };
    if (name == "decode")
        return makeMethod(vm, "decode", {"encoding"}, [self, self_b](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::string enc = "utf-8";
            if (!a.empty() && vm.arena().deref(a[0]).kind() != ValueKind::None) enc = Value(vm, a[0]).asString("decode encoding");
            return vm.makeString(bytesutil::decode(self_b(vm, self).data, enc));
        }, std::vector<Handle>{self});
    if (name == "hex")
        return makeMethod(vm, "hex", {}, [self, self_b](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return vm.makeString(bytesutil::toHex(self_b(vm, self).data));
        }, std::vector<Handle>{self});
    // serialization: round-trip the raw bytes as a latin-1 String (lossless byte<->code-point).
    if (name == "_getstate_")
        return makeMethod(vm, "_getstate_", {}, [self, self_b](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return vm.makeString(bytesutil::decode(self_b(vm, self).data, "latin-1"));
        }, std::vector<Handle>{self});
    if (name == "_setstate_")
        return makeMethod(vm, "_setstate_", {"state"}, [self, self_b](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            self_b(vm, self).data = bytesutil::encode(Value(vm, a[0]).asString("Bytes state"), "latin-1");
            return vm.none();
        }, std::vector<Handle>{self});
    return Object::getAttr(vm, self, name);
}

// Build a Bytes from a Kirito value: a List of Integers (0–255), an Integer n (n zero bytes), a String
// (encoded with `enc`, default utf-8), or another Bytes (copied).
inline Handle makeBytes(KiritoVM& vm, Handle x, const std::string& enc = "utf-8") {
    Object& o = vm.arena().deref(x);
    if (const auto* b = dynamic_cast<const BytesVal*>(&o))
        return vm.alloc(std::make_unique<BytesVal>(b->data));
    if (o.kind() == ValueKind::String)
        return vm.alloc(std::make_unique<BytesVal>(bytesutil::encode(static_cast<const StrVal&>(o).value(), enc)));
    if (o.kind() == ValueKind::Integer) {
        int64_t n = static_cast<const IntVal&>(o).value();
        if (n < 0) throw KiritoError("negative count");
        if (static_cast<uint64_t>(n) > 256ull * 1024 * 1024) throw KiritoError("Bytes too large");
        return vm.alloc(std::make_unique<BytesVal>(std::string(static_cast<std::size_t>(n), '\0')));
    }
    auto it = o.iterate(vm);
    if (!it) throw KiritoError("Bytes() expects a List of Integers, an Integer, a String, or Bytes");
    std::string out;
    out.reserve(it->size());
    for (Handle h : *it) {
        const Object& e = vm.arena().deref(h);
        if (e.kind() != ValueKind::Integer) throw KiritoError("Bytes() list elements must be Integers");
        int64_t v = static_cast<const IntVal&>(e).value();
        if (v < 0 || v > 255) throw KiritoError("Bytes() element out of range (0..255)");
        out += static_cast<char>(static_cast<unsigned char>(v));
    }
    return vm.alloc(std::make_unique<BytesVal>(std::move(out)));
}

}  // namespace kirito

#endif
