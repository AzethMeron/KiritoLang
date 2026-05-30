# CLAUDE.md - Claude Code Instructions

**Read this file in full at the start of every session before doing anything else.**
It is the source of truth for what Kirito is, how we build it, and how we work.

## Git

**ALWAYS commit and push to `main`. Creating ANY branch — temporary or otherwise — is absolutely forbidden.**

## What we are building

**Kirito** — a from-scratch, dynamically-typed, strongly-typed general-purpose scripting language.
Source files use the `.ki` extension. The language namespace is `Kirito`.

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

- `var` declarations; `#` line comments.
- **Reference assignment semantics**: `A = B + C` allocates a new value, and `A`
  is bound to it. Assignment binds names to values; it does not deep-copy.
- **First-class functions**: `var main = Function() { ... }`, called as `main()`.
- **Modules** via `import("io")`; first stdlib module is `io` (`io.input`, `io.print`).
- Built-in types, dynamically typed: `None`, `Bool`, `Integer`, `Float`, `String`,
  and collections `Array`, `List`, `Set`, `Dict`. Values are hashable where it makes
  sense. Numeric/math depth (complex `Number`, `Matrix`) is a *later* enrichment, not
  the starting goal — get the general scripting core right first.

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

## Build & test

Toolchain present: `g++ 13`, `clang++ 18`, `cmake 3.28`, `ninja`, `ctest`.

- Build system is **CMake** (out-of-source, e.g. `build/`). Target C++20.
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

## Keep this file current

When a decision changes the language design, architecture, or workflow, update this
file in the same change. It must always describe Kirito as it actually is.
