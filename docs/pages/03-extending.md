# Extending Kirito from C++

Kirito's whole object model is one uniform `Object` protocol. Built-ins, your C++ types, and
Kirito-level classes all derive from the same base and dispatch through the same slots — so anything
you add is indistinguishable from a built-in to the evaluator. You extend Kirito by subclassing two
convenience bases and registering with one call.

> **Reach for the built-in types first.** Returning Integers, Floats, Strings, Bools, `None`, Lists,
> Dicts, and Sets covers the vast majority of modules. Defining a *new* `NativeClass` is the fallback
> — only for genuinely new behaviour (a file handle, a socket, a matrix). The `Value` API below makes
> working with the built-in types the easy, default path.

## Working with built-in types — the `Value` API

`#include "kirito.hpp"` brings in `value.hpp`. Three facts about how the evaluator calls a native
function make every line of the example below concrete:

- **A native function has the signature `Handle fn(KiritoVM& vm, std::span<const Handle> raw)`.**
  `vm` is the interpreter this call belongs to. `raw` is the list of *positional argument handles*
  the caller passed — for `demo(10, 20, [1, 2])`, `raw` holds three entries. The function returns one
  `Handle`: the result value.
- **A `Handle` is an opaque reference** (an arena slot + generation), *not* a value you can read
  directly — Kirito owns the actual object. You reach the object through the VM, and the `Value`
  facade (`Value{vm, handle}`) does exactly that: it lets you ask a handle's type, read it as a C++
  number/string, index it, or iterate it.
- **You create new values with `val(vm, x)`** (for `Integer`/`Float`/`String`/`Bool`) and the
  `List`/`Dict`/`Set` builders (for collections). Each yields a `Handle` you can return. `Value`
  converts implicitly to and from `Handle`, so these all interoperate.

```cpp
using namespace kirito;

// demo(a: Integer, b: Integer, items: List) -> Dict   (returns {"sum", "items", "ok"})
NativeFn demo = [](KiritoVM& vm, std::span<const Handle> raw) -> Handle {
    Args a(vm, raw, "demo");          // wrap the raw arg handles; "demo" labels this fn in errors
    int64_t x = a.at(0).asInt("a");   // 1st arg, required, must be an Integer
    int64_t y = a.at(1).asInt("b");   // 2nd arg, required, must be an Integer

    List items(vm);                   // a GC-safe List builder (roots its contents while building)
    for (Value e : a.at(2).items())   // a.at(2) is the 3rd arg; .items() iterates any iterable
        items.add(e);                 //   copy each element into the new list
    items.add(x + y);                 //   then append the sum

    return Dict(vm)                   // build the result Dict, fluent style
        .set("sum", x + y)            //   "sum"   -> an Integer (val() is applied for you)
        .set("items", items.build())  //   "items" -> the List we just built
        .set("ok", true)              //   "ok"    -> a Bool
        .build();                     // finalize -> Handle (Dict converts implicitly)
};
```

**Line by line:**

- `Args a(vm, raw, "demo")` — wraps the raw handle span in a small checked view. The string `"demo"`
  is cosmetic: it is the function name spliced into argument-error messages.
- `a.at(0)` — returns the first argument **as a `Value`**, bounds-checked. If the caller passed too
  few arguments it throws *"demo missing argument 1"*. (`a[0]` is the same read but *unchecked* — use
  it only after testing `a.size()`; `a.opt(i, dflt)` substitutes a default when the argument is
  absent.)
- `.asInt("a")` — reads that `Value` as an `int64_t`, requiring an `Integer`; on a mismatch it throws
  *"a expected Integer, got 'Float'"*. You pass the **parameter's name** as the label so the message
  points at the offending argument. `asFloat` / `asString` / `asBool` behave the same (`asFloat` also
  accepts an `Integer`).
- `.items()` — turns any iterable argument (List, Set, String, Dict, …) into a `std::vector<Value>`
  you can range over; it throws if the argument isn't iterable.
- The `List` / `Dict` / `Set` builders own a GC root scope, so an allocation midway through
  construction can never collect the half-built value. `.set(key, v)` takes a C++ primitive directly
  (calling `val()` for you) or a `Handle`; `.build()` returns the finished `Handle`.

