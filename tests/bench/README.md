# Benchmarks

Two things live here:

- `bench.cpp` — the in-tree correctness/timing harness run by CTest (a feature isn't "fast enough"
  until this stays green); built like any other test.
- The **cross-language comparison** (`compare.py` driver + `compare_bench.{cpp,py,ki}`) — Kirito vs
  C++ vs Python on identical algorithms over identical (LCG-generated) data. It is a manual tool, not
  a CTest case (a full run takes ~30 s and shells out to `g++`/`python3`).

## Running the comparison

```sh
cmake --build build --target ki          # the driver needs the ki interpreter
python3 tests/bench/compare.py           # full run (reps in the thousands)
python3 tests/bench/compare.py --scale 0.05   # quick smoke run
python3 tests/bench/compare.py --ki /path/to/ki
```

Each implementation times `reps` internal repetitions with its own high-resolution clock
(`steady_clock` / `time.perf_counter_ns` / `time.perf_counter_ns`), excluding process startup and
input generation, then prints `<mean_ns> <stddev_ns>`. The driver tabulates mean ± stddev and the
Kirito slowdown factor.

## Workloads

Chosen to bracket Kirito's performance envelope:

- **pessimistic** — interpreter-bound tight loops where a tree-walker pays per-operation dispatch on
  every step: `sum_loop` (arithmetic loop), `fib` (recursive calls), `sieve` (nested loops + indexed
  list writes).
- **optimistic** — work delegated to C++ builtins/library so the interpreter overhead is amortized
  over a few native calls: `sort` (builtin sort), `dict_ops` (hash-map insert/lookup), `string_ops`
  (`split`/`join`).

The expected shape: Kirito is orders of magnitude slower than C++/Python on the pessimistic loops,
but closes most of the gap on the optimistic, builtin-delegated workloads — exactly what a
tree-walking interpreter with a fast C++ standard library should show.
