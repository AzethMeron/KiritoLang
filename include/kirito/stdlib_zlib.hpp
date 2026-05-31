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
        m.fn("compress", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.makeString(deflate::zlibCompress(bytesOf(vm, a[0], "compress")));
        });
        m.fn("decompress", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            try {
                return vm.makeString(deflate::zlibDecompress(bytesOf(vm, a[0], "decompress")));
            } catch (const deflate::DeflateError& e) {
                throw KiritoError(std::string("zlib: ") + e.what());
            }
        });
        // Raw DEFLATE (no zlib header/trailer) — symmetric pair.
        m.fn("deflate", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.makeString(deflate::compress(bytesOf(vm, a[0], "deflate")));
        });
        m.fn("inflate", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            try {
                return vm.makeString(deflate::inflate(bytesOf(vm, a[0], "inflate")));
            } catch (const deflate::DeflateError& e) {
                throw KiritoError(std::string("zlib: ") + e.what());
            }
        });
        m.fn("adler32", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.makeInt(static_cast<int64_t>(deflate::adler32(bytesOf(vm, a[0], "adler32"))));
        });
    }

private:
    static const std::string& bytesOf(KiritoVM& vm, Handle h, const char* who) { return argString(vm, h, who); }
};

}  // namespace kirito

#endif