**Reading a `Value`:** `v.asInt()` / `asFloat()` (accepts Integer or Float) / `asString()` /
`asBool()`; the type tests `v.isInt()` / `isString()` / `isList()` / …; `v.len()`, `v.at(i)` (list
index, negative counts from the end), `v.items()` (iterate); and for Dicts `v.get(key)`,
`v.get(key, default)`, `v.has(key)`, `v.pairs()`.
**Building a value:** `val(vm, 42)`, `val(vm, 3.14)`, `val(vm, "hi")`, `val(vm, true)`, `none(vm)`,
`makeList(vm, {h1, h2})`, and the `List` / `Dict` / `Set` builders.

Every bundled stdlib module — `io`, `math`, `random`, `matrix`, `complex`, `json`, `serialize`,
`dump`, `net`, `sys`, `time`, `zlib`, `hash` — is authored against this `Value` API, so their headers
double as worked examples of idiomatic native code.

## Adding a function

The lightest extension. A `NativeFunction` wraps a `std::function<Handle(KiritoVM&, std::span<const Handle>)>`.
Reach through `Args` + `Value` so a bad argument becomes a clear `KiritoError` instead of a cast crash:

```cpp
vm.registerGlobal("clamp", vm.arena().alloc(std::make_unique<NativeFunction>(
    "clamp", [](KiritoVM& vm, std::span<const Handle> raw) -> Handle {
        Args a(vm, raw, "clamp");                       // checks arity on access, labels errors
        int64_t x  = a.at(0).asInt("x");
        int64_t lo = a.at(1).asInt("lo");
        int64_t hi = a.at(2).asInt("hi");
        return val(vm, std::max(lo, std::min(x, hi)));
    })));
```

### Declaring a signature (keyword arguments + `inspect`)

Give a native function a signature and it gains exactly what Kirito functions have: callers may pass
**keyword arguments** and rely on **defaults**, and `inspect` shows its parameters, types, and return
type. The implementation still receives a flat positional `span` — the runtime binds keywords and
fills defaults before calling it. Construct params as `{"name"}`, `{"name", "Type"}`, or
`{"name", "Type", defaultHandle}`:

```cpp
vm.registerGlobal("greet", vm.arena().alloc(std::make_unique<NativeFunction>(
    "greet",
    std::vector<NativeParam>{{"name", "String"}, {"loud", "Bool", vm.makeBool(false)}},
    "String",
    [](KiritoVM& vm, std::span<const Handle> raw) -> Handle {
        Args a(vm, raw, "greet");
        // a[0] is `name`; a[1] is `loud`, defaulted to False when the caller omits it.
        std::string msg = "Hello, " + a.at(0).asString("name");
        return val(vm, a.at(1).asBool("loud") ? msg + "!" : msg);
    })));
// Kirito:  greet("Ada")  /  greet(name="Ada", loud=True)  /  inspect(greet)
```

Inside a module's `setup`, the `ModuleBuilder` has the same overload:
`m.fn("open", {{"path", "String"}, {"mode", "String", m.vm().makeString("r")}}, "File", impl);`.

## Adding a module

Subclass `NativeModule`, override `name()` and `setup()`, and register the whole thing with one
`install<T>()`. Inside `setup`, declare members through the `ModuleBuilder` — `fn` for functions,
`value` for constants. This is a complete `stats` module (it is also the embedding integration test,
`tools/tests/integration/embed_demo.cpp`, so it is guaranteed to compile and run):

```cpp
struct StatsModule : NativeModule {
    std::string name() const override { return "stats"; }

    void setup(ModuleBuilder& m) override {
        // mean(xs: List) -> Float — iterate any iterable, read each element as a number.
        m.fn("mean", {{"xs", "List"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "mean");
            double sum = 0; int64_t n = 0;
            for (Value x : args.at(0).items()) { sum += x.asFloat("mean element"); ++n; }
            if (n == 0) throw KiritoError("mean of empty list");
            return val(vm, sum / static_cast<double>(n));
        });

        // clamp(x, lo, hi) -> Integer — signatured, so it accepts keyword arguments and defaults.
        m.fn("clamp", {{"x", "Integer"}, {"lo", "Integer"}, {"hi", "Integer"}}, "Integer",
             [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                 Args args(vm, a, "clamp");
                 int64_t x = args.at(0).asInt("x"), lo = args.at(1).asInt("lo"), hi = args.at(2).asInt("hi");
                 return val(vm, std::max(lo, std::min(x, hi)));
             });

        m.value("VERSION", val(m.vm(), "1.0"));   // a plain constant member
    }
};

vm.install<StatsModule>();   // register once; then import("stats") works from Kirito
```

