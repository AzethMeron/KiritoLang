# Embedding Kirito in C++

Kirito is header-only and embeddable: `#include "kirito.hpp"`, construct a `KiritoVM`, and
run source. There is no `main()` in the library — the standalone interpreter's `main` lives only in
`main.cpp`.

## Minimal example

```cpp
#include "kirito.hpp"
using namespace kirito;

int main() {
    KiritoVM vm;                       // one fully-encapsulated interpreter
    Handle result = vm.runSource("var x = 6 * 7\nx\n");
    std::printf("%s\n", vm.stringify(result).c_str());   // 42
}
```

Build it by adding `src/` to your include path and compiling as C++20. No library to link — the
core is all inline/templated headers.

## The `KiritoVM` surface

A `KiritoVM` owns its arena, global environment, and module registry. Key methods:

| Method | Purpose |
|--------|---------|
| `runSource(src [, chunkName])` | Lex+parse+eval a chunk in a fresh module scope; returns the last value's `Handle`. |
| `runRepl(src)` | Like `runSource` but reuses one persistent module scope across calls. |
| `stringify(Handle)` | The `str()` of a value (what `print` shows). |
| `makeInt / makeFloat / makeString / makeBool / none()` | Construct primitive values (or use `val(vm, x)` from the Value API). |
| `alloc(unique_ptr<Object>)` | Box any `Object` (a built-in or a `NativeClass`) into the arena, returning its `Handle`. |
| `registerGlobal(name, Handle)` | Bind a global name (e.g. inject a value or function). |
| `install<Module>()` | Register a C++-authored module (see *Extending*). |
| `registerSourceModule(name, src)` | Register a module whose body is Kirito source. |
| `arena()` | The value arena, for `deref(Handle)` etc. |

## Values and handles

Every Kirito value is an `Object` owned by the VM's arena; your C++ code holds lightweight `Handle`s
(slot + generation). Reference-assignment in Kirito means two names share one handle. You never own
raw `Object` pointers.

The ergonomic way to read and build values is the **`Value` API** (`value.hpp`, pulled in by
`kirito.hpp`): a `Value{vm, handle}` facade with typed accessors, plus `val(vm, x)` constructors and
`List`/`Dict`/`Set` builders. It converts implicitly to/from `Handle`, so it drops in anywhere.

```cpp
Value list = Value(vm, vm.runSource("[1, 2, 3]\n"));
std::printf("len = %lld\n", (long long)list.len());   // 3
std::printf("first = %lld\n", (long long)list.at(0).asInt());   // 1

Value d = Value(vm, vm.runSource("{\"a\": 1}\n"));
if (d.has("a")) std::printf("a = %lld\n", (long long)d.get("a").asInt());   // 1
```

The full `Value`/`Args`/builder reference — and how to author your own functions, modules, and types
with it — is in [Extending Kirito](extending.html).

## Errors

Runtime and parse problems are thrown as `KiritoError`, which carries a `SourceSpan{line, col}` and
a message. An uncaught Kirito `throw` surfaces to the embedder as a `KiritoError` too.

```cpp
try {
    vm.runSource("1 / 0\n");
} catch (const kirito::KiritoError& e) {
    std::fprintf(stderr, "%d:%d: %s\n", e.span.line, e.span.col, e.what());
}
```

## Injecting a C++ function

Use `registerGlobal` with a `NativeFunction`. Read arguments through `Args` + `Value` and build the
result with `val(...)`, so a bad argument becomes a clear `KiritoError` instead of a cast crash:

```cpp
vm.registerGlobal("repeat", vm.alloc(std::make_unique<NativeFunction>(
    "repeat", [](KiritoVM& vm, std::span<const Handle> raw) -> Handle {
        Args a(vm, raw, "repeat");
        std::string s = a.at(0).asString("s");
        int64_t n     = a.at(1).asInt("n");
        std::string out;
        for (int64_t i = 0; i < n; ++i) out += s;
        return val(vm, out);
    })));
// Kirito: repeat("ab", 3)   ->   "ababab"
```

For functions that should accept keyword arguments and defaults, give them a signature — see
[Extending Kirito](extending.html).

## Isolation

Each `KiritoVM` is independent — no global mutable state. You can run several VMs in one process
(e.g. per-request sandboxes) and they never share values or modules.

## Running multiple VMs (`KiritoDispatcher`)

Because a VM's arena is unsynchronized, exactly one thread may touch a given VM. To run Kirito code in
parallel, build VMs through a **`KiritoDispatcher`**, which owns the main VM plus worker VMs (one per
thread) and the cross-VM primitives that back the [`parallel`](stdlib.html#parallel) module:

```cpp
#include "kirito.hpp"

kirito::KiritoDispatcher dispatcher;
kirito::KiritoVM& vm = dispatcher.mainVM();   // built + configured (the `parallel` module enabled)
dispatcher.addLibPath(".");                    // forwarded to the main VM and inherited by workers
vm.runSource(source, "script.ki");             // Kirito `parallel.spawn(...)` now works
// ~KiritoDispatcher (or dispatcher.shutdown()) wakes any blocked workers and joins every thread.
```

The `ki` interpreter does exactly this, so `parallel` is always available there. A **bare** `KiritoVM`
(constructed directly, as above) has **no** `parallel` module — multiprocessing is a
dispatcher-provided capability. `shutdown()` is idempotent and deadlock-safe: it aborts every blocked
primitive before joining, so a worker blocked on a queue/lock/event can never stall teardown.
