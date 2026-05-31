# Embedding Kirito in C++

Kirito is header-only and Lua-style embeddable: `#include "kirito.hpp"`, construct a `KiritoVM`, and
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

Build it by adding `include/` to your include path and compiling as C++20. No library to link — the
core is all inline/templated headers.

## The `KiritoVM` surface

A `KiritoVM` owns its arena, global environment, and module registry. Key methods:

| Method | Purpose |
|--------|---------|
| `runSource(src [, chunkName])` | Lex+parse+eval a chunk in a fresh module scope; returns the last value's `Handle`. |
| `runRepl(src)` | Like `runSource` but reuses one persistent module scope across calls. |
| `stringify(Handle)` | The `str()` of a value (what `print` shows). |
| `makeInt / makeFloat / makeString / makeBool / none()` | Construct values from C++. |
| `registerGlobal(name, Handle)` | Bind a global name (e.g. inject a value or function). |
| `install<Module>()` | Register a C++-authored module (see *Extending*). |
| `registerSourceModule(name, src)` | Register a module whose body is Kirito source. |
| `arena()` | The value arena, for `deref(Handle)` etc. |

## Values and handles

Every Kirito value is an `Object` owned by the VM's arena; your C++ code holds lightweight `Handle`s
(slot + generation) and calls `vm.arena().deref(h)` to reach the object. Reference-assignment in
Kirito means two names share one handle. You never own raw `Object` pointers.

```cpp
Handle list = vm.runSource("[1, 2, 3]\n");
const Object& o = vm.arena().deref(list);
std::printf("len = %lld\n", (long long)o.length(vm).value());   // 3
```

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

Use `registerGlobal` with a `NativeFunction`:

```cpp
vm.registerGlobal("now_ms", vm.arena().alloc(std::make_unique<NativeFunction>(
    "now_ms", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return vm.makeInt(static_cast<int64_t>(ms));
    })));
// Kirito: var t = now_ms()
```

## Isolation

Each `KiritoVM` is independent — no global mutable state. You can run several VMs in one process
(e.g. per-request sandboxes) and they never share values or modules.