<!--norun (uses the stats module defined in C++ above)-->
```kirito
var stats = import("stats")
stats.mean([2, 4, 6, 8])      # 5.0
stats.clamp(12, lo=0, hi=9)   # 9   — keyword args come straight from the signature
stats.VERSION                 # "1.0"
inspect(stats)                # lists mean/clamp with their parameter types and returns
```

A third party adds a module exactly the way the bundled stdlib does: `#include` the header, call
`install<T>()`, no global state.

## Adding a type

When returning built-in values isn't enough — you need a value with its own identity, methods, and
operators — subclass `NativeClass<Derived>`. The CRTP base fills in `kind`/`typeName`/`truthy`/
`equals`; you define `static constexpr const char* kTypeName` and override only the protocol slots
your type uses. Here is a complete 2-D vector — attributes, methods, an overloaded `+`, and a custom
`str` (also verified in `tools/tests/integration/embed_demo.cpp`):

```cpp
struct Vec2 : NativeClass<Vec2> {
    static constexpr const char* kTypeName = "Vec2";
    double x, y;
    Vec2(double x, double y) : x(x), y(y) {}

    std::string str(StringifyCtx&) const override {        // how print / str(v) show it
        return "Vec2(" + std::to_string(x) + ", " + std::to_string(y) + ")";
    }

    // Attribute reads return values directly; a method name returns a callable with `self` bound, so
    // v.length() / v.dot(o) arrive with the receiver already in hand. Build methods with `makeMethod`
    // (see below): name them parameters and they accept keyword arguments — v.dot(other = o) — exactly
    // like a Kirito method. The impl still receives a plain positional `span`.
    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override {
        if (name == "x") return val(vm, x);
        if (name == "y") return val(vm, y);
        if (name == "length")
            return makeMethod(vm, "length", {},          // no params
                [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
                    auto& v = static_cast<Vec2&>(vm.arena().deref(self));
                    return val(vm, std::sqrt(v.x * v.x + v.y * v.y));
                }, std::vector<Handle>{self});            // <-- bind self into the method
        if (name == "dot")
            return makeMethod(vm, "dot", {"other"},       // v.dot(o) or v.dot(other = o)
                [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                    Args args(vm, a, "dot");
                    auto& v = static_cast<Vec2&>(vm.arena().deref(self));
                    auto* o = dynamic_cast<const Vec2*>(&vm.arena().deref(args.at(0)));
                    if (!o) throw KiritoError("dot expects a Vec2");
                    return val(vm, v.x * o->x + v.y * o->y);
                }, std::vector<Handle>{self});
        return Object::getAttr(vm, self, name);            // unknown attr -> a clear error
    }

    // Operator overloading: Vec2 + Vec2 -> Vec2; anything else falls back to the default error.
    Handle binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) override {
        if (op == BinOp::Add)
            if (auto* o = dynamic_cast<const Vec2*>(&vm.arena().deref(rhs)))
                return vm.alloc(std::make_unique<Vec2>(x + o->x, y + o->y));
        return Object::binary(vm, op, self, rhs);
    }
};

// A constructor name so `Vec2(x, y)` builds one from Kirito (signatured -> keyword args + inspect).
vm.registerGlobal("Vec2", vm.alloc(std::make_unique<NativeFunction>(
    "Vec2", std::vector<NativeParam>{{"x", "Number"}, {"y", "Number"}}, "Vec2",
    [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        Args args(vm, a, "Vec2");
        return vm.alloc(std::make_unique<Vec2>(args.at(0).asFloat("x"), args.at(1).asFloat("y")));
    })));
```

<!--norun (uses the Vec2 native type defined in C++ above)-->
```kirito
var a = Vec2(3, 4)
a.x                       # 3.0
a.length()                # 5.0
a.dot(Vec2(1, 0))         # 3.0
a.dot(other = Vec2(1, 0)) # 3.0   — methods built with makeMethod accept keyword arguments
String(a + Vec2(1, 1))    # "Vec2(4, 5)"
```

`vm.alloc(std::make_unique<T>(...))` boxes any `Object` (a built-in *or* a `NativeClass`) into the
arena and returns a `Handle`.

### Methods that accept keyword arguments — `makeMethod`

