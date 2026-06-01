#!/usr/bin/env bash
# Kirito post-work verification routine.
#
# Run this AFTER finishing a change and BEFORE calling the work done. It rebuilds every build
# variant available in the current environment from scratch and runs the WHOLE CTest suite for each.
#
# Forward-compatible by design: it NEVER enumerates individual test files. Tests are auto-discovered
# by CTest (unit executables and the globbed scripts/ and errors/ directories register themselves in
# tests/CMakeLists.txt), so adding or removing a test needs no change here.
#
# Variants (each in its OWN build dir, so they share no files and run concurrently):
#   debug   — g++, Debug, the everyday build.
#   strict  — g++, -Werror with the full warning set (the gate the CI/quality bar uses).
#   asan    — clang++ AddressSanitizer + UBSan (memory/UB safety).
#   windows — mingw-w64 cross build, ONLY if a toolchain file or compiler is present (skipped
#             gracefully otherwise; this sandbox has no mingw).
#
# Parallelism: all variants build+test AT THE SAME TIME, and each runs CTest multithreaded. Total
# concurrency is bounded (cores are split across the live variants) so three header-only builds —
# each TU is RAM-hungry — never thrash the CPU or exhaust memory. The win is pipeline overlap: one
# variant's test phase runs while another is still compiling.
#
# Isolation / race-safety:
#   - distinct binary dirs (build / build-strict / build-asan / build-win) — no shared build files;
#   - the source tree is read-only during the run (CMake writes only inside its binary dir);
#   - each variant runs in its own subshell with its own log file (no interleaved output, no shared
#     mutable shell state); asan's stack/env tweaks are scoped to asan's subshell, never leaking.
#
# Usage:  scripts/post_work_check.sh [--quick]
#   --quick   debug + strict only (skip asan); useful for a fast inner loop.
#
# Exit status is non-zero if ANY configured variant fails to build or has a failing test.

set -u
cd "$(dirname "$0")/.."

QUICK=0
[ "${1:-}" = "--quick" ] && QUICK=1

have() { command -v "$1" >/dev/null 2>&1; }

# Total cores available (never fewer than 2).
JOBS="$( { nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2; } )"
[ "$JOBS" -lt 2 ] 2>/dev/null && JOBS=2

WIN_CXX=x86_64-w64-mingw32-g++

# --- decide the variant set up front, so we can split cores across the ones that will run live ---
VARIANTS=(debug strict)
[ "$QUICK" -eq 0 ] && VARIANTS+=(asan)
have "$WIN_CXX" && VARIANTS+=(windows) || echo "SKIP[windows]: $WIN_CXX not found (no mingw toolchain in this environment)"

# Per-variant job budget. Every variant builds+tests with the FULL core count and they run
# concurrently: the kernel scheduler keeps all cores saturated while several builds overlap, and —
# crucially — as the quick variants (debug/strict) finish, their cores flow to the slow one (asan,
# the long pole), instead of sitting idle as they would under a static per-variant split. Memory
# stays bounded because a single compiler TU is modest and the one memory-hungry test (the module
# fuzzer) recycles its VM, so concurrent variants don't stack into an OOM.
PER="$JOBS"

LOGDIR="$(mktemp -d "${TMPDIR:-/tmp}/kirito-pwc.XXXXXX")"
trap 'rm -rf "$LOGDIR"' EXIT

# configure command for one variant (kept here so the launcher stays a flat loop)
configure_cmd() {
    case "$1" in
        debug)   echo "cmake --preset debug" ;;
        strict)  echo "cmake --preset strict" ;;
        asan)    echo "cmake --preset asan" ;;
        windows) echo "cmake -S . -B build-win -G Ninja -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_CXX_COMPILER=$WIN_CXX -DCMAKE_EXE_LINKER_FLAGS=-static" ;;
    esac
}
binary_dir() {
    case "$1" in
        debug) echo build ;; strict) echo build-strict ;; asan) echo build-asan ;; windows) echo build-win ;;
    esac
}

# Build+test one variant, fully self-contained, all output to its own log. Exit 0 = OK, 1 = failure.
run_variant() {
    local name="$1" dir; dir="$(binary_dir "$name")"
    local log="$LOGDIR/$name.log"
    {
        echo "=== variant: $name  (build/test jobs: $PER, dir: $dir) ==="
        # Private temp dir per variant: the file-I/O tests resolve scratch paths from the system
        # temp dir (io.gettempdir / std::filesystem::temp_directory_path, both honoring TMPDIR), so
        # giving each concurrent variant its own TMPDIR means they never touch the same file.
        export TMPDIR="$LOGDIR/tmp-$name"
        mkdir -p "$TMPDIR"
        # asan: instrumented frames are several times larger, so the Kirito recursion guard (~3000
        # frames, safe on a normal stack) can blow an 8 MB stack before it trips. Give this run a
        # generous stack — scoped to THIS subshell only, so debug/strict are unaffected.
        if [ "$name" = asan ]; then
            ulimit -s 262144 2>/dev/null || true
            export ASAN_OPTIONS=detect_leaks=1
            export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
        fi
        rm -rf "$dir"
        # shellcheck disable=SC2046  # configure_cmd is a trusted, space-split command line
        if ! $(configure_cmd "$name"); then echo "FAIL[$name]: configure"; exit 1; fi
        if ! cmake --build "$dir" -j "$PER"; then echo "FAIL[$name]: build"; exit 1; fi
        if ! ctest --test-dir "$dir" --output-on-failure -j "$PER"; then echo "FAIL[$name]: tests"; exit 1; fi
        echo "OK[$name]"
    } >"$log" 2>&1
}

echo "Running ${#VARIANTS[@]} variant(s) concurrently: ${VARIANTS[*]}  (cores=$JOBS, per-variant jobs=$PER)"

declare -A PID
for name in "${VARIANTS[@]}"; do
    run_variant "$name" &
    PID[$name]=$!
    echo "  -> launched $name (pid ${PID[$name]})"
done

# Collect results: wait on each PID for its real exit status (subshells can't mutate parent state).
rc=0
declare -A STATUS
for name in "${VARIANTS[@]}"; do
    if wait "${PID[$name]}"; then STATUS[$name]=ok; else STATUS[$name]=fail; rc=1; fi
done

# Replay each variant's log in a stable order (concurrent stdout never interleaves this way).
ran=()
for name in "${VARIANTS[@]}"; do
    echo "==================================================================="
    cat "$LOGDIR/$name.log"
    [ "${STATUS[$name]}" = ok ] && ran+=("$name")
done

echo "==================================================================="
if [ "$rc" -eq 0 ]; then
    echo "ALL GREEN — variants run: ${ran[*]:-none}"
else
    echo "FAILURES PRESENT — see FAIL[...] lines above"
fi
exit "$rc"
