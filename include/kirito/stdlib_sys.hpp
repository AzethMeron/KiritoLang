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

        m.fn("getenv", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            // getenv(name[, default]) -> String, or default/None if unset.
            const std::string& name = asStr(vm, a[0], "getenv");
            const char* v = std::getenv(name.c_str());
            if (v) return vm.makeString(v);
            return a.size() > 1 ? a[1] : vm.none();
        });

        m.fn("setenv", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const std::string& name = asStr(vm, a[0], "setenv");
            const std::string& val = asStr(vm, a[1], "setenv");
#if defined(_WIN32)
            bool ok = ::SetEnvironmentVariableA(name.c_str(), val.c_str()) != 0;
            // keep the CRT view (getenv) consistent too
            ::_putenv_s(name.c_str(), val.c_str());
#else
            bool ok = ::setenv(name.c_str(), val.c_str(), 1) == 0;
#endif
            if (!ok) throw KiritoError("setenv failed for '" + name + "'");
            return vm.none();
        });

        m.fn("unsetenv", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const std::string& name = asStr(vm, a[0], "unsetenv");
#if defined(_WIN32)
            ::SetEnvironmentVariableA(name.c_str(), nullptr);
            ::_putenv_s(name.c_str(), "");
#else
            ::unsetenv(name.c_str());
#endif
            return vm.none();
        });

        // environ() -> Dict of all environment variables.
        m.fn("environ", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            RootScope rs(vm);
            auto dict = std::make_unique<DictVal>();
            Handle h = rs.add(vm.alloc(std::move(dict)));
            auto& d = static_cast<DictVal&>(vm.arena().deref(h));
#if defined(_WIN32)
            LPCH block = ::GetEnvironmentStringsA();
            if (block) {
                for (LPCH p = block; *p; p += std::strlen(p) + 1) {
                    std::string entry(p);
                    std::size_t eq = entry.find('=');
                    if (eq != std::string::npos && eq > 0)
                        d.set(vm.arena(), rs.add(vm.makeString(entry.substr(0, eq))),
                              rs.add(vm.makeString(entry.substr(eq + 1))));
                }
                ::FreeEnvironmentStringsA(block);
            }
#else
            if (environ) {
                for (char** e = environ; *e; ++e) {
                    std::string entry(*e);
                    std::size_t eq = entry.find('=');
                    if (eq != std::string::npos)
                        d.set(vm.arena(), rs.add(vm.makeString(entry.substr(0, eq))),
                              rs.add(vm.makeString(entry.substr(eq + 1))));
                }
            }
#endif
            return h;
        });

        m.fn("exit", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            int code = 0;
            if (!a.empty()) {
                const Object& o = vm.arena().deref(a[0]);
                if (o.kind() == ValueKind::Integer) code = static_cast<int>(static_cast<const IntVal&>(o).value());
            }
            std::exit(code);
            return vm.none();  // unreachable
        });
    }

private:
    static const std::string& asStr(KiritoVM& vm, Handle h, const char* who) { return argString(vm, h, who); }
};

}  // namespace kirito

#endif
