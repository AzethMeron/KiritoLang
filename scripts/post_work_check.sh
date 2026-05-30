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
# Variants:
#   debug   — g++, Debug, the everyday build.
#   strict  — g++, -Werror with the full warning set (the gate the CI/quality bar uses).
#   asan    — clang++ AddressSanitizer + UBSan (memory/UB safety).
#   windows — mingw-w64 cross build, ONLY if a toolchain file or compiler is present (skipped
#             gracefully otherwise; this sandbox has no mingw).
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

rc=0
ran=()
run_variant() {  # name  build_dir  configure-cmd...
    local name="$1" dir="$2"; shift 2
    echo "==================================================================="
    echo "  variant: $name"
    echo "==================================================================="
    rm -rf "$dir"
    if ! "$@"; then echo "FAIL[$name]: configure"; rc=1; return; fi
    if ! cmake --build "$dir"; then echo "FAIL[$name]: build"; rc=1; return; fi
    if ! ctest --test-dir "$dir" --output-on-failure; then echo "FAIL[$name]: tests"; rc=1; return; fi
    ran+=("$name")
    echo "OK[$name]"
}

# --- debug (g++) ---
run_variant debug build cmake --preset debug

# --- strict (-Werror, full warnings) ---
run_variant strict build-strict cmake --preset strict

# --- asan/ubsan ---
# AddressSanitizer's instrumented frames are several times larger than a release build's, so the
# recursion-depth guard (which fires at ~3000 Kirito frames — safe on a normal stack) can blow an
# 8 MB stack before it trips. Give the sanitized run a generous stack so the guard, not the OS, wins.
if [ "$QUICK" -eq 0 ]; then
    ulimit -s 262144 2>/dev/null || true
    export ASAN_OPTIONS=detect_leaks=1
    export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
    run_variant asan build-asan cmake --preset asan
fi

# --- windows cross build (mingw-w64), only if a toolchain is available ---
WIN_CXX=x86_64-w64-mingw32-g++
if have "$WIN_CXX"; then
    run_variant windows build-win \
        cmake -S . -B build-win -G Ninja \
              -DCMAKE_SYSTEM_NAME=Windows \
              -DCMAKE_CXX_COMPILER="$WIN_CXX" \
              -DCMAKE_EXE_LINKER_FLAGS=-static
else
    echo "SKIP[windows]: $WIN_CXX not found (no mingw toolchain in this environment)"
fi

echo "==================================================================="
if [ "$rc" -eq 0 ]; then
    echo "ALL GREEN — variants run: ${ran[*]:-none}"
else
    echo "FAILURES PRESENT — see FAIL[...] lines above"
fi
exit "$rc"
