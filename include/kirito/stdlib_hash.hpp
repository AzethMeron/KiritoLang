#ifndef KIRITO_STDLIB_HASH_HPP
#define KIRITO_STDLIB_HASH_HPP

#include <span>
#include <string>

#include "builtins.hpp"
#include "hashing.hpp"
#include "native.hpp"

namespace kirito {

// The `hash` module: cryptographic-style digests of byte Strings. Includes MD5 and SHA-256 (the
// required minimum) plus SHA-1. Each returns the lowercase hex digest. Self-contained — no
// external dependency.
class HashModule : public NativeModule {
public:
    std::string name() const override { return "hash"; }
    void setup(ModuleBuilder& m) override {
        m.fn("md5", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.makeString(hashing::md5(bytesOf(vm, a[0], "md5")));
        });
        m.fn("sha1", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.makeString(hashing::sha1(bytesOf(vm, a[0], "sha1")));
        });
        m.fn("sha256", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.makeString(hashing::sha256(bytesOf(vm, a[0], "sha256")));
        });
    }

private:
    static const std::string& bytesOf(KiritoVM& vm, Handle h, const char* who) { return argString(vm, h, who); }
};

}  // namespace kirito

#endif
