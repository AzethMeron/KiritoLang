#ifndef KIRITO_STDLIB_RANDOM_HPP
#define KIRITO_STDLIB_RANDOM_HPP

#include <cstdint>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <vector>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"

namespace kirito {

// A random generator object. There is no global RNG: you create a Random (default seed from the
// system, or an explicit seed for reproducibility) and call methods on it.
class RandomState : public NativeClass<RandomState> {
public:
    static constexpr const char* kTypeName = "Random";
    std::mt19937_64 engine;

    RandomState() {
        std::random_device rd;
        engine.seed((static_cast<uint64_t>(rd()) << 32) ^ rd());
    }
    explicit RandomState(uint64_t seed) { engine.seed(seed); }

    static int64_t asInt(KiritoVM& vm, Handle h) { return argInt(vm, h, "argument"); }
    static double asNum(KiritoVM& vm, Handle h) {
        const Object& o = vm.arena().deref(h);
        if (o.kind() == ValueKind::Integer) return static_cast<double>(static_cast<const IntVal&>(o).value());
        if (o.kind() == ValueKind::Float) return static_cast<const FloatVal&>(o).value();
        throw KiritoError("expected a number");
    }

    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override {
        auto bind = [&](const char* nm, NativeFn fn) {
            return vm.alloc(std::make_unique<NativeFunction>(nm, std::move(fn), std::vector<Handle>{self}));
        };
        auto rng = [](KiritoVM& vm, Handle self) -> RandomState& {
            return static_cast<RandomState&>(vm.arena().deref(self));
        };
        if (name == "seed")
            return bind("seed", [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                rng(vm, self).engine.seed(static_cast<uint64_t>(asInt(vm, a[0])));
                return vm.none();
            });
        if (name == "random")
            return bind("random", [self, rng](KiritoVM& vm, std::span<const Handle>) -> Handle {
                return vm.makeFloat(std::uniform_real_distribution<double>(0.0, 1.0)(rng(vm, self).engine));
            });
        if (name == "uniform")
            return bind("uniform", [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                std::uniform_real_distribution<double> d(asNum(vm, a[0]), asNum(vm, a[1]));
                return vm.makeFloat(d(rng(vm, self).engine));
            });
        if (name == "gauss" || name == "normalvariate")
            return bind(std::string(name).c_str(), [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                double mu = a.size() > 0 ? asNum(vm, a[0]) : 0.0;
                double sigma = a.size() > 1 ? asNum(vm, a[1]) : 1.0;
                return vm.makeFloat(std::normal_distribution<double>(mu, sigma)(rng(vm, self).engine));
            });
        if (name == "expovariate")
            return bind("expovariate", [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                double lambda = a.size() > 0 ? asNum(vm, a[0]) : 1.0;
                if (lambda <= 0.0) throw KiritoError("expovariate: lambda must be positive");
                return vm.makeFloat(std::exponential_distribution<double>(lambda)(rng(vm, self).engine));
            });
        if (name == "randint")  // inclusive [a, b]
            return bind("randint", [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                int64_t lo = asInt(vm, a[0]), hi = asInt(vm, a[1]);
                if (lo > hi) throw KiritoError("randint: empty range");
                return vm.makeInt(std::uniform_int_distribution<int64_t>(lo, hi)(rng(vm, self).engine));
            });
        if (name == "randrange")  // [0, n)
            return bind("randrange", [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                int64_t n = asInt(vm, a[0]);
                if (n <= 0) throw KiritoError("randrange: argument must be positive");
                return vm.makeInt(std::uniform_int_distribution<int64_t>(0, n - 1)(rng(vm, self).engine));
            });
        if (name == "choice")
            return bind("choice", [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto items = vm.arena().deref(a[0]).iterate(vm);
                if (items.value().empty()) throw KiritoError("choice from empty sequence");
                std::size_t i = std::uniform_int_distribution<std::size_t>(0, items.value().size() - 1)(rng(vm, self).engine);
                return items.value()[i];
            });
        if (name == "shuffle")  // in place, requires a List
            return bind("shuffle", [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                Object& o = vm.arena().deref(a[0]);
                if (o.kind() != ValueKind::List) throw KiritoError("shuffle requires a List");
                auto& e = static_cast<ListVal&>(o).elems;
                for (std::size_t i = e.size(); i > 1; --i)
                    std::swap(e[i - 1], e[std::uniform_int_distribution<std::size_t>(0, i - 1)(rng(vm, self).engine)]);
                return vm.none();
            });
        if (name == "sample")  // k unique elements -> new List
            return bind("sample", [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto items = vm.arena().deref(a[0]).iterate(vm);
                std::vector<Handle> pool = items.value();
                int64_t k = asInt(vm, a[1]);
                if (k < 0 || static_cast<std::size_t>(k) > pool.size())
                    throw KiritoError("sample: k out of range");
                RootScope rs(vm);
                auto out = std::make_unique<ListVal>();
                for (int64_t picked = 0; picked < k; ++picked) {
                    std::size_t j = std::uniform_int_distribution<std::size_t>(picked, pool.size() - 1)(rng(vm, self).engine);
                    std::swap(pool[picked], pool[j]);
                    out->elems.push_back(rs.add(pool[picked]));
                }
                return vm.alloc(std::move(out));
            });
        return Object::getAttr(vm, self, name);
    }
};

class RandomModule : public NativeModule {
public:
    std::string name() const override { return "random"; }
    void setup(ModuleBuilder& m) override {
        m.fn("Random", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.empty()) return vm.alloc(std::make_unique<RandomState>());
            const Object& o = vm.arena().deref(a[0]);
            if (o.kind() != ValueKind::Integer) throw KiritoError("Random seed must be an Integer");
            return vm.alloc(std::make_unique<RandomState>(
                static_cast<uint64_t>(static_cast<const IntVal&>(o).value())));
        });
    }
};

}  // namespace kirito

#endif
