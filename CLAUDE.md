# CLAUDE.md - Claude Code Instructions

**Read this file in full at the start of every session before doing anything else.**
It is the source of truth for what Kirito is, how we build it, and how we work.

## Git

**ALWAYS commit and push to `main`. Creating ANY branch — temporary or otherwise — is absolutely forbidden.**

## What we are building

**Kirito** — a from-scratch, dynamically-typed, strongly-typed general-purpose scripting language.
Source files use the `.ki` extension. The language namespace is `Kirito`.

Main idea for the language: it should be high-level language like Python that's fast to develop in. We want just right level of abstraction to allow for that without demanind "boilerplate code" from the user. At the same time, Kirito is supposed to be a C++ framework - users should be able to easily implement and wire-in new objects / functions / modules in C++ Kirito's framework.

Furthemore, Kirito is expected to be capable of being extension language that can be integrated into bigger application in C++. It's therefore important that single "proccess" of Kirito is expected to be fully encapsulated in single object of KiritoVM class.

Implementation: **modern C++ (C++20)**, as a **tree-walking interpreter**.

Pipeline — keep these stages separate, each behind a clean interface:

```
.ki source -> Lexer -> [tokens] -> Parser -> [AST] -> Evaluator -> result
```

The **AST is the stable boundary**. Treat it as a contract: the evaluator depends
only on the AST, never on lexer/parser internals. This keeps the door open to one
day swapping the tree-walker for a bytecode VM without rewriting the front end.

### Language shape (the target)

From the design notes and `Archive/V2/main.ki`, Kirito should support:

- `var` declarations; `#` line comments. **Python-style significant indentation**: blocks are
  introduced by `:` + newline + indent (no braces).
- **Reference assignment semantics**: `A = B + C` allocates a new value, and `A`
  is bound to it. Assignment binds names to values; it does not deep-copy. `var` declares in the
  current scope; bare `=` rebinds the nearest existing binding (a `NameError` if undefined).
- **First-class functions**: `var main = Function():` followed by an indented block, called as `main()`.
- **Control flow**: explicit `return` (functions default to `None`), `if`/`elif`/`else`, `while`,
  `for VAR in ITERABLE`, `break`, `continue`; logical keywords `and`/`or`/`not`.
- **Numerics**: separate `Integer` (int64) and `Float` (double); **Python-3 division** — `/` always
  yields `Float`, `//` is floor division, `%` modulo, `**` right-assoc exponentiation.
- **Modules** via `import("io")`; first stdlib module is `io` (`io.input`, `io.print`).
- Built-in types, dynamically typed: `None`, `Bool`, `Integer`, `Float`, `String`,
  and collections `Array`, `List`, `Set`, `Dict`. Values are hashable where it makes
  sense. Numeric/math depth (complex `Number`, `Matrix`) is a *later* enrichment, not
  the starting goal — get the general scripting core right first.
- **Classes** (planned): user-defined types in the Python spirit — `class` with
  methods and instance attributes, instantiated by calling the class. A class is just
  another first-class value, so it lives in the same value/object model as built-ins
  and a C++-defined built-in type and a Kirito-defined `class` should look alike to the
  evaluator. Like the numeric depth above, this is a *later* enrichment — get the
  procedural/functional core (vars, functions, modules) solid first.

Build the smallest thing that runs end-to-end first (lex+parse+eval an integer
literal, then arithmetic, then `var`, then functions, then `io`), and grow outward.
Every step must keep `main.ki`-style programs as the north star.

## The Archive is reference only

`Archive/V1` and `Archive/V2` are **prior incomplete attempts. We start from scratch.**

- **Do not build on them, do not compile them, do not import their files.**
- **Do** mine them for ideas: `Archive/V1/notes.txt` (the language spec intent),
  V1's reference-counted object model, V2's `Number`/`Matrix`/`Math`. Lift *concepts*,
  re-implement cleanly.
- The archives' MSVC `.vcxproj`/`.sln` files are dead on this Linux toolchain — ignore them.

## Architecture (as built)

- **Header-only core.** The whole interpreter lives in `include/kirito/*.hpp`, surfaced through one
  umbrella header: `#include "kirito.hpp"` embeds Kirito in any C++ program (Lua-style), **no `main`**.
  The standalone interpreter's `main()` lives only in `main.cpp`. Use **`#ifndef` include guards**
  (e.g. `KIRITO_OBJECT_HPP`), **never `#pragma once`**; everything `inline`/templated, no mutable
  globals — all state is VM-scoped.
- **One `KiritoVM` = one fully-encapsulated process**, composing its owned sub-objects: an
  `ObjectArena`, the global `Environment`, and the `ModuleRegistry`. No global/static mutable state,
  so multiple VMs coexist and the whole context is serializable later.
- **VM-owned value graph + handles.** Every value is an `Object` owned by an arena slot
  (`unique_ptr`); everything else holds lightweight `Handle`s (slot+generation). Reference-assignment
  = two bindings sharing one handle. No `shared_ptr`, no per-value refcount. Mark-sweep GC is
  designed-for (`Object::children()`) but deferred — early on, values accumulate until the VM dies.
- **Unified object protocol.** Built-ins, C++-authored types, and future Kirito `class`es all derive
  from one `Object` base exposing the same slots (`truthy/str/equals/hash`, and operation slots
  `binary/unary/call/getAttr/setAttr/getItem/setItem/iterate/length`). The evaluator dispatches
  through the protocol — it can't tell built-ins from user types.
- **Layered scoping**: global (built-ins) → module (per `.ki` file) → local (per function call);
  closures capture their lexical scope by handle. Only functions/modules introduce scopes.
- **Extending in C++**: subclass `NativeModule` (override `setup`) or `NativeClass` (override only
  the slots you need) and register with one call — indistinguishable from a built-in to the evaluator.

## Build & test

Toolchain present: `g++ 13`, `clang++ 18`, `cmake 3.28`, `ninja`, `ctest`.

- Build is **thin CMake** (out-of-source, e.g. `build/`), C++20: the header-only core is an
  `INTERFACE` target; CMake builds only `ki` (from `main.cpp`) and the test executables.
- Tests run under **CTest**. **Every language feature gets a test.** Prefer many
  small, focused tests (one behavior each) over large ones. A feature isn't done
  until it has a test and the suite is green.
- Before claiming something works, actually build and run it; report real output.

## Code style & working directives

- **Terse but self-explanatory.** Names carry the meaning. Comment the *why*, never
  the *what*; if code needs a comment to say what it does, rewrite the code.
- Small, single-responsibility units. Use abstraction only responsibly, where 
  you expect extension will be required later.
- **Keep code DRY**. If you do something three Times or more, it should probably be a 
  separate function.
- Match the style of surrounding code: naming, structure, idiom.
- Clear diagnostics: lexer/parser/runtime errors should carry line and column and a
  message a user can act on. Errors are part of the language, not an afterthought.
- Prefer the standard library and plain data structures over cleverness.
- C++: use STL and modern standard (C++20). Never expose raw pointers. Everywhere 
  it is possible favor references, if reference can't be used, use smart pointers.
  Favor std::unique_ptr over std::shared_ptr.
- In general, objects shouldn't share attributes. If B belongs to A, then A "owns" B
  and this gets messy when C that's not part of A has reference to A. Variables in
  kirito code can get and use references like this, but it must not be ingrained in 
  language internals, we want clean separation so in the end it's easy to save&load 
  context. 

## Keep this file current

When a decision changes the language design, architecture, or workflow, update this
file in the same change. It must always describe Kirito as it actually is.
