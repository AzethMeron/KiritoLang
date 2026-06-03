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

// The native-binding idiom below re-uses `vm`/`self` as bound-method lambda parameters that
// intentionally shadow the enclosing getAttr/setup `vm`/`self` (same VM, by design). Silence
// -Wshadow for these mechanical bindings; it stays active in the evaluator/parser/lexer core.
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#endif

// Read a numeric argument (Integer or Float) as a double, via the ergonomic Value API.
inline double mathNum(KiritoVM& vm, Handle h) {
    return Value(vm, h).asFloat("math");
}

// Convert a double to int64 safely: casting a NaN/inf/out-of-range double to int64 is UB, so guard.
inline int64_t toInt64Checked(double d, const char* who) {
    if (std::isnan(d)) throw KiritoError(std::string(who) + ": cannot convert NaN to Integer");
    if (std::isinf(d)) throw KiritoError(std::string(who) + ": cannot convert infinity to Integer");
    if (d >= 9223372036854775808.0 || d < -9223372036854775808.0)
        throw KiritoError(std::string(who) + ": result out of Integer range");
    return static_cast<int64_t>(d);
}

// The `math` standard module: constants and the usual functions. Unary functions return Float;
// floor/ceil/factorial/gcd return Integer.
class MathModule : public NativeModule {
public:
    std::string name() const override { return "math"; }

