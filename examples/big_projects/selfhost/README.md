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

### Standard library

Module functions are **bridged** to the host: `import("math")` inside the self-host returns a module
whose members forward to the host's `math`. The point of the project is to exercise the *language*,
not to re-implement zlib or sockets in pure Kirito. Currently bridged: `io` (print/eprint/write/
input, `BytesIO`, and rebindable `stdout`/`stderr` for redirection) and `math` (full function set).

## Test coverage

The harness runs every main-suite program whose dependencies are bridged — the language core plus
`io` and `math`. That is **27 programs** spanning arithmetic, control flow, functions/closures,
collections, strings, f-strings, classes (inheritance, operators, `_super_`, private members),
exceptions, scoping, references, value representation, builtins, numbers, edge cases and math — all
passing, i.e. producing byte-for-byte the same output as the real interpreter.

Programs that depend on native modules with no bridge yet (`net`, `zlib`, `hash`, `time`, `matrix`,
`complex`, `random`, `json`, `serialize`, `dump`, and the `.ki`-authored stdlib modules) are out of
scope for the harness.
