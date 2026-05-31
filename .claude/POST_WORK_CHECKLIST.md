# Post-work checklist

The routine to run **after every change, before declaring it done**. The mechanics live in
`scripts/post_work_check.sh`; this file is the human-readable contract.

## The routine

1. **Write tests for what changed.** Every new feature or fixed bug gets a focused test in the same
   change (CLAUDE.md rule). Prefer many small tests over one big one.
   - Behaviour at the C++/embedding boundary → a `tests/unit/test_*.cpp` (register it in
     `tests/CMakeLists.txt`).
   - End-to-end `.ki` behaviour with known output → `tests/scripts/NAME.ki` + `NAME.expected`.
   - Code that *should fail* → `tests/errors/NAME.ki` + `NAME.experr` (each `.experr` line is a
     required substring of stderr; the script must exit non-zero). Cover the bad path, not just the
     good one.

2. **Build every variant from scratch and run the WHOLE suite** — `scripts/post_work_check.sh`:
   - `debug`  — g++, the everyday build.
   - `strict` — g++ with `-Werror` and the full warning set (the quality gate).
   - `asan`   — clang AddressSanitizer + UBSan (memory/UB safety).
   - `windows`— mingw-w64 cross build **iff** a toolchain is present (skipped gracefully otherwise).
   Each variant is a clean reconfigure + build + `ctest`. The suite is **auto-discovered**: unit
   executables register in `tests/CMakeLists.txt`, and the `scripts/` and `errors/` directories are
   globbed, so the routine never enumerates test files and stays correct as tests come and go.

3. **Commit and push to `main`.** (Branches are forbidden — see CLAUDE.md and the
   `block_new_branches` hook.) Commit only once the suite is green.

4. **Update documentation and `CLAUDE.md` — always, in the same change.** After tests pass and the
   work is pushed, the docs must reflect reality:
   - Update `CLAUDE.md` whenever a change touches the language design, architecture, builtins,
     stdlib, or workflow (it is the source of truth and must always describe Kirito as it *is*).
   - Update the HTML docs: edit the relevant `docs/pages/*.md` (language guide, builtins, stdlib,
     recipes, course) and regenerate with `python3 docs/build_docs.py`.
   A feature without matching doc + CLAUDE.md updates is **not done**.

## Run it

```sh
scripts/post_work_check.sh          # debug + strict + asan (+ windows if a toolchain exists)
scripts/post_work_check.sh --quick  # debug + strict only, for a fast inner loop
```

Exit status is non-zero if any configured variant fails to build or has a failing test. A run is
"done" only when it prints `ALL GREEN`.

## Notes

- This sandbox has **no mingw**, so the `windows` variant is skipped here; the routine still names it
  so the intent is documented and it runs automatically wherever a cross-toolchain exists.
- Optional features behind flags (e.g. `-DKIRITO_ENABLE_TLS=ON`) are not part of the default sweep;
  build them explicitly when a change touches that code path.
