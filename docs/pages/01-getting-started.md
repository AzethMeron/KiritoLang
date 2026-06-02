# Getting Started

## Requirements

- A C++20 compiler (GCC 13+ or Clang 18+).
- CMake 3.28+ and a generator (Ninja recommended).

The interpreter core is **header-only**; CMake builds only the `ki` executable and the tests.

## Building

```
cmake --preset debug          # or: release / strict / asan
cmake --build build
```

Presets (`CMakePresets.json`):

| Preset | What it is |
|--------|-----------|
| `debug` | everyday build, assertions on |
| `release` | optimized |
| `strict` | `-O2 -Werror -Wall -Wextra -Wpedantic ...` — the quality gate |
| `asan` | AddressSanitizer + UBSan (memory/UB checks) |

The standalone binary is statically linked by default, so `build/ki` is self-contained.

## Running

```
ki                      # REPL (interactive)
ki script.ki            # run a file
ki script.ki a b c      # run a file; a b c become arglist
ki --lib path/to/libs script.ki   # add an import search directory
ki -w script.ki         # disable static warnings
```

### The REPL

With no file, `ki` starts a Read-Eval-Print Loop. Type an expression to see its value; type a
statement to run it. Multi-line blocks are supported — when a line ends with `:` (opening an
indented suite), the prompt switches to `...` and keeps reading until you enter a blank line:

```
>>> var x = 41
>>> x + 1
42
>>> var f = Function():
...     return "hi"
...
>>> f()
hi
```

## Your first script

Create `hello.ki`:

```kirito
var io = import("io")
var name = io.input("What's your name? ")
io.print(f"Hello, {name}!")
```

Run it:

```
ki hello.ki
```

## Tests

Kirito has an extensive CTest suite (unit tests, golden `.ki` scripts, error-message tests, an
adversarial/fuzz suite, and an embedding test). Run it with:

```
ctest --test-dir build --output-on-failure
```

The `scripts/post_work_check.sh` routine clean-builds every variant (debug/strict/asan) and runs
the whole suite — the bar a change must clear before it's "done".
