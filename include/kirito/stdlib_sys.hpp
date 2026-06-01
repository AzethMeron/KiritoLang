#ifndef KIRITO_STDLIB_SYS_HPP
#define KIRITO_STDLIB_SYS_HPP

#include <cstdlib>
#include <cstring>
#include <span>
#include <string>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
extern "C" char** environ;
#endif

namespace kirito {

// The `sys` module: process environment and platform facilities. Environment access is portable
// (getenv plus a platform setter); no global Kirito state — everything goes through the VM.
class SysModule : public NativeModule {
public:
    std::string name() const override { return "sys"; }

    void setup(ModuleBuilder& m) override {
        KiritoVM& vm = m.vm();

        // platform name, as a value.
#if defined(_WIN32)
        m.value("platform", vm.makeString("windows"));
#elif defined(__APPLE__)
        m.value("platform", vm.makeString("darwin"));
#else
        m.value("platform", vm.makeString("linux"));
#endif

        m.fn("getenv", {{"name", "String"}, {"default", "", vm.none()}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            // getenv(name[, default]) -> String, or default/None if unset.
            Args args(vm, a, "getenv");
            const char* v = std::getenv(args[0].asString("getenv").c_str());
            if (v) return val(vm, v);
            return args.opt(1, none(vm));
        });

        m.fn("setenv", {{"name", "String"}, {"value", "String"}}, "", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "setenv");
            std::string name = args[0].asString("setenv");
            std::string value = args[1].asString("setenv");
#if defined(_WIN32)
            bool ok = ::SetEnvironmentVariableA(name.c_str(), value.c_str()) != 0;
            ::_putenv_s(name.c_str(), value.c_str());  // keep the CRT view (getenv) consistent too
#else
            bool ok = ::setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
            if (!ok) throw KiritoError("setenv failed for '" + name + "'");
            return none(vm);
        });

        m.fn("unsetenv", {{"name", "String"}}, "", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::string name = Args(vm, a, "unsetenv")[0].asString("unsetenv");
#if defined(_WIN32)
            ::SetEnvironmentVariableA(name.c_str(), nullptr);
            ::_putenv_s(name.c_str(), "");
#else
            ::unsetenv(name.c_str());
#endif
            return none(vm);
        });

        // environ() -> Dict of all environment variables.
        m.fn("environ", {}, "Dict", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            Dict d(vm);
#if defined(_WIN32)
            LPCH block = ::GetEnvironmentStringsA();
            if (block) {
                for (LPCH p = block; *p; p += std::strlen(p) + 1) {
                    std::string entry(p);
                    std::size_t eq = entry.find('=');
                    if (eq != std::string::npos && eq > 0) d.set(entry.substr(0, eq), val(vm, entry.substr(eq + 1)));
                }
                ::FreeEnvironmentStringsA(block);
            }
#else
            if (environ) {
                for (char** e = environ; *e; ++e) {
                    std::string entry(*e);
                    std::size_t eq = entry.find('=');
                    if (eq != std::string::npos) d.set(entry.substr(0, eq), val(vm, entry.substr(eq + 1)));
                }
            }
#endif
            return d.build();
        });

        m.fn("exit", {{"code", "Integer", vm.makeInt(0)}}, "", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "exit");
            int code = args.empty() || !args[0].isInt() ? 0 : static_cast<int>(args[0].asInt());
            std::exit(code);
            return none(vm);  // unreachable
        });
    }
};

}  // namespace kirito

#endif
