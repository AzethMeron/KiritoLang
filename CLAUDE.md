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
  Parameters take **keyword arguments** (`f(b = 2, a = 1)`, any order) and **default values**
  (`Function(base, exp = 2):`). Parameters and the return value take optional **enforcing type
  annotations** (`Function(d : Dict) -> Float:`): unlike Python hints these are *checked at runtime* —
  the argument must be an instance of the named type (inheritance-aware: a subclass satisfies a base
  annotation) and the function must return that type, else a clear error. `Any` / no annotation
  accepts anything.
- **Control flow**: explicit `return` (functions default to `None`), `if`/`elif`/`else`, `while`,
  `for VAR in ITERABLE`, `break`, `continue`; logical keywords `and`/`or`/`not`. `return` outside a
  function and `break`/`continue` outside a loop are rejected at parse time.
- **Packing & unpacking**: a bare comma sequence packs into a List (`var t = 1, 2, 3`; `return a, b`
  returns `[a, b]`). The left side of `=`, `var`, and `for` unpacks any iterable — `var a, b = pair`,
  `a, b = b, a` (swap), `for k, v in d.items()` — with a single **starred** target absorbing the
  surplus (`var first, *rest = xs`, `var *init, last = xs`). Counts are checked (a clear error on
  mismatch); unpack targets may be names, indices, or members (`a[0], a[1] = x, y`).
- **Numerics**: separate `Integer` (int64) and `Float` (double); float literals allow scientific
  notation (`1e10`, `1.5e3`, `2e-3`). **Python-3 division** — `/` always yields `Float`, `//` is
  floor division, `%` modulo, `**` right-assoc exponentiation. Integer arithmetic is fixed-width
  int64 with **well-defined two's-complement wraparound** on overflow (no UB); arbitrary-precision
  integers are a future enrichment. Resource guards: huge string/list repetition, padding, and
  `range` are bounded (raise instead of OOMing); deeply nested source/data structures raise instead
  of overflowing the native stack.
- **Modules** via `import("io")`; first stdlib module is `io` (`io.input`, `io.print`).
- Built-in types, dynamically typed: `None`, `Bool`, `Integer`, `Float`, `String`,
  and collections `Array`, `List`, `Set`, `Dict`. Values are hashable where it makes
  sense. Numeric/math depth (complex `Number`, `Matrix`) is a *later* enrichment, not
  the starting goal — get the general scripting core right first.
