# Kirito-in-Kirito — a Kirito interpreter written in Kirito

A complete tree-walking interpreter for the Kirito language, **written entirely in Kirito** (`.ki`).
It lexes, parses and evaluates Kirito source, and is exercised by running the main project's own
`tools/tests/scripts/*.ki` suite through it and checking each program reproduces its recorded output.

This is the ultimate stress test for the real interpreter: every token, AST node and evaluation step
of the inner programs runs through thousands of lines of Kirito — closures, classes, exceptions,
dictionaries, recursion and string processing — all at once.

## Running

```sh
cmake --build build                                                   # build the host interpreter
ki --lib examples/big_projects/selfhost/lib examples/big_projects/selfhost/run_tests.ki
```

(run from the repository root, so the `tools/tests/scripts` paths resolve). It prints `PASS`/`FAIL` per
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
- **container storage**: List/Dict/Set creation, `append`/`pop`/`add`/`discard`/`popitem`, indexing
  and key access/assignment, membership, `len`, iteration — the interpreter's memory model.
- **`ord` / `chr`** (character ↔ code point) — genuinely irreducible.
- **the IEEE formatting/parsing primitives**: float → text (shortest-round-trip repr and the `:.2f`
  format spec), text → float, float ↔ int truncation, cycle-safe container repr, and the single
  most-negative integer's decimal form — the analogue of libc's `strtod`/`snprintf` plus the object
  identity needed to print a cyclic structure as `[...]`.
- **one thin syscall** (`_sys_readFile`, built on `io.open`) used only to load a std module's source.

Everything else is pure Kirito: the lexer, parser and evaluator; the class/exception/`with`/`switch`
machinery; the `math` module; **every builtin** (`range`, `sorted`, `map`, `abs`, `round`, `divmod`,
`pow`, `bin`/`oct`/`hex`, the bitwise ops, `Integer`-of-string, integer `String()`, …); and **every
type method** of String (`upper`/`split`/`replace`/`find`/`strip`/`center`/…), List, Dict and Set
(`index`/`count`/`reverse`/`extend`/`union`/`intersection`/`items`/`update`/…).

### Standard modules — all implemented in Kirito under `lib/stdmods/`

| Module | Notes |
|--------|-------|
| `math` | series/Newton/Lanczos; inf/NaN via ordering comparisons |
| `sys`  | `joinpath` logic in Kirito; env/cwd facts via the `_os` syscall module |
| `json` | recursive-descent parser + compact/indented serializer |
| `random` | object PRNG (LCG): random/uniform/randint/randrange/choice/shuffle/sample/gauss |
| `matrix` | Matrix class: +,-,*, transpose, determinant, trace, inverse, zeros/ones/identity |
| `time` | clocks via `_os`; DateTime with epoch↔civil math, format/iso/diff/strptime |
| `net` | URL helpers: quote/unquote (UTF-8), urlencode/parseqs, urlsplit |
| `hash` | **standards-conformant** md5 / sha1 / sha256 (byte-exact) |
| `zlib` | run-length codec + adler32 (deflate/inflate/compress/decompress) |
| `serialize` / `dump` | reference- and cycle-preserving graph serialization (uses `id()`) |
| `itertools` `functools` `collections` `statistics` `string` `textwrap` `base64` `csv` `heapq` `bisect` `copy` `enum` | the Kirito-authored stdlib, evaluated through the self-host |

The only host facilities used: the value substrate (above), `id()` (object identity, added to the
host as a small general-purpose builtin — the primitive the serializer needs), and the `_os` thin
syscall module (clocks + environment) used by `sys`/`time`.

## Test coverage

The harness auto-discovers all 43 runnable `tools/tests/scripts/*.ki`. Status:

- **40 / 43 reproduce the real interpreter's output byte-for-byte.** Two of them (`spec_modules`,
  `spec_serde`) are slow because they run the pure-Kirito md5/sha1/sha256 — correct, but a hash
  block costs ~20 s under the doubly-interpreted runtime.
- `sample_pipeline` and `spec_fuzz` are **correct but impractically slow** here: the first hashes
  kilobytes of data, the second runs 5000 hostile iterations — both minutes-to-tens-of-minutes
  through the double interpreter.
- `libraries` is the one genuine mismatch: it asserts an **exact** `random.randint` value, which
  would require bit-reproducing the host's `std::mt19937_64` *and* libstdc++'s implementation-defined
  `uniform_int_distribution`. The rest of that program matches.

The performance ceiling is inherent to running a tree-walking interpreter on top of a tree-walking
interpreter; it is a speed limit, not a correctness one.

## Three-level tower (Kirito on Kirito on Kirito)

`tower.ki` runs a program on a self-host that is *itself* running on a self-host — the C++ Kirito
interprets the self-host, which interprets the self-host, which interprets your program (three
interpreters deep). Run from the repo root:

```sh
ki --lib examples/big_projects/selfhost/lib examples/big_projects/selfhost/tower.ki program.ki
```

It's slow (every operation is interpreted twice over), but it proves the interpreter is complete
enough to host itself: the level-2 self-host parses and evaluates the entire `lib/*.ki` source and
then runs the inner program at level 3.
