#ifndef KIRITO_STDLIB_MATH_HPP
#define KIRITO_STDLIB_MATH_HPP

#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <string>

#include "builtins.hpp"
#include "native.hpp"

namespace kirito {

// Read a numeric argument (Integer or Float) as a double.
inline double mathNum(KiritoVM& vm, Handle h) {
    const Object& o = vm.arena().deref(h);
    if (o.kind() == ValueKind::Integer) return static_cast<double>(static_cast<const IntVal&>(o).value());
    if (o.kind() == ValueKind::Float) return static_cast<const FloatVal&>(o).value();
    throw KiritoError("math expected a number, got '" + o.typeName() + "'");
}

// The `math` standard module: constants and the usual functions. Unary functions return Float;
// floor/ceil/factorial/gcd return Integer.
class MathModule : public NativeModule {
public:
    std::string name() const override { return "math"; }

    void setup(ModuleBuilder& m) override {
        KiritoVM& vm = m.vm();
        m.value("pi", vm.makeFloat(std::numbers::pi));
        m.value("e", vm.makeFloat(std::numbers::e));
        m.value("tau", vm.makeFloat(2.0 * std::numbers::pi));
        m.value("inf", vm.makeFloat(HUGE_VAL));
        m.value("nan", vm.makeFloat(std::nan("")));

        auto unary = [&](const char* nm, double (*f)(double)) {
            m.fn(nm, [f](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                if (a.size() != 1) throw KiritoError("math function expected 1 argument");
                return vm.makeFloat(f(mathNum(vm, a[0])));
            });
        };
        unary("sqrt", std::sqrt);
        unary("sin", std::sin);
        unary("cos", std::cos);
        unary("tan", std::tan);
        unary("asin", std::asin);
        unary("acos", std::acos);
        unary("atan", std::atan);
        unary("exp", std::exp);
        unary("log2", std::log2);
        unary("log10", std::log10);

        m.fn("log", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.size() == 1) return vm.makeFloat(std::log(mathNum(vm, a[0])));
            if (a.size() == 2) return vm.makeFloat(std::log(mathNum(vm, a[0])) / std::log(mathNum(vm, a[1])));
            throw KiritoError("log expected 1 or 2 arguments");
        });
        m.fn("pow", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.size() != 2) throw KiritoError("pow expected 2 arguments");
            return vm.makeFloat(std::pow(mathNum(vm, a[0]), mathNum(vm, a[1])));
        });
        m.fn("atan2", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.size() != 2) throw KiritoError("atan2 expected 2 arguments");
            return vm.makeFloat(std::atan2(mathNum(vm, a[0]), mathNum(vm, a[1])));
        });
        m.fn("hypot", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.size() != 2) throw KiritoError("hypot expected 2 arguments");
            return vm.makeFloat(std::hypot(mathNum(vm, a[0]), mathNum(vm, a[1])));
        });
        m.fn("fabs", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.makeFloat(std::fabs(mathNum(vm, a[0])));
        });
        m.fn("degrees", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.makeFloat(mathNum(vm, a[0]) * 180.0 / std::numbers::pi);
        });
        m.fn("radians", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.makeFloat(mathNum(vm, a[0]) * std::numbers::pi / 180.0);
        });
        m.fn("floor", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.makeInt(static_cast<int64_t>(std::floor(mathNum(vm, a[0]))));
        });
        m.fn("ceil", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.makeInt(static_cast<int64_t>(std::ceil(mathNum(vm, a[0]))));
        });
        m.fn("factorial", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const Object& o = vm.arena().deref(a[0]);
            if (o.kind() != ValueKind::Integer) throw KiritoError("factorial expects an Integer");
            int64_t n = static_cast<const IntVal&>(o).value();
            if (n < 0) throw KiritoError("factorial is not defined for negatives");
            int64_t r = 1;
            for (int64_t i = 2; i <= n; ++i) r *= i;
            return vm.makeInt(r);
        });
        m.fn("gcd", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto get = [&](Handle h) {
                const Object& o = vm.arena().deref(h);
                if (o.kind() != ValueKind::Integer) throw KiritoError("gcd expects Integers");
                return static_cast<const IntVal&>(o).value();
            };
            int64_t x = std::abs(get(a[0])), y = std::abs(get(a[1]));
            while (y) { int64_t t = x % y; x = y; y = t; }
            return vm.makeInt(x);
        });
    }
};

}  // namespace kirito

#endif
