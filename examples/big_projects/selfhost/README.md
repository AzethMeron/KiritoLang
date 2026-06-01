# Kirito-in-Kirito — a Kirito interpreter written in Kirito

A complete tree-walking interpreter for the Kirito language, **written entirely in Kirito** (`.ki`).
It lexes, parses and evaluates Kirito source, and is exercised by running the main project's own
`tests/scripts/*.ki` suite through it and checking each program reproduces its recorded output.

This is the ultimate stress test for the real interpreter: every token, AST node and evaluation step
of the inner programs runs through thousands of lines of Kirito — closures, classes, exceptions,
dictionaries, recursion and string processing — all at once.

## Running

```sh
cmake --build build                                                   # build the host interpreter
ki --lib examples/big_projects/selfhost/lib examples/big_projects/selfhost/run_tests.ki
```

(run from the repository root, so the `tests/scripts` paths resolve). It prints `PASS`/`FAIL` per
program and ends with `ALL SELF-HOST TESTS PASSED`. It is also wired into CTest as the `selfhost`
test.

To run a single program through the self-host from C++/CLI, use `run.ki`:

```sh
ki --lib examples/big_projects/selfhost/lib examples/big_projects/selfhost/run.ki path/to/program.ki
```

## Architecture

The interpreter mirrors the real one's pipeline — `source -> Lexer -> [tokens] -> Parser -> [AST]
-> Evaluator -> result` — with each stage in its own module:

| File | Role |
|------|------|
| `lib/lexer.ki`  | Tokeniser: numbers (dec/hex/oct/bin, floats, scientific), strings with escapes, f-strings, operators, and the layout tokens NEWLINE/INDENT/DEDENT that drive significant indentation. |
| `lib/parser.ki` | Recursive-descent + precedence-climbing parser producing an AST of tagged Dicts. Handles `var`/assignment/unpacking (incl. starred), `if`/`elif`/`else`, `while`, `for`, `switch`/`case`/`default`, functions (defaults, kwargs, type annotations, inline bodies), classes, `try`/`catch`/`finally`, `throw`, `with`, slices, variadic subscripts, and f-string holes. |
| `lib/interp.ki` | The evaluator: layered scopes, closures, the full operator/`_dunder_` protocol, inheritance with `self._super_()`, private members, runtime-enforced type annotations, control flow, and a comprehensive builtin set. |
| `kirito.ki`     | Umbrella exposing `run(source)` / `runWithInput(source, stdin)`. |
| `run.ki`        | CLI: run a `.ki` file through the self-host. |
| `run_tests.ki`  | Acceptance harness: runs the main suite's programs and diffs output against `.expected`. |

### How values are represented

The self-host runs *inside* the real interpreter, so it reuses host values wherever possible: an
inner Integer **is** a host Integer, an inner List **is** a host List, and so arithmetic, comparison
and stringification are inherited for free. Only things the host has no native value for are boxed
as tagged Dicts (key `"sk"`): functions, native functions, bound methods, classes, instances and
modules.

Control flow (`return`/`break`/`continue`) is carried as a small signal Dict propagated up through
statement execution; interpreted `throw` and runtime errors use the host's own exceptions so they
unwind to the nearest interpreted `try` — which means a self-host `try`/`catch` also transparently
catches errors raised by the host underneath it (mirroring how the real interpreter catches C++
exceptions).

### Standard library — implemented in Kirito, not borrowed

Nothing is forwarded to the host's stdlib. Standard modules are ordinary `.ki` source files under
`lib/stdmods/` that **the self-host lexes, parses and evaluates through itself** (true self-hosting),
exposing their top-level bindings as members. `lib/stdmods/math.ki`, for example, computes trig/exp/
log from range-reduced Taylor/atanh series, roots by Newton iteration, and gamma via Lanczos — pure
Kirito, accurate within the suite's tolerance, classifying inf/NaN with ordering comparisons (since
float `==` is relative-epsilon).

`io` is the one native module, because it is the interpreter's boundary to real output: it owns the
captured `stdout`/`stderr` buffers and the fed `stdin`, and provides `print`/`eprint`/`write`/`input`/
`BytesIO` plus rebindable streams for redirection.

#### The irreducible substrate

A tree-walking interpreter must bottom out somewhere. The self-host leans on the host for exactly:

- **scalar arithmetic / comparison operators** on Integer and Float (an inner Integer *is* a host
  Integer; `+` *is* the host `+`). Note float `==` is the host's relative-epsilon comparison.
- **container storage**: List/Dict/Set creation, indexing, `append`/`pop`/`get`/`keys`/`add`, `len`,
  iteration — the interpreter's memory model.
- **`ord` / `chr`** (character ↔ code point) — genuinely irreducible.
- **number ↔ text and float ↔ int conversions** (`Integer`/`Float`/`String` on scalars) — the
  IEEE formatting/parsing primitives (the analogue of libc's `strtod`/`snprintf`).
- **one thin syscall** (`_sys_readFile`, built on `io.open`) used only to load a std module's source
  off disk.

Everything else — the lexer, parser, evaluator, every statement and operator, the class/exception/
`with` machinery, and the `math` module — is pure Kirito.

## Test coverage

The harness runs every main-suite program whose dependencies are bridged — the language core plus
`io` and `math`. That is **27 programs** spanning arithmetic, control flow, functions/closures,
collections, strings, f-strings, classes (inheritance, operators, `_super_`, private members),
exceptions, scoping, references, value representation, builtins, numbers, edge cases and math — all
passing, i.e. producing byte-for-byte the same output as the real interpreter.

Programs that depend on native modules with no bridge yet (`net`, `zlib`, `hash`, `time`, `matrix`,
`complex`, `random`, `json`, `serialize`, `dump`, and the `.ki`-authored stdlib modules) are out of
scope for the harness.
