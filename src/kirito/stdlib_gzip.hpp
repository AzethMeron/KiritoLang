#ifndef KIRITO_STDLIB_GZIP_HPP
#define KIRITO_STDLIB_GZIP_HPP

#include <cstdint>
#include <span>
#include <string>

#include "builtins.hpp"
#include "bytes.hpp"
#include "deflate.hpp"
#include "native.hpp"

namespace kirito {

// The gzip container format (RFC 1952): a DEFLATE stream wrapped with a header (magic + flags) and a
// CRC-32 / ISIZE trailer. It is its own thing — `.gz` files, HTTP `Content-Encoding: gzip` — distinct
// from the bare zlib stream (RFC 1950) in the `zlib` module, so it lives in its own `gzip` module.
namespace gzipfmt {

// Wrap a DEFLATE body in the gzip container, byte-for-byte as `gzip(1)` writes it.
inline std::string compress(const std::string& data) {
    std::string out;
    auto byte = [&](unsigned v) { out += static_cast<char>(static_cast<unsigned char>(v)); };
    byte(0x1f); byte(0x8b); byte(0x08);          // magic, CM = deflate
    byte(0x00);                                  // FLG = 0
    for (int i = 0; i < 4; ++i) byte(0x00);      // MTIME = 0
    byte(0x00); byte(0xff);                      // XFL, OS = unknown
    out += deflate::compress(data);              // raw DEFLATE body
    uint32_t crc = deflate::crc32(data), isize = static_cast<uint32_t>(data.size());
    for (int i = 0; i < 4; ++i) byte((crc >> (8 * i)) & 0xff);
    for (int i = 0; i < 4; ++i) byte((isize >> (8 * i)) & 0xff);
    return out;
}

// Parse a gzip stream: validate the header, skip the optional FEXTRA/FNAME/FCOMMENT/FHCRC fields per
// FLG, INFLATE the body, and verify the CRC-32 trailer.
inline std::string decompress(const std::string& data) {
    auto b = [&](std::size_t i) -> unsigned { return static_cast<unsigned char>(data[i]); };
    if (data.size() < 18) throw deflate::DeflateError("gzip: stream too short");
    if (b(0) != 0x1f || b(1) != 0x8b) throw deflate::DeflateError("gzip: bad magic (not a gzip stream)");
    if (b(2) != 0x08) throw deflate::DeflateError("gzip: unsupported compression method");
    unsigned flg = b(3);
    std::size_t pos = 10;                        // fixed header: magic, CM, FLG, MTIME, XFL, OS
    if (flg & 0x04) {                            // FEXTRA: 2-byte length, then that many bytes
        if (pos + 2 > data.size()) throw deflate::DeflateError("gzip: truncated FEXTRA");
        std::size_t xlen = b(pos) | (b(pos + 1) << 8);
        pos += 2 + xlen;
    }
    if (flg & 0x08) { while (pos < data.size() && data[pos] != 0) ++pos; ++pos; }  // FNAME (NUL-terminated)
    if (flg & 0x10) { while (pos < data.size() && data[pos] != 0) ++pos; ++pos; }  // FCOMMENT
    if (flg & 0x02) pos += 2;                    // FHCRC
    if (pos + 8 > data.size()) throw deflate::DeflateError("gzip: truncated header/stream");
    std::string out = deflate::inflate(data.substr(pos, data.size() - pos - 8));
    std::size_t t = data.size() - 8;
    uint32_t want = b(t) | (b(t + 1) << 8) | (b(t + 2) << 16) | (static_cast<uint32_t>(b(t + 3)) << 24);
    if (deflate::crc32(out) != want) throw deflate::DeflateError("gzip: CRC-32 mismatch (corrupt stream)");
    return out;
}

}  // namespace gzipfmt

// The `gzip` module: gzip-container compress/decompress. Each function accepts a String OR a Bytes
// and returns the same type as its input, so binary data (downloads, `.gz` files) stays byte-correct
// via Bytes while text round-trips as a String.
class GzipModule : public NativeModule {
public:
    std::string name() const override { return "gzip"; }
    void setup(ModuleBuilder& m) override {
        auto raw = [](KiritoVM& vm, Handle h, const char* who) -> const std::string& {
            Object& o = vm.arena().deref(h);
            if (o.kind() == ValueKind::String) return static_cast<StrVal&>(o).value();
            if (auto* b = dynamic_cast<BytesVal*>(&o)) return b->data;
            throw KiritoError(std::string(who) + " expects a String or Bytes");
        };
        auto wrap = [](KiritoVM& vm, Handle in, std::string out) -> Handle {
            if (dynamic_cast<BytesVal*>(&vm.arena().deref(in)))
                return vm.alloc(std::make_unique<BytesVal>(std::move(out)));
            return vm.makeString(std::move(out));
        };
        auto codec = [&](const char* nm, std::string (*fn)(const std::string&)) {
            m.fn(nm, {{"data"}}, "", [fn, raw, wrap, nm](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                Args args(vm, a, nm);
                try {
                    return wrap(vm, args[0].handle(), fn(raw(vm, args[0].handle(), nm)));
                } catch (const deflate::DeflateError& e) {
                    std::string msg = e.what();  // the gzip decode messages already carry the prefix
                    throw KiritoError(msg.rfind("gzip:", 0) == 0 ? msg : "gzip: " + msg);
                }
            });
        };
        codec("compress", gzipfmt::compress);
        codec("decompress", gzipfmt::decompress);
        m.alias("gzip", "compress");             // familiar verb aliases
        m.alias("gunzip", "decompress");
    }
};

}  // namespace kirito

#endif
