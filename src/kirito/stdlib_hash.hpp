#ifndef KIRITO_STDLIB_HASH_HPP
#define KIRITO_STDLIB_HASH_HPP

#include <cstdint>
#include <span>
#include <string>

#include "builtins.hpp"
#include "bytes.hpp"
#include "deflate.hpp"   // adler32 / crc32 checksums
#include "hashing.hpp"
#include "native.hpp"

namespace kirito {

// CRC-64/XZ (ECMA-182 polynomial, reflected) — the crc64 used by the .xz format. Table-free, no
// mutable global state. Returns the raw 64-bit value; as a signed Kirito Integer the top bit makes
// large values negative (Kirito ints are int64), which is still a stable, unique checksum.
inline uint64_t crc64(const std::string& data) {
    uint64_t crc = ~0ULL;
    for (unsigned char c : data) {
        crc ^= c;
        for (int k = 0; k < 8; ++k)
            crc = (crc & 1) ? (0xC96C5795D7870F42ULL ^ (crc >> 1)) : (crc >> 1);
    }
    return ~crc;
}

// The `hash` module: digests and checksums of byte data. Cryptographic-style digests MD5 / SHA-1 /
// SHA-256 (lowercase hex) and the fast non-cryptographic checksums Adler-32 / CRC-32 / CRC-64
// (Integers, as gzip/zlib/PNG/xz use). Every function accepts a String OR a Bytes (so binary data
// hashes correctly). Self-contained — no external dependency.
class HashModule : public NativeModule {
public:
    std::string name() const override { return "hash"; }
    void setup(ModuleBuilder& m) override {
        // Raw byte view of a String-or-Bytes argument.
        auto raw = [](KiritoVM& vm, Handle h, const char* who) -> const std::string& {
            Object& o = vm.arena().deref(h);
            if (o.kind() == ValueKind::String) return static_cast<StrVal&>(o).value();
            if (auto* b = dynamic_cast<BytesVal*>(&o)) return b->data;
            throw KiritoError(std::string(who) + " expects a String or Bytes");
        };
        auto digest = [&](const char* name, std::string (*fn)(const std::string&)) {
            m.fn(name, {{"data"}}, "String", [fn, raw, name](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                Args args(vm, a, name);
                return val(vm, fn(raw(vm, args[0].handle(), name)));
            });
        };
        digest("md5", hashing::md5);
        digest("sha1", hashing::sha1);
        digest("sha256", hashing::sha256);
        // 32-bit checksums (returned as a non-negative Integer).
        auto checksum32 = [&](const char* name, uint32_t (*fn)(const std::string&)) {
            m.fn(name, {{"data"}}, "Integer", [fn, raw, name](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                Args args(vm, a, name);
                return val(vm, static_cast<int64_t>(fn(raw(vm, args[0].handle(), name))));
            });
        };
        checksum32("adler32", deflate::adler32);
        checksum32("crc32", deflate::crc32);
        // crc64 — the 64-bit value reinterpreted as a signed Integer (top bit -> negative).
        m.fn("crc64", {{"data"}}, "Integer", [raw](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "crc64");
            return val(vm, static_cast<int64_t>(crc64(raw(vm, args[0].handle(), "crc64"))));
        });
    }
};

}  // namespace kirito

#endif
