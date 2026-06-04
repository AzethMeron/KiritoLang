#ifndef KIRITO_STDLIB_RANDOM_HPP
#define KIRITO_STDLIB_RANDOM_HPP

#include <cstdint>
#include <memory>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"

namespace kirito {

// The native-binding idiom below re-uses `vm`/`self` as bound-method lambda parameters that
// intentionally shadow the enclosing getAttr/setup `vm`/`self` (same VM, by design). Silence
// -Wshadow for these mechanical bindings; it stays active in the evaluator/parser/lexer core.
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#endif

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

    std::vector<std::string> inspectMembers() const override {
        return {
            "random() -> Float", "uniform(a, b) -> Float", "randint(a, b) -> Integer",
            "randrange(start, stop, step) -> Integer", "choice(seq)", "shuffle(seq)",
            "sample(population, k) -> List", "seed(a)", "gauss(mu, sigma) -> Float",
            "normalvariate(mu, sigma) -> Float", "expovariate(lambd) -> Float",
        };
    }

    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override {
        auto bind = [&](const char* nm, std::vector<std::string> params, NativeFn fn) {
            return makeMethod(vm, nm, std::move(params), std::move(fn), std::vector<Handle>{self});
        };
        auto rng = [](KiritoVM& vm, Handle self) -> RandomState& {
            return static_cast<RandomState&>(vm.arena().deref(self));
        };
        if (name == "seed")
            return bind("seed", {"n"}, [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                rng(vm, self).engine.seed(static_cast<uint64_t>(asInt(vm, a[0])));
                return vm.none();
            });
        if (name == "random")
            return bind("random", {}, [self, rng](KiritoVM& vm, std::span<const Handle>) -> Handle {
                return vm.makeFloat(std::uniform_real_distribution<double>(0.0, 1.0)(rng(vm, self).engine));
            });
        if (name == "uniform")
            return bind("uniform", {"a", "b"}, [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                std::uniform_real_distribution<double> d(asNum(vm, a[0]), asNum(vm, a[1]));
                return vm.makeFloat(d(rng(vm, self).engine));
            });
        if (name == "gauss" || name == "normalvariate")
            return bind(std::string(name).c_str(), {"mu", "sigma"}, [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                double mu = a.size() > 0 ? asNum(vm, a[0]) : 0.0;
                double sigma = a.size() > 1 ? asNum(vm, a[1]) : 1.0;
                return vm.makeFloat(std::normal_distribution<double>(mu, sigma)(rng(vm, self).engine));
            });
        if (name == "expovariate")
            return bind("expovariate", {"lambd"}, [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                double lambda = a.size() > 0 ? asNum(vm, a[0]) : 1.0;
                if (lambda <= 0.0) throw KiritoError("expovariate: lambda must be positive");
                return vm.makeFloat(std::exponential_distribution<double>(lambda)(rng(vm, self).engine));
            });
        if (name == "randint")  // inclusive [a, b]
            return bind("randint", {"a", "b"}, [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                int64_t lo = asInt(vm, a[0]), hi = asInt(vm, a[1]);
                if (lo > hi) throw KiritoError("randint: empty range");
                return vm.makeInt(std::uniform_int_distribution<int64_t>(lo, hi)(rng(vm, self).engine));
            });
        if (name == "randrange")  // randrange(stop) | randrange(start, stop[, step]) — like range
            return bind("randrange", {"start", "stop", "step"}, [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto optInt = [&](Handle h, int64_t dflt) -> int64_t {
                    return vm.arena().deref(h).kind() == ValueKind::None ? dflt : asInt(vm, h);
                };
                int64_t start = 0, stop = 0, step = 1;
                if (a.size() == 1) stop = asInt(vm, a[0]);                       // randrange(stop)
                else if (a.size() == 2) { start = optInt(a[0], 0); stop = asInt(vm, a[1]); }
                else if (a.size() == 3) { start = optInt(a[0], 0); stop = asInt(vm, a[1]); step = optInt(a[2], 1); }
                else throw KiritoError("randrange expects 1 to 3 arguments");
                if (step == 0) throw KiritoError("randrange: step must not be zero");
                // Count the members of range(start, stop, step); pick one uniformly.
                int64_t count = step > 0 ? (stop > start ? (stop - start + step - 1) / step : 0)
                                         : (start > stop ? (start - stop + (-step) - 1) / (-step) : 0);
                if (count <= 0) throw KiritoError("randrange: empty range");
                int64_t k = std::uniform_int_distribution<int64_t>(0, count - 1)(rng(vm, self).engine);
                return vm.makeInt(start + k * step);
            });
        if (name == "choice")
            return bind("choice", {"seq"}, [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto items = vm.arena().deref(a[0]).iterate(vm);
                if (items.value().empty()) throw KiritoError("choice from empty sequence");
                std::size_t i = std::uniform_int_distribution<std::size_t>(0, items.value().size() - 1)(rng(vm, self).engine);
                return items.value()[i];
            });
        if (name == "shuffle")  // in place, requires a List
            return bind("shuffle", {"seq"}, [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                Object& o = vm.arena().deref(a[0]);
                if (o.kind() != ValueKind::List) throw KiritoError("shuffle requires a List");
                auto& e = static_cast<ListVal&>(o).elems;
                for (std::size_t i = e.size(); i > 1; --i)
                    std::swap(e[i - 1], e[std::uniform_int_distribution<std::size_t>(0, i - 1)(rng(vm, self).engine)]);
                return vm.none();
            });
        if (name == "sample")  // k unique elements -> new List
            return bind("sample", {"population", "k"}, [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
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
        // --- serialization (serialize / dump): the full Mersenne-Twister state, so a restored
        // generator continues the exact same stream (reproducible checkpoints, like Python's). The
        // standard engine's stream operators emit/parse its complete internal state. ---
        if (name == "_getstate_")
            return bind("_getstate_", {}, [self, rng](KiritoVM& vm, std::span<const Handle>) -> Handle {
                std::ostringstream os;
                os << rng(vm, self).engine;
                return vm.makeString(os.str());
            });
        if (name == "_setstate_")
            return bind("_setstate_", {"state"}, [self, rng](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                const Object& o = vm.arena().deref(a[0]);
                if (o.kind() != ValueKind::String)
                    throw KiritoError("Random _setstate_: expected the engine-state String");
                std::istringstream is(static_cast<const StrVal&>(o).value());
                if (!(is >> rng(vm, self).engine))
                    throw KiritoError("Random _setstate_: malformed engine state");
                return vm.none();
            });
        return Object::getAttr(vm, self, name);
    }
};

class RandomModule : public NativeModule {
public:
    std::string name() const override { return "random"; }
    void setup(ModuleBuilder& m) override {
        // Let serialize/dump reconstruct a Random: build a default one; _setstate_ restores its state.
        m.vm().registerDeserializer("Random", [](KiritoVM& vm, Handle) -> Handle {
            return vm.alloc(std::make_unique<RandomState>());
        });
        m.fn("Random", {{"seed", "", m.vm().none()}}, "Random", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "Random");
            if (args.empty() || args[0].isNone()) return vm.alloc(std::make_unique<RandomState>());
            return vm.alloc(std::make_unique<RandomState>(static_cast<uint64_t>(args[0].asInt("Random seed"))));
        });
    }
};

}  // namespace kirito

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
#endif