`makeMethod(vm, name, {param names…}, impl, {self})` is the one helper you need for member functions.
It binds `self` (so the receiver arrives in the closure) **and** gives the method **keyword-argument
support**: positional arguments fill the named slots left-to-right, keywords bind by name, and the
impl still receives the same flat positional `span` it always would — so you write the body once and
get `obj.method(a, b)` *and* `obj.method(b = …, a = …)` for free. Naming a slot `{}` (no params) makes
a nullary method that cleanly rejects any stray keyword. This is exactly how every built-in type
method (`xs.sort(key = f, reverse = True)`, `s.split(sep = ",")`) and every stdlib native-class method
gains keyword arguments; prefer it over a raw `NativeFunction` for anything reachable as `obj.method`.
(For a free function or a module function, declare a [signature](#declaring-a-signature-keyword-arguments-inspect)
instead — that additionally surfaces the parameters in `inspect`.)

### The protocol slots

Override only what your type supports; every slot defaults to a clear "unsupported" `KiritoError`.

| Slot | Triggered by |
|------|-------------|
| `binary(vm, op, self, rhs)` | `a + b`, `a < b`, … (`BinOp::Add/Sub/Mul/Div/FloorDiv/Mod/Pow/Eq/Ne/Lt/Le/Gt/Ge`) |
| `unary(vm, op, self)` | `-a`, `not a` (`UnOp::Neg/Not`) |
| `call(vm, args)` | `obj(...)` — makes the value itself callable |
| `getAttr(vm, self, name)` | `obj.field` |
| `setAttr(vm, name, value)` | `obj.field = v` |
| `getItem(vm, keys)` | `obj[i]` / `obj[i, j]` (keys are variadic) |
| `setItem(vm, keys, value)` | `obj[i] = v` / `obj[i, j] = v` (keys are variadic) |
| `slice(vm, start, stop, step)` | `obj[a:b:c]` — a dedicated slice slot (each bound may be `None`) |
| `iterate(vm)` | `for x in obj` — return the elements as a vector of `Handle` |
| `length(vm)` | `len(obj)` |
| `contains(vm, value)` | `x in obj` |
| `str(ctx)` | stringify (`String(obj)`, printing, nesting) |
| `equals(arena, other)` | `==` |
| `hash()` | use as a Set element / Dict key |
| `children(out)` | GC reachability — push every `Handle` your object owns |

If your type holds Kirito values (e.g. a container), implement `children()` so the garbage collector
can trace them. The bundled `matrix`, `io.open` (File), `BytesIO`, `Random`, and socket types are all
`NativeClass`es — read `stdlib_matrix.hpp` or `stdlib_random.hpp` for production examples.

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

## Cross-VM objects (the dispatcher)

A native type can be made to cross between worker VMs **by identity** — the way `parallel.Queue` is
shared, where each VM holds a thin handle to one underlying C++ object. The pattern: keep the real
state in a `KiritoDispatcher`-owned object addressed by an integer id, then expose `_getstate_` /
`_setstate_` so serialization carries only that id, and register a deserializer that rebuilds the
handle and rebinds it via `vm.dispatcher()`:

```cpp
// _getstate_ emits the shared object's id; _setstate_ rebinds to the same object in another VM.
if (name == "_getstate_")
    return makeMethod(vm, "_getstate_", {}, [self](KiritoVM& v, std::span<const Handle>) {
        return v.makeInt(static_cast<int64_t>(thing(v, self).id()));
    }, {self});
if (name == "_setstate_")
    return makeMethod(vm, "_setstate_", {"state"}, [self](KiritoVM& v, std::span<const Handle> a) {
        // v.dispatcher() is the coordinator shared by all worker VMs; look the object up by id.
        static_cast<MyVal&>(v.arena().deref(self)).obj =
            v.dispatcher()->thingById(static_cast<uint64_t>(argInt(v, a[0], "_setstate_")));
        return v.none();
    }, {self});
```

`vm.dispatcher()` is null for a bare VM, so guard on it (a value that needs cross-VM identity only makes
sense under a dispatcher). See `stdlib_parallel.hpp` for the full pattern. Live resources that *can't*
meaningfully cross (open sockets, file handles) should simply omit `_getstate_` — serialization then
raises a clear error instead of silently breaking.

## Design rules

- **Naming**: Kirito's public functions and methods are **all lowercase, no underscores**
  (`gettempdir`, `joinpath`, `startswith`, `symmetricdifference`) — match that when you add your own.
- **No global mutable state** — everything is VM-scoped, so multiple VMs stay isolated.
- **Never expose raw pointers** across the boundary; hold `Handle`s, deref at point of use.
- **Header-only ODR**: everything `inline`/templated, `#ifndef` include guards (never `#pragma once`).
- Raise `KiritoError` with a clear, actionable message for bad arguments — errors are part of the
  language, not an afterthought.