    void setup(ModuleBuilder& m) override {
        KiritoVM& vm = m.vm();
        m.value("pi", val(vm, std::numbers::pi));
        m.value("e", val(vm, std::numbers::e));
        m.value("tau", val(vm, 2.0 * std::numbers::pi));
        m.value("inf", val(vm, HUGE_VAL));
        m.value("nan", val(vm, std::nan("")));

        auto unary = [&](const char* nm, double (*f)(double)) {
            m.fn(nm, {{"x", "Number"}}, "Float", [f](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                if (a.size() != 1) throw KiritoError("math function expected 1 argument");
                return val(vm, f(mathNum(vm, a[0])));
            });
        };
        unary("sqrt", std::sqrt);
        unary("cbrt", std::cbrt);
        unary("sin", std::sin);
        unary("cos", std::cos);
        unary("tan", std::tan);
        unary("asin", std::asin);
        unary("acos", std::acos);
        unary("atan", std::atan);
        unary("sinh", std::sinh);
        unary("cosh", std::cosh);
        unary("tanh", std::tanh);
        unary("asinh", std::asinh);
        unary("acosh", std::acosh);
        unary("atanh", std::atanh);
        unary("exp", std::exp);
        unary("expm1", std::expm1);
        unary("log1p", std::log1p);
        unary("log2", std::log2);
        unary("log10", std::log10);
        unary("trunc", std::trunc);
        unary("gamma", std::tgamma);
        unary("lgamma", std::lgamma);
        unary("erf", std::erf);
        unary("erfc", std::erfc);

        m.fn("isnan", {{"x", "Number"}}, "Bool", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, std::isnan(mathNum(vm, a[0])));
        });
        m.fn("isinf", {{"x", "Number"}}, "Bool", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, std::isinf(mathNum(vm, a[0])));
        });
        m.fn("isfinite", {{"x", "Number"}}, "Bool", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, std::isfinite(mathNum(vm, a[0])));
        });
        m.fn("copysign", {{"x", "Number"}, {"y", "Number"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, std::copysign(mathNum(vm, a[0]), mathNum(vm, a[1])));
        });
        m.fn("fmod", {{"x", "Number"}, {"y", "Number"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, std::fmod(mathNum(vm, a[0]), mathNum(vm, a[1])));
        });
        m.fn("lcm", {{"a", "Integer"}, {"b", "Integer"}}, "Integer", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto get = [&](Handle h) {
                const Object& o = vm.arena().deref(h);
                if (o.kind() != ValueKind::Integer) throw KiritoError("lcm expects Integers");
                return static_cast<const IntVal&>(o).value();
            };
            // Work in unsigned magnitudes: std::abs(INT64_MIN) is UB, and the product can overflow.
            auto mag = [](int64_t v) -> uint64_t {
                return v < 0 ? 0ull - static_cast<uint64_t>(v) : static_cast<uint64_t>(v);
            };
            uint64_t x = mag(get(a[0])), y = mag(get(a[1]));
            if (x == 0 || y == 0) return val(vm, 0);
            uint64_t g = x, h2 = y;
            while (h2) { uint64_t t = g % h2; g = h2; h2 = t; }
            uint64_t prod;
            if (__builtin_mul_overflow(x / g, y, &prod) || prod > static_cast<uint64_t>(INT64_MAX))
                throw KiritoError("lcm result too large for Integer");
            return val(vm, static_cast<int64_t>(prod));
        });

        m.fn("log", {{"x", "Number"}, {"base", "", vm.none()}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            // log(x) is the natural log; log(x, base) changes the base. A None base (the default) is
            // the natural log too.
            if (a.size() < 2 || vm.arena().deref(a[1]).kind() == ValueKind::None)
                return val(vm, std::log(mathNum(vm, a[0])));
            return val(vm, std::log(mathNum(vm, a[0])) / std::log(mathNum(vm, a[1])));
        });
        m.fn("pow", {{"x", "Number"}, {"y", "Number"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.size() != 2) throw KiritoError("pow expected 2 arguments");
            return val(vm, std::pow(mathNum(vm, a[0]), mathNum(vm, a[1])));
        });
        m.fn("atan2", {{"y", "Number"}, {"x", "Number"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.size() != 2) throw KiritoError("atan2 expected 2 arguments");
            return val(vm, std::atan2(mathNum(vm, a[0]), mathNum(vm, a[1])));
        });
        m.fn("hypot", {{"x", "Number"}, {"y", "Number"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.size() != 2) throw KiritoError("hypot expected 2 arguments");
            return val(vm, std::hypot(mathNum(vm, a[0]), mathNum(vm, a[1])));
        });
        m.fn("fabs", {{"x", "Number"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, std::fabs(mathNum(vm, a[0])));
        });
        m.fn("degrees", {{"x", "Number"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, mathNum(vm, a[0]) * 180.0 / std::numbers::pi);
        });
        m.fn("radians", {{"x", "Number"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, mathNum(vm, a[0]) * std::numbers::pi / 180.0);
        });
        m.fn("floor", {{"x", "Number"}}, "Integer", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, toInt64Checked(std::floor(mathNum(vm, a[0])), "floor"));
        });
        m.fn("ceil", {{"x", "Number"}}, "Integer", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, toInt64Checked(std::ceil(mathNum(vm, a[0])), "ceil"));
        });
        m.fn("factorial", {{"n", "Integer"}}, "Integer", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const Object& o = vm.arena().deref(a[0]);
            if (o.kind() != ValueKind::Integer) throw KiritoError("factorial expects an Integer");
            int64_t n = static_cast<const IntVal&>(o).value();
            if (n < 0) throw KiritoError("factorial is not defined for negatives");
            int64_t r = 1;
            for (int64_t i = 2; i <= n; ++i)
                if (__builtin_mul_overflow(r, i, &r))  // 21! already exceeds int64
                    throw KiritoError("factorial result too large for Integer");
            return val(vm, r);
        });
        m.fn("gcd", {{"a", "Integer"}, {"b", "Integer"}}, "Integer", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto get = [&](Handle h) {
                const Object& o = vm.arena().deref(h);
                if (o.kind() != ValueKind::Integer) throw KiritoError("gcd expects Integers");
                return static_cast<const IntVal&>(o).value();
            };
            auto mag = [](int64_t v) -> uint64_t {  // avoid std::abs(INT64_MIN) UB
                return v < 0 ? 0ull - static_cast<uint64_t>(v) : static_cast<uint64_t>(v);
            };
            uint64_t x = mag(get(a[0])), y = mag(get(a[1]));
            while (y) { uint64_t t = x % y; x = y; y = t; }
            if (x > static_cast<uint64_t>(INT64_MAX))
                throw KiritoError("gcd result too large for Integer");
            return val(vm, static_cast<int64_t>(x));
        });
        // prod(iterable[, start]): product of the elements (Integer if all Integer, else Float).
        m.fn("prod", {{"iterable"}, {"start", "", vm.makeInt(1)}}, "Number", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.empty()) throw KiritoError("prod expects an iterable");
            auto items = vm.arena().deref(a[0]).iterate(vm);
            if (!items) throw KiritoError("prod expects an iterable");
            bool isFloat = false;
            double f = 1.0;
            int64_t n = 1;
            if (a.size() > 1) {
                const Object& s = vm.arena().deref(a[1]);
                if (s.kind() == ValueKind::Float) { isFloat = true; f = static_cast<const FloatVal&>(s).value(); }
                else if (s.kind() == ValueKind::Integer) { n = static_cast<const IntVal&>(s).value(); f = static_cast<double>(n); }
                else throw KiritoError("prod start must be a number");
            }
            for (Handle h : items.value()) {
                const Object& o = vm.arena().deref(h);
                if (o.kind() == ValueKind::Float) { isFloat = true; f *= static_cast<const FloatVal&>(o).value(); }
                else if (o.kind() == ValueKind::Integer) { int64_t v = static_cast<const IntVal&>(o).value(); n *= v; f *= static_cast<double>(v); }
                else throw KiritoError("prod expects numbers");
            }
            return isFloat ? val(vm, f) : val(vm, n);
        });
        // comb(n, k) / perm(n, k): combinations / partial permutations, computed without overflow
        // for the common small cases (raises if the exact result exceeds int64).
        auto combPerm = [](KiritoVM& vm, std::span<const Handle> a, bool comb) -> Handle {
            auto geti = [&](Handle h, const char* w) {
                const Object& o = vm.arena().deref(h);
                if (o.kind() != ValueKind::Integer) throw KiritoError(std::string(w) + " expects Integers");
                return static_cast<const IntVal&>(o).value();
            };
            int64_t n = geti(a[0], comb ? "comb" : "perm");
            int64_t k = a.size() > 1 ? geti(a[1], comb ? "comb" : "perm") : n;
            if (n < 0 || k < 0) throw KiritoError("comb/perm require non-negative Integers");
            if (k > n) return val(vm, 0);
            if (comb && k > n - k) k = n - k;  // symmetry: fewer multiplications
            if (!comb) {
                // perm: n * (n-1) * ... * (n-k+1), with overflow checking.
                unsigned long long r = 1;
                for (int64_t i = 0; i < k; ++i)
                    if (__builtin_mul_overflow(r, static_cast<unsigned long long>(n - i), &r))
                        throw KiritoError("comb/perm result too large for Integer");
                if (r > static_cast<unsigned long long>(INT64_MAX))
                    throw KiritoError("comb/perm result too large for Integer");
                return val(vm, static_cast<int64_t>(r));
            }
            // comb: multiply then divide step-by-step so the running value stays the EXACT partial
            // binomial coefficient (always an integer) — avoids overflowing on the full numerator
            // for results that themselves fit in int64 (e.g. comb(30, 15) = 155117520).
            unsigned long long r = 1;
            for (int64_t i = 1; i <= k; ++i) {
                if (__builtin_mul_overflow(r, static_cast<unsigned long long>(n - k + i), &r))
                    throw KiritoError("comb/perm result too large for Integer");
                r /= static_cast<unsigned long long>(i);  // exact: r is C(n-k+i, i) * (k! / i!)... integer
            }
            if (r > static_cast<unsigned long long>(INT64_MAX))
                throw KiritoError("comb/perm result too large for Integer");
            return val(vm, static_cast<int64_t>(r));
        };
        m.fn("comb", {{"n", "Integer"}, {"k", "Integer"}}, "Integer", [combPerm](KiritoVM& vm, std::span<const Handle> a) { return combPerm(vm, a, true); });
        m.fn("perm", {{"n", "Integer"}, {"k", "Integer"}}, "Integer", [combPerm](KiritoVM& vm, std::span<const Handle> a) { return combPerm(vm, a, false); });
    }
};

}  // namespace kirito

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
#endif
