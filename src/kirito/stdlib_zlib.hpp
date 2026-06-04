#ifndef KIRITO_STDLIB_ZLIB_HPP
#define KIRITO_STDLIB_ZLIB_HPP

#include <span>
#include <string>

#include "builtins.hpp"
#include "deflate.hpp"
#include "native.hpp"

namespace kirito {

// The `zlib` module: compress/decompress byte Strings. Uses a self-contained DEFLATE/INFLATE
// implementation (no external dependency), producing standard zlib streams (RFC 1950) so the
// output interoperates with other zlib tools. Also exposes raw DEFLATE and an Adler-32 checksum.
class ZlibModule : public NativeModule {
public:
    std::string name() const override { return "zlib"; }
    void setup(ModuleBuilder& m) override {
        m.fn("compress", {{"data", "String"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, deflate::zlibCompress(Args(vm, a, "compress")[0].asString("compress")));
        });
        m.fn("decompress", {{"data", "String"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            try {
                return val(vm, deflate::zlibDecompress(Args(vm, a, "decompress")[0].asString("decompress")));
            } catch (const deflate::DeflateError& e) {
                throw KiritoError(std::string("zlib: ") + e.what());
            }
        });
        // Raw DEFLATE (no zlib header/trailer) — symmetric pair.
        m.fn("deflate", {{"data", "String"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, deflate::compress(Args(vm, a, "deflate")[0].asString("deflate")));
        });
        m.fn("inflate", {{"data", "String"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            try {
                return val(vm, deflate::inflate(Args(vm, a, "inflate")[0].asString("inflate")));
            } catch (const deflate::DeflateError& e) {
                throw KiritoError(std::string("zlib: ") + e.what());
            }
        });
        m.fn("adler32", {{"data", "String"}}, "Integer", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, static_cast<int64_t>(deflate::adler32(Args(vm, a, "adler32")[0].asString("adler32"))));
        });
    }
};

}  // namespace kirito

#endif
