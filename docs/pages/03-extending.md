# Extending Kirito from C++

Kirito's whole object model is one uniform `Object` protocol. Built-ins, your C++ types, and
Kirito-level classes all derive from the same base and dispatch through the same slots — so anything
you add is indistinguishable from a built-in to the evaluator. You extend Kirito by subclassing two
convenience bases and registering with one call.

## Adding a function

The lightest extension. A `NativeFunction` wraps a `std::function<Handle(KiritoVM&, std::span<const Handle>)>`:

```cpp
vm.registerGlobal("clamp", vm.arena().alloc(std::make_unique<NativeFunction>(
    "clamp", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        if (a.size() != 3) throw KiritoError("clamp expected 3 arguments");
        int64_t x  = static_cast<const IntVal&>(vm.arena().deref(a[0])).value();
        int64_t lo = static_cast<const IntVal&>(vm.arena().deref(a[1])).value();
        int64_t hi = static_cast<const IntVal&>(vm.arena().deref(a[2])).value();
        return vm.makeInt(std::max(lo, std::min(x, hi)));
    })));
```

## Adding a module

Subclass `NativeModule`, override `name()` and `setup()`, and register with `install<T>()`. Inside
`setup`, declare functions and values via the `ModuleBuilder`:

```cpp
class StatsModule : public NativeModule {
public:
    std::string name() const override { return "stats"; }
    void setup(ModuleBuilder& m) override {
        m.fn("clamp", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            /* ... */ return vm.none();
        });
        m.value("VERSION", vm_value);   // bind a constant
    }
};

// register once on the VM (e.g. just after construction):
vm.install<StatsModule>();
// Kirito: var s = import("stats")   then   s.clamp(...)
```

A third party adds their own module exactly the way the bundled stdlib does: `#include` the header,
call `install<T>()`, no global state.

## Adding a type

Subclass `NativeClass<Derived>` (a CRTP convenience that fills in sane defaults) and override only
the protocol slots your type needs:

| Slot | Triggered by |
|------|-------------|
| `binary(vm, op, self, rhs)` | `a + b`, `a < b`, ... |
| `unary(vm, op, self)` | `-a`, `not a` |
| `call(vm, args)` | `obj(...)` |
| `getAttr(vm, self, name)` / `setAttr(...)` | `obj.field` |
| `getItem(vm, keys)` / `setItem(...)` | `obj[i]`, `obj[i] = v` |
| `iterate(vm)` | `for x in obj` |
| `length(vm)` | `len(obj)` |
| `contains(vm, value)` | `x in obj` |
| `str(ctx)` / `equals(...)` / `hash()` | stringify / `==` / hashing |

Register a constructor name so `MyType(args)` works from Kirito. The bundled `matrix`, `io.open`,
`BytesIO`, and socket types are all `NativeClass`es authored this way — read `stdlib_matrix.hpp` for
a full worked example.

## Authoring a module in Kirito

If your module is pure logic (no new C++ primitives), it's often cleaner to write it in Kirito and
freeze the source into the binary:

```cpp
inline constexpr std::string_view mymod = R"KI(
var double = Function(x):
    return x * 2
)KI";

vm.registerSourceModule("mymod", mymod);
```

The source is compiled once per VM on first `import("mymod")`; its top-level `var`s become the
module's members (names starting with `_` stay private). The bundled `itertools`, `collections`,
`statistics`, etc. are built this way — see `stdlib_kimodules.hpp`.

## Design rules

- **No global mutable state** — everything is VM-scoped, so multiple VMs stay isolated.
- **Never expose raw pointers** across the boundary; hold `Handle`s, deref at point of use.
- **Header-only ODR**: everything `inline`/templated, `#ifndef` include guards (never `#pragma once`).
- Raise `KiritoError` with a clear, actionable message for bad arguments — errors are part of the
  language, not an afterthought.
