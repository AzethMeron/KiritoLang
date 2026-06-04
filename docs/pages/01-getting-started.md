# Getting Started

## Installing (prebuilt)

If you just want to *use* Kirito, the one-line installers fetch the `ki` interpreter and the `kpm`
package manager, put launchers on your `PATH`, and create `~/.kirito/packages` (which `ki` searches
automatically):

```sh
# Linux / macOS  (installs to ~/.local/bin, no root)
curl -fsSL https://raw.githubusercontent.com/AzethMeron/KiritoLang/main/scripts/install.sh | sh
```

```powershell
# Windows (PowerShell) — installs under %LOCALAPPDATA%\Programs\Kirito and updates your user PATH
irm https://raw.githubusercontent.com/AzethMeron/KiritoLang/main/scripts/install.ps1 | iex
```

Prebuilt 64-bit binaries (`ki-linux-x64`, `ki-windows-x64.exe`) are also attached to each GitHub
Release for manual download. To build from source instead, read on.

## Requirements

- A C++20 compiler (GCC 13+ or Clang 18+).
- CMake 3.28+ and a generator (Ninja recommended).

The interpreter core is **header-only**; CMake builds only the `ki` executable and the tests.

## Building

```
cmake --preset debug          # or: release / asan
cmake --build build-debug
```

Presets (`CMakePresets.json`):

| Preset | What it is |
|--------|-----------|
| `debug` | `-O2` with the hardened warning set (`-Werror -Wall -Wextra -Wconversion -Wpedantic -fstack-protector-all -Wshadow ...`) — the strictest compile gate (binary dir `build-debug`) |
| `release` | `-O2`, the looser warnings-as-errors set (no `-Wconversion`/`-Wshadow`); the build to benchmark and ship (`build-release`) |
| `asan` | AddressSanitizer + UBSan with the same hardened warnings (memory/UB checks) (`build-asan`) |

The standalone binary is statically linked by default, so `build-debug/ki` (or the release build's
`build-release/ki`) is self-contained.

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

## Packages (`kpm`)

`kpm` is Kirito's package manager. It installs packages — bundles of `.ki` modules — **straight from
GitHub repositories**; there's no central index, you name an `owner/repo`:

```sh
kpm install owner/repo            # install a package (and its dependencies) from GitHub
kpm install owner/repo@v1.2.0     # pin a tag / branch / commit
kpm list                          # list installed packages
kpm update owner --all            # reinstall the latest
kpm remove name                   # uninstall
```

Packages install under `~/.kirito/packages/<name>/` and are importable directly — `import("name")` —
because `ki` automatically searches that directory, every package sub-directory, and any directory in
the `KIRITO_PATH` environment variable (PATH-style, `:`-separated on Unix, `;` on Windows), in
addition to the current directory, `--lib` directories, and the running script's own folder.

A package repo carries a `kirito.json` manifest at its root listing its modules (repo-relative `.ki`
paths) and any dependencies:

```json
{
  "name": "mypkg",
  "version": "1.0.0",
  "modules": ["mypkg.ki", "extra/util.ki"],
  "dependencies": ["someone/dep"]
}
```

`kpm` itself is written in Kirito (`tools/kpm.ki`), using only the `net`, `json`, and `io` modules.

## Tests

Kirito has an extensive CTest suite (unit tests, golden `.ki` scripts, error-message tests, an
adversarial/fuzz suite, and an embedding test). Run it with:

```
ctest --test-dir build-debug --output-on-failure
```

The `scripts/post_work_check.sh` routine clean-builds the variants **sequentially** — `debug`, then
`release`, commit+push once both are green, then `asan` — running the whole suite for each: the bar a
change must clear before it's "done".
