// Demonstrates embedding Kirito in a C++ program: registering a custom module and a custom
// native object type, driving them from Kirito source, and reading values back.
#include <cmath>
#include <memory>
#include <span>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static double num(KiritoVM& vm, Handle h) {
    const Object& o = vm.arena().deref(h);
    if (o.kind() == ValueKind::Integer) return static_cast<double>(static_cast<const IntVal&>(o).value());
    if (o.kind() == ValueKind::Float) return static_cast<const FloatVal&>(o).value();
    throw KiritoError("expected a number");
}

// A C++-authored module computing simple statistics over a Kirito iterable.
struct StatsModule : NativeModule {
    std::string name() const override { return "stats"; }
    void setup(ModuleBuilder& m) override {
        m.fn("mean", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto items = vm.arena().deref(a[0]).iterate(vm);
            double sum = 0;
            std::size_t n = 0;
            for (Handle h : items.value()) { sum += num(vm, h); ++n; }
            return vm.makeFloat(n ? sum / static_cast<double>(n) : 0.0);
        });
    }
};

// A C++-authored value type usable from Kirito like a built-in.
struct Vec2 : NativeClass<Vec2> {
    static constexpr const char* kTypeName = "Vec2";
    double x, y;
    Vec2(double x, double y) : x(x), y(y) {}
    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override {
        if (name == "x") return vm.makeFloat(x);
        if (name == "y") return vm.makeFloat(y);
        if (name == "length")
            return vm.alloc(std::make_unique<NativeFunction>(
                "length",
                [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
                    auto& v = static_cast<Vec2&>(vm.arena().deref(self));
                    return vm.makeFloat(std::sqrt(v.x * v.x + v.y * v.y));
                },
                std::vector<Handle>{self}));
        return Object::getAttr(vm, self, name);
    }
};

int main() {
    KiritoVM vm;
    vm.install<StatsModule>();
    vm.registerGlobal("Vec2", vm.alloc(std::make_unique<NativeFunction>(
        "Vec2", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.alloc(std::make_unique<Vec2>(num(vm, a[0]), num(vm, a[1])));
        })));

    // Kirito source drives the C++-registered module and type, indistinguishable from built-ins.
    CHECK(vm.stringify(vm.runSource("import(\"stats\").mean([2, 4, 6, 8])")) == "5.0");
    CHECK(vm.stringify(vm.runSource("var v = Vec2(3, 4)\nv.length()")) == "5.0");
    CHECK(vm.stringify(vm.runSource("Vec2(3, 4).x + Vec2(3, 4).y")) == "7.0");

    // The embedder reads a computed value back out.
    Handle r = vm.runSource("6 * 7");
    CHECK(vm.arena().deref(r).kind() == ValueKind::Integer);
    CHECK(static_cast<const IntVal&>(vm.arena().deref(r)).value() == 42);

    // A second VM is fully independent: it never saw the 'stats' module.
    KiritoVM vm2;
    CHECK_THROWS(vm2.runSource("import(\"stats\")"));

    return RUN_TESTS();
}
