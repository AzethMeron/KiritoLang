# Embedding Kirito in C++

Kirito is header-only and embeddable: `#include "kirito.hpp"`, construct a **`KiritoDispatcher`**, and
run source on the VM it gives you. There is no `main()` in the library — the standalone interpreter's
`main` lives only in `main.cpp`.

The `KiritoDispatcher` is the recommended entry point. It owns a fully-configured main `KiritoVM`
(with the [`parallel`](stdlib.html#parallel) module enabled) plus the machinery for worker VMs and
the cross-VM primitives — exactly what the `ki` interpreter builds. It costs nothing until you use it
(no threads are spawned until Kirito calls `parallel.spawn`), and its destructor cleanly tears
everything down, so it is the right default even for a single-threaded embed. (If you genuinely want
the bare minimum and no `parallel`, you can construct a `KiritoVM` directly — see
[A bare `KiritoVM`](#a-bare-kiritovm) at the end.)

## Minimal example

```cpp
#include "kirito.hpp"
using namespace kirito;

int main() {
    KiritoDispatcher dispatcher;          // the embedding entry point
    KiritoVM& vm = dispatcher.mainVM();   // a fully-configured interpreter (parallel enabled)
    Handle result = vm.runSource("var x = 6 * 7\nx\n");
    std::printf("%s\n", vm.stringify(result).c_str());   // 42
}   // ~KiritoDispatcher wakes any blocked workers and joins every thread
```

Build it by adding `src/` to your include path and compiling as C++20. No library to link — the
core is all inline/templated headers. (Link `-pthread` on POSIX; the dispatcher uses threads for
`parallel` workers.)

## Why go through the dispatcher

A `KiritoVM`'s arena is **unsynchronized**, so exactly one OS thread may ever touch a given VM.
Kirito's concurrency model is therefore **multiprocessing**: the dispatcher owns the main VM plus a
pool of worker VMs (one per thread) that share nothing and communicate only by passing serialized
values through thread-safe primitives. The dispatcher is what wires this together:

- **It configures the VM.** `mainVM()` lazily constructs the main VM and installs the `parallel`
  module on it (a bare `KiritoVM` has none). Worker VMs are configured the same way, so
  `parallel.spawn`/`Queue`/`Lock`/`Event`/`Semaphore`/`Barrier` work out of the box.
- **It forwards import paths.** `addLibPath` is recorded on the dispatcher and inherited by every
  worker VM, so a spawned function resolves the same `import("...")` modules as the main VM.
- **Its teardown is deadlock-safe.** `~KiritoDispatcher` (or `shutdown()`) aborts every blocked
  primitive *before* joining threads, so a worker blocked on a queue/lock/event can never stall
  teardown. `shutdown()` is idempotent.

The `ki` CLI does exactly this, which is why `parallel` is always available when you run a script
with `ki`.

## The `KiritoDispatcher` surface

| Method | Purpose |
|--------|---------|
| `mainVM()` | The main `KiritoVM` (lazily constructed + configured on first call). Run your code on this. |
| `addLibPath(dir)` | Add a module import-path directory; applied to the main VM and inherited by workers. |
| `setMaxCallDepth(n)` | Set the recursion guard (applied to the main VM and workers). |
| `shutdown()` | Abort every blocked cross-VM primitive and join all worker threads. Idempotent; also run by the destructor. |

You drive the interpreter through the `KiritoVM&` returned by `mainVM()` — its surface is below.

## The `KiritoVM` surface

A `KiritoVM` owns its arena, global environment, and module registry. Key methods (call them on
`dispatcher.mainVM()`):

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
| `addLibPath(dir)` | Add a module import path (prefer `dispatcher.addLibPath` so workers inherit it). |
| `setArgs(List)` | Set the command-line arguments a directly-run script sees as `arglist` (and makes `argmain` True). |
| `arena()` | The value arena, for `deref(Handle)` etc. |

## Values and handles

Every Kirito value is an `Object` owned by the VM's arena; your C++ code holds lightweight `Handle`s
(slot + generation). Reference-assignment in Kirito means two names share one handle. You never own
raw `Object` pointers.

The ergonomic way to read and build values is the **`Value` API** (`value.hpp`, pulled in by
`kirito.hpp`): a `Value{vm, handle}` facade with typed accessors, plus `val(vm, x)` constructors and
`List`/`Dict`/`Set` builders. It converts implicitly to/from `Handle`, so it drops in anywhere.

```cpp
KiritoDispatcher dispatcher;
KiritoVM& vm = dispatcher.mainVM();

Value list = Value(vm, vm.runSource("[1, 2, 3]\n"));
std::printf("len = %lld\n", (long long)list.len());   // 3
std::printf("first = %lld\n", (long long)list.at(0).asInt());   // 1

Value d = Value(vm, vm.runSource("{\"a\": 1}\n"));
if (d.has("a")) std::printf("a = %lld\n", (long long)d.get("a").asInt());   // 1
```

The full `Value`/`Args`/builder reference — and how to author your own functions, modules, and types
with it — is in [Extending Kirito](extending.html).

### Lifetime & rooting

The VM has a precise mark-sweep GC that runs on allocation. A `Handle`/`Value` your C++ code holds in
a local is **not** itself a GC root — if you keep one across a later `runSource`, `alloc`, or explicit
`collectGarbage()`, the value it points at may be collected and the handle then dangles (a
`"dangling handle"` error on next use). The builders root their intermediates only *during* `build()`.
So when you hold a value across further VM work, protect it: `registerGlobal(name, h)` to keep it
reachable from the module scope, or root it with a `RootScope` (`RootScope rs(vm); rs.add(h);`) /
`vm.pushTemp(h)` for the duration you need it. Values you read and use immediately (as in the snippets
above) need no rooting.

## Errors

Runtime and parse problems are thrown as `KiritoError`, which carries a `SourceSpan{line, col}` and
a message. An uncaught Kirito `throw` surfaces to the embedder as a `KiritoError` too.

```cpp
try {
    dispatcher.mainVM().runSource("1 / 0\n");
} catch (const kirito::KiritoError& e) {
    std::fprintf(stderr, "%u:%u: %s\n", e.span.line, e.span.col, e.what());   // span.line/col are uint32_t
}
```

## Injecting a C++ function

Use `registerGlobal` with a `NativeFunction`. Read arguments through `Args` + `Value` and build the
result with `val(...)`, so a bad argument becomes a clear `KiritoError` instead of a cast crash:

```cpp
KiritoVM& vm = dispatcher.mainVM();
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

## Multiprocessing with `parallel`

Building through the dispatcher is what makes the [`parallel`](stdlib.html#parallel) module available
(a bare VM can't `import("parallel")`). One requirement shapes how you embed it: **a function passed
to `parallel.spawn` must be defined in a loadable `.ki` file** — a worker VM re-reads it from disk by
its source span — and its arguments and result must be serializable. So the natural pattern is to run
a script *from a file* (give `runSource` the real path) rather than from an inline string:

```cpp
// worker.ki on disk:
//   var parallel = import("parallel")
//   var square = Function(x): return x * x
//   var tasks = []
//   for i in range(8):
//       tasks.append(parallel.spawn(square, i))
//   var total = 0
//   for t in tasks:
//       total = total + t.join()
//   import("io").print(total)        # 140

KiritoDispatcher dispatcher;
KiritoVM& vm = dispatcher.mainVM();
dispatcher.addLibPath(".");                       // workers inherit the same import path
std::ifstream in("worker.ki");
std::stringstream ss; ss << in.rdbuf();
vm.runSource(ss.str(), "worker.ki");              // the real path lets a worker re-read `square`
```

Workers are spawned on demand and share nothing — they exchange only serialized blobs through the
dispatcher-owned `Queue`/`Lock`/`Event`/`Semaphore`/`Barrier` primitives. See the
[`parallel`](stdlib.html#parallel) reference and the [Concurrency](bonus-06-concurrency.html) bonus
lesson for the full model. (An inline-string program can still use every *non-spawn* feature; only
`spawn` needs the function on disk.)

## Isolation and multiple dispatchers

There is no global mutable state. Each dispatcher owns an independent main VM and its own worker pool;
you can run several dispatchers in one process (e.g. per-request sandboxes) and they never share
values, modules, or primitives. Within one dispatcher, the main VM and its workers are isolated from
each other too — they exchange only serialized blobs.

## A bare `KiritoVM`

If you truly need the absolute minimum — no `parallel`, no worker threads — you can construct a
`KiritoVM` directly and use the same surface as above:

```cpp
KiritoVM vm;                       // standalone interpreter, no `parallel` module
vm.install<StatsModule>();         // your C++ modules still work
Handle r = vm.runSource("import(\"stats\").mean([2, 4, 6, 8])\n");
```

This is the lightest possible embed and is exactly what the extending integration test
(`tools/tests/integration/embed_demo.cpp`) uses. The only thing you give up is multiprocessing:
`import("parallel")` throws, because the `parallel` module is a dispatcher-provided capability. Reach
for this only when you are certain you'll never want worker VMs; otherwise prefer the dispatcher.