- **Classes**: user-defined types in the Python spirit — `class` with methods and instance
  attributes, instantiated by calling the class. A class is just another first-class value, in the
  same value/object model as built-ins, so a C++-defined type and a Kirito `class` look alike to
  the evaluator. Special methods use Python's dunder names with **single** underscores:
  `_init_`, `_str_`, `_add_`/`_sub_`/`_mul_`/`_div_`/`_floordiv_`/`_mod_`/`_pow_`,
  `_eq_`/`_ne_`/`_lt_`/`_le_`/`_gt_`/`_ge_`, `_neg_`/`_not_`, `_call_`, `_getitem_`/`_setitem_`
  (variadic keys: `m[i, j] = v`), `_len_`, `_contains_`, `_iter_`, `_enter_`/`_exit_`. Members whose
  name has a **single leading underscore and no trailing underscore** (e.g. `_count`) are **private**
  — accessible only from within a method of the same class. `self._super_()` returns a *parent view*
  of self (method lookup starts at the base of the currently-running method's class) — for extending
  inherited methods/constructors; it climbs one level per call (so multi-level chains compose),
  throws if the class has no base, and is overridable (but shouldn't be).

Build the smallest thing that runs end-to-end first (lex+parse+eval an integer
literal, then arithmetic, then `var`, then functions, then `io`), and grow outward.
Every step must keep `main.ki`-style programs as the north star.

**Status:** the language is broadly implemented and tested end-to-end (`include/kirito/*.hpp`, the
`ki` runner, an extensive CTest suite incl. golden `.ki` scripts, an embedding integration test,
a stability fuzzer, and a benchmark). Working today:
- Arithmetic (Python-3 division), `var`/reference-assignment, comparisons, `in`/`not in`.
- Indentation blocks with `if/elif/else`/`while`/`for`/`break`/`continue`/`pass`, `and`/`or`/`not`.
  Tabs and spaces both work but ambiguous mixing is rejected (Python-3 rule: measured with tab=8
  and tab=1, both must agree).
- `switch SUBJECT:` with `case V[, V2...]:` arms and an optional `default:` — **no fallthrough**
  (exactly one arm runs). Case labels are constant scalars (`Integer`/`Float`/`String`/`Bool`/`None`),
  matched exactly by type+value (so `case 1` ≠ `case 1.0`); compiled once into a hash jump-table for
  O(1) dispatch regardless of arm count. Non-scalar subjects only reach `default`; duplicate case
  values, a second `default`, and an empty body are rejected. `case`/`default` are **soft keywords**
  (lexed as identifiers, recognized only inside a switch body) so they stay usable as ordinary names
  like a `default` parameter; only `switch` itself is reserved.
- First-class functions with closures and `return`; `assert`. Recursion is bounded by a call-depth
  guard (configurable) that raises a catchable error instead of overflowing the native stack.
- `List`/`Set`/`Dict` with literals, indexing, slicing, iteration, `in`, and methods (append/pop/
  reverse/insert/remove/index/extend/copy/clear/count; keys/values/items/get/pop/update/setdefault/
  popitem/clear; add/discard/contains/union/intersection/difference/symmetric_difference/issubset/
  issuperset/isdisjoint/pop/clear/...); `len`. Lists support lexicographic ordering (`<`/`<=`/`>`/`>=`,
  element-by-element then by length, like Python) and `+` concatenation, enabling multi-key sorts via
  a list-returning `key`. Ordered collections have an efficient in-place
  `sort([key][, reverse])` that is **stable** by default (so is the `sorted()` builtin); keys are
  precomputed once per element.
- **Unicode** `String` (code-point indexing/slicing/iteration), `*` repetition, and methods
  (upper/lower [Unicode-aware]/strip/split/join/replace/startswith/endswith/find/rfind/index/count/
  is{digit,alpha,alnum,space,lower,upper}/removeprefix/removesuffix/ljust/rjust/center/zfill/
  partition/rpartition) and `.format()`.
- **User-defined `class`es** with methods, attributes, inheritance, Python-style operator methods
  (`_add_`/`_str_`/`_getitem_`/...), and private `_members`.
- **Exceptions**: `try`/`catch [Type as e]`/`finally`/`throw` (typed matching via the class chain).
  Python-style indented blocks, but **C++-style keyword names** (`catch`/`throw`, not `except`/`raise`).
  A bare `catch` also catches **any `std::exception`** crossing the native boundary (surfaced as a
  catchable String), so a C++ module that throws can't escape a Kirito `try`.
- **Context managers**: `with ... as ...` (enter/exit protocol).
- **Garbage collection**: precise mark-sweep with rooted intermediates (AddressSanitizer-clean).
- **f-strings** `f"{expr}"`; inline anonymous functions `Function(x): return x*x`.
- **Static warnings + `discard`**: a non-fatal analysis pass (`analyzer.hpp`) run before execution
  flags function-local variables assigned-but-never-used and bare expression statements whose
  non-`None` value is dropped; `discard EXPR` evaluates and intentionally drops a value (suppressing
  the latter). Warnings print `file:line:col: warning: ...` to stderr; the `ki` flag `-w`/`--no-warn`
  disables them. Module-level names (exports) and class members are never flagged.
- **Builtins**: `range`, `sum`, `min`, `max`, `abs`, `round`, `sorted`, `enumerate`, `zip`, `map`,
  `filter`, `len`, `type`, `import`, `inspect`, `all`, `any`, `reversed`, `divmod`, `isinstance`,
  `ord`, `chr`, `bin`, `oct`, `hex`, `pow` (2- and 3-arg modular), `format` (Python mini-format-spec:
  fill/align/sign/width/`,`/precision/type), and the
  `Integer`/`Float`/`String`/`Bool`/`List`/`Set`/`Dict` constructors/converters. `inspect(x)` returns
  a String describing the public methods/attributes (with signatures + annotations) of a class,
  instance, module, or function.
- **Standard library** (each a one-liner `vm.install<T>()`; a third party adds their own the same
  way — `#include` a header, register on the VM, no global state):
  - `io` — print/eprint/write/input/read acting on **rebindable, interchangeable streams**: the
    module-level `stdout`/`stderr`/`stdin` (with originals kept as `__stdout__`/`__stderr__`/
    `__stdin__`) can be reassigned to a file, a `BytesIO`, another std stream, or any object exposing
    `write`/`readline`/`read` (duck-typed) — so I/O redirection is just an assignment. A common
    stream protocol (`IoStream`: streamWrite/streamRead/streamReadLine/streamFlush) is implemented by
    `File`, `BytesIO`, and the std streams. `open` files & streams (read([n])/readline/readlines/
    write/writelines/seek/tell/flush, iterable line-by-line, usable as a `with` context manager),
    `BytesIO` (an in-memory binary buffer like Python's), plus filesystem helpers (exists/remove/
    rename/mkdir/getcwd/listdir/isfile/isdir/getsize/walk) and os.path-style path helpers
    (dirname/basename/splitext/join). Module members are rebindable (`ModuleValue::setAttr`).
  - `math` — constants and the usual functions (trig/hyperbolic, exp/log, gamma/erf, floor/ceil/
    trunc, gcd/lcm, factorial, isnan/isinf, prod/comb/perm, ...).
  - `random` — object-based RNG (`Random([seed])`, no global state): random/uniform/randint/
    randrange/choice/shuffle/sample/gauss/expovariate.
  - `matrix` — dense real matrices (no complex, no concurrency): +,-,* (matrix/scalar), `m[i, j]`
    element access/assignment, transpose, determinant, inverse, trace, apply, factories
    (zeros/ones/identity).
  - `json` — parse/loads (objects → Dict; decodes \u escapes + surrogate pairs) and stringify/dumps
    (optional indent for pretty-printing).
  - `serialize` — text graph dumps/loads/save/load preserving shared references and cycles.
  - `dump` — compact BINARY serialization (a `Dump` blob value) preserving references and cycles;
    dumps/loads, Dump(bytes), save/load.
  - `net` — TCP sockets (connect/bind/listen/accept/send/recv), an HTTP/1.1 client
    (http_get/http_post; HTTPS optional via `-DKIRITO_ENABLE_TLS=ON`, links OpenSSL), and URL
    helpers (quote/unquote/urlencode/parse_qs/urlsplit).
  - `sys` — environment (getenv/setenv/unsetenv/environ), `platform`, `exit`.
  - `time` — high-precision clocks (time/time_ns/monotonic/perf_counter_ns), sleep, and Python-like
    calendar time (`now`/`datetime`/`make`/`strptime`; `DateTime` with fields, iso/format,
    add/sub/diff arithmetic).
  - `zlib` — compress/decompress (standard zlib streams), raw deflate/inflate, adler32 — a
    self-contained DEFLATE/INFLATE, no external dependency, interoperable with real zlib.
  - `hash` — md5/sha1/sha256 hex digests (self-contained, standard-conformant).
  - **Kirito-authored, frozen-source modules** (registered via `vm.registerSourceModule(name, src)`;
    bodies live in `stdlib_kimodules.hpp`, compiled once per VM on first import): `itertools`
    (chain/repeat/cycle/islice/accumulate/product/permutations/combinations/count/takewhile/
    dropwhile/filterfalse/compress/starmap/pairwise/zip_longest/groupby), `functools`
    (reduce/partial/cache), `collections` (deque/Counter/defaultdict), `statistics`
    (mean/median/mode/variance/stdev/multimode/quantiles/...), `string` (constants + capwords),
    `textwrap` (wrap/fill/indent/dedent), `base64` (+urlsafe), `csv`, `heapq`
    (+nlargest/heapreplace/merge), `bisect`, `copy` (copy/deepcopy), `enum`.
- **Modules** can also be `.ki` files found on the import path (`--lib <dir>`, the cwd, and the
  script's directory), lexed+parsed+evaluated once per VM and cached by resolved path. The `ki` CLI
  is Python-like: REPL with no file (multi-line blocks via a `...` continuation prompt until a blank
  line), runs a file otherwise, with script `argv`. Small-integer interning, flat-vector scopes, a
  no-temporaries fast call path, and other non-invasive perf wins.
- **Sample projects** in `examples/` (complex linear-system solver, rule34 image downloader,
  word-frequency analyzer, RPN calculator) demonstrate non-trivial programs in pure Kirito.
  `examples/big_projects/` holds larger ones with Python test harnesses that double as interpreter
  stress tests: `sqldb` (a networked SQL database) and `webserver` (an HTTP/1.1 server + a small
  routing framework — method+path routing with `:name` params, middleware, JSON, static files).

Tested under strict flags (`-O2 -Werror -Wall -Wextra -Wformat=2 -Wpointer-arith -Wpedantic
-fstack-protector`, preset `strict`) and AddressSanitizer/UBSan (preset `asan`); an 11k-input fuzzer
guards stability. Tests include an **error-message suite** (`tests/errors/*.ki` + `.experr`: programs
that must fail, with the required diagnostic text) and an **adversarial suite**
(`tests/unit/test_adversarial.cpp`: overflow, recursion, cyclic structures, Unicode, slicing edge
cases). The **post-work routine** (`scripts/post_work_check.sh`, documented in
`.claude/POST_WORK_CHECKLIST.md`) clean-builds every variant (debug/strict/asan, plus a Windows
cross build where a toolchain exists) and runs the whole auto-discovered CTest suite — run it before
calling a change done.

**Docs:** an expandable HTML site (`docs/`) — hand-authored Markdown in `docs/pages/` rendered by
the dependency-free `docs/build_docs.py` into `docs/site/` (intro, build, embedding, extending,
language guide, recipes, builtins/stdlib reference, and a 3-part course with worked sample projects).
Documentation is authored in those `.md` files, NOT scraped from code comments.

Not yet done (future enrichment): comprehensions, variadic params,
generators, complex numbers, full-Unicode case folding (current `upper`/`lower` cover
ASCII + Latin-1 + Latin Extended-A), and a bytecode VM behind the AST boundary.

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
- **Cross-platform** (Linux + Windows minimum): the only platform-specific code is sockets,
  isolated behind `net_compat.hpp` (BSD sockets vs Winsock); everything else is `std::filesystem`/
  STL. CMake links `ws2_32` on Windows automatically.
- **Static linking** by default (self-contained binaries): full `-static` on GCC/Clang, static CRT
  on MSVC; TLS builds fall back to a static C++ runtime since OpenSSL is usually shared-only.
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
