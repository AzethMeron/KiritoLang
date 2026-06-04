#ifndef KIRITO_STDLIB_ZLIB_HPP
#define KIRITO_STDLIB_ZLIB_HPP

#include <cstdint>
#include <span>
#include <string>

#include "builtins.hpp"
#include "bytes.hpp"
#include "deflate.hpp"
#include "native.hpp"

namespace kirito {

// The `zlib` module: zlib-stream (RFC 1950) and raw DEFLATE (RFC 1951) compression. A self-contained
// DEFLATE/INFLATE (no external dependency), interoperable with standard zlib tools. The gzip
// *container* (RFC 1952) is its own thing — see the `gzip` module; the Adler-32 / CRC-32 checksums
// live in the `hash` module. Every function accepts a String OR a Bytes and returns a value of the
// same type as its input, so binary data (downloads, files) stays byte-correct via Bytes while text
// round-trips as a String.
class ZlibModule : public NativeModule {
public:
    std::string name() const override { return "zlib"; }
    void setup(ModuleBuilder& m) override {
        // Raw byte view of a String-or-Bytes argument, and a result wrapped to match the input type.
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
        // A codec function `data -> data` (same type), translating DeflateError to a clean KiritoError.
        auto codec = [&](const char* name, std::string (*fn)(const std::string&)) {
            m.fn(name, {{"data"}}, "", [fn, raw, wrap, name](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                Args args(vm, a, name);
                try {
                    return wrap(vm, args[0].handle(), fn(raw(vm, args[0].handle(), name)));
                } catch (const deflate::DeflateError& e) {
                    throw KiritoError(std::string("zlib: ") + e.what());
                }
            });
        };
        codec("compress", deflate::zlibCompress);
        codec("decompress", deflate::zlibDecompress);
        codec("deflate", deflate::compress);     // raw DEFLATE (no zlib header/trailer)
        codec("inflate", deflate::inflate);
    }
};

}  // namespace kirito

#endif
