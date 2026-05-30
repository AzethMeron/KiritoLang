#!/usr/bin/env python3
# Cross-language benchmark driver: Kirito vs C++ vs Python.
#
# For each workload it runs the three equivalent implementations (compare_bench.{cpp,py,ki}), each of
# which times `reps` internal repetitions (excluding process startup and input generation) and prints
# "<mean_ns> <stddev_ns>". This driver tabulates mean ± stddev per language and the Kirito slowdown
# factor, grouped into:
#   pessimistic  — interpreter-bound tight loops, where a tree-walker is expected to do poorly
#   optimistic   — work delegated to library/builtins, where Kirito's per-op overhead is amortized
#
# Usage:  python3 tests/bench/compare.py [--ki PATH] [--scale F] [--reps-min N]
#   --scale F     multiply every reps count by F (e.g. 0.1 for a quick smoke run)
#   --ki PATH     path to the `ki` interpreter (default: ../../build/ki relative to this script)
import argparse
import math
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))

# (name, category, N, reps).  N/reps are tuned so each Kirito run stays in a few seconds while every
# language times the same algorithm on the same LCG-generated data.
WORKLOADS = [
    ("sum_loop",   "pessimistic", 1000, 2000),
    ("fib",        "pessimistic",   17,  300),
    ("sieve",      "pessimistic", 1500,  500),
    ("sort",       "optimistic",  1500, 2000),
    ("dict_ops",   "optimistic",  1000, 1500),
    ("string_ops", "optimistic",  1500, 2000),
]


def fmt(ns):
    if ns < 1e3:
        return "%.0f ns" % ns
    if ns < 1e6:
        return "%.2f us" % (ns / 1e3)
    if ns < 1e9:
        return "%.2f ms" % (ns / 1e6)
    return "%.2f s" % (ns / 1e9)


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode != 0:
        raise RuntimeError("command failed: %s\n%s" % (" ".join(cmd), p.stderr))
    mean, sd = p.stdout.strip().split("\n")[-1].split()
    return float(mean), float(sd)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ki", default=os.path.join(HERE, "..", "..", "build", "ki"))
    ap.add_argument("--scale", type=float, default=1.0)
    args = ap.parse_args()

    ki = os.path.abspath(args.ki)
    if not os.path.exists(ki):
        sys.exit("ki interpreter not found at %s (build it first, or pass --ki)" % ki)

    # Build the C++ baseline (-O2) into a temp binary.
    cbin = os.path.join(tempfile.gettempdir(), "kirito_compare_bench")
    print("compiling C++ baseline (-O2) ...", flush=True)
    subprocess.run(["g++", "-O2", "-std=c++17", os.path.join(HERE, "compare_bench.cpp"), "-o", cbin],
                   check=True)

    pybench = os.path.join(HERE, "compare_bench.py")
    kibench = os.path.join(HERE, "compare_bench.ki")

    rows = []
    for name, cat, N, reps in WORKLOADS:
        reps = max(20, int(reps * args.scale))
        print("running %-12s (N=%d, reps=%d) ..." % (name, N, reps), flush=True)
        cpp = run([cbin, name, str(N), str(reps)])
        py = run([sys.executable, pybench, name, str(N), str(reps)])
        kj = run([ki, kibench, name, str(N), str(reps)])
        rows.append((name, cat, N, reps, cpp, py, kj))

    # --- present results ---
    print()
    bar = "=" * 100
    print(bar)
    print("  Kirito vs C++ vs Python  —  mean ± stddev per repetition  (lower is better)")
    print(bar)
    hdr = "%-12s %-7s %8s  %18s  %18s  %18s  %8s %8s" % (
        "workload", "N", "reps", "C++ (-O2)", "Python 3", "Kirito", "Ki/C++", "Ki/Py")
    for cat in ("pessimistic", "optimistic"):
        print("\n[%s]" % cat)
        print(hdr)
        print("-" * 100)
        for name, c, N, reps, cpp, py, kj in rows:
            if c != cat:
                continue
            cpp_s = "%s ± %s" % (fmt(cpp[0]), fmt(cpp[1]))
            py_s = "%s ± %s" % (fmt(py[0]), fmt(py[1]))
            ki_s = "%s ± %s" % (fmt(kj[0]), fmt(kj[1]))
            ki_cpp = kj[0] / cpp[0] if cpp[0] else float("inf")
            ki_py = kj[0] / py[0] if py[0] else float("inf")
            print("%-12s %-7d %8d  %18s  %18s  %18s  %7.0fx %7.1fx" % (
                name, N, reps, cpp_s, py_s, ki_s, ki_cpp, ki_py))

    # geometric-mean slowdowns
    def geomean(vals):
        return math.exp(sum(math.log(v) for v in vals) / len(vals))
    for cat in ("pessimistic", "optimistic"):
        sub = [r for r in rows if r[1] == cat]
        gm_cpp = geomean([kj[0] / cpp[0] for _, _, _, _, cpp, _, kj in sub])
        gm_py = geomean([kj[0] / py[0] for _, _, _, _, _, py, kj in sub])
        print("\n  %-11s geo-mean slowdown:  Kirito is %.0fx C++,  %.1fx Python" % (cat, gm_cpp, gm_py))
    print()


if __name__ == "__main__":
    main()
