#!/usr/bin/env bash
# Kirito post-work verification routine.
#
# Run this AFTER finishing a change and BEFORE calling the work done. It rebuilds each build variant
# from scratch and runs the WHOLE CTest suite for each — SEQUENTIALLY, in a fixed order, because the
# order encodes the workflow gate (see below).
#
# Forward-compatible by design: it NEVER enumerates individual test files. Tests are auto-discovered
# by CTest (unit executables and the globbed scripts/ and errors/ directories register themselves in
# tests/CMakeLists.txt), so adding or removing a test needs no change here.
#
# Variants (each in its OWN build dir; the three the project ships — `strict` was folded into `debug`):
#   debug   — g++ -O2 with the HARDENED warning set: -Wall -Wextra -Wformat=2 -Wconversion
#             -Wpointer-arith -Wpedantic -Werror -fstack-protector-all -Wreorder -Wunused -Wshadow.
#             The strictest compile gate. (binaryDir: build-debug)
#   release — g++ -O2, the looser warnings-as-errors set (no -Wconversion/-Wshadow); the build to
#             benchmark and ship. (binaryDir: build-release)
#   asan    — AddressSanitizer + UBSan (-fno-sanitize-recover=all) with the hardened warning set; the
#             memory/UB-safety gate, and the slow one. (binaryDir: build-asan)
#
# THE WORKFLOW GATE (run sequentially, in THIS order):
#   1. build + test `debug`.
#   2. build + test `release`.
#   3. If BOTH debug and release are green -> COMMIT AND PUSH. This is the point at which the work
#      becomes durable; do it BEFORE the long asan run so a crash/preemption/rollback can't lose it.
#   4. build + test `asan`; fix any error it surfaces, then re-run (and push the fix).
#
# This script runs the variants in that order and reports each. It does NOT git-commit for you (the
# commit message/branch is a decision for the author), but after debug+release pass it prints a clear
# READY-TO-PUSH marker, then continues into asan.
#
# Usage:  scripts/post_work_check.sh [--no-asan]
#   --no-asan   run debug + release only (the commit gate); skip the slow asan pass.
#
# Exit status is non-zero if ANY variant fails to build or has a failing test.

set -u
cd "$(dirname "$0")/.."

NO_ASAN=0
[ "${1:-}" = "--no-asan" ] && NO_ASAN=1

JOBS="$( { nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4; } )"
[ "$JOBS" -lt 2 ] 2>/dev/null && JOBS=2

declare -A DIR=( [debug]=build-debug [release]=build-release [asan]=build-asan )
FAILED=0
GREEN_GATE=1   # cleared if debug or release fails

# Build+test one variant from scratch. Returns 0 on success. asan gets a generous stack + sanitizer
# options, scoped to its own invocation (the recursion guard's frames are larger under ASan).
run_variant() {
    local name="$1" dir="${DIR[$1]}"
    echo "==================== VARIANT: $name ===================="
    rm -rf "$dir"
    if ! cmake --preset "$name" >"/tmp/pw_$name.cfg.log" 2>&1; then
        echo "[$name] CONFIG FAILED"; tail -8 "/tmp/pw_$name.cfg.log"; FAILED=1; return 1
    fi
    if ! cmake --build "$dir" -j"$JOBS" -- -k 0 >"/tmp/pw_$name.build.log" 2>&1; then
        echo "[$name] BUILD FAILED ($(grep -cE 'error:' "/tmp/pw_$name.build.log") errors):"
        grep -E 'error:' "/tmp/pw_$name.build.log" | head -20
        FAILED=1; return 1
    fi
    local pre=""
    if [ "$name" = asan ]; then
        ulimit -s 262144 2>/dev/null || true
        pre="ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
    fi
    if env $pre ctest --test-dir "$dir" -j"$JOBS" >"/tmp/pw_$name.test.log" 2>&1; then
        echo "[$name] $(grep -E 'tests passed' "/tmp/pw_$name.test.log" | tail -1)"
        return 0
    fi
    echo "[$name] TESTS FAILED:"; grep -A8 'The following tests FAILED' "/tmp/pw_$name.test.log" | tail -10
    FAILED=1; return 1
}

run_variant debug   || GREEN_GATE=0
run_variant release || GREEN_GATE=0

echo "==================== COMMIT GATE ===================="
if [ "$GREEN_GATE" -eq 1 ]; then
    echo "READY TO PUSH: debug + release are GREEN — commit and push now, before asan."
else
    echo "DO NOT PUSH: debug or release failed — fix before committing."
fi

[ "$NO_ASAN" -eq 0 ] && run_variant asan

echo "==================== SUMMARY ===================="
for v in debug release asan; do
    [ "$v" = "asan" ] && [ "$NO_ASAN" -eq 1 ] && { echo "asan: <skipped>"; continue; }
    line=$(grep -hE 'tests passed|TESTS FAILED|BUILD FAILED|CONFIG FAILED' \
                 "/tmp/pw_$v.test.log" "/tmp/pw_$v.build.log" "/tmp/pw_$v.cfg.log" 2>/dev/null | tail -1)
    echo "$v: ${line:-<not run>}"
done
[ "$FAILED" -eq 0 ] && echo "ALL GREEN" || echo "FAILURES PRESENT"
exit "$FAILED"
