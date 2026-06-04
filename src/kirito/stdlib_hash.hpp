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
        m.fn("md5", {{"data", "String"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, hashing::md5(Args(vm, a, "md5")[0].asString("md5")));
        });
        m.fn("sha1", {{"data", "String"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, hashing::sha1(Args(vm, a, "sha1")[0].asString("sha1")));
        });
        m.fn("sha256", {{"data", "String"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, hashing::sha256(Args(vm, a, "sha256")[0].asString("sha256")));
        });
    }
};

}  // namespace kirito

#endif
