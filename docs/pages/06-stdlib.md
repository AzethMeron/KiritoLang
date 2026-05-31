# Standard Library

Import a module with `import("name")`. Modules load once per VM.

## io

Console, files, in-memory buffers, and filesystem helpers.

- `print(*args)` — print arguments separated by spaces, then a newline.
- `input([prompt])` — read a line from stdin (prompt optional).
- `write(s)` — write without a trailing newline.
- `open(path, mode)` — open a file; modes `"r"`/`"w"`/`"a"` (add `"b"` for binary). Returns a file
  object supporting `read`, `readline`, `readlines`, `write`, `writelines`, `seek`, `tell`, `flush`,
  line-by-line iteration, and use as a `with` context manager.
- `BytesIO([bytes])` — an in-memory binary buffer like Python's.
- Filesystem: `exists(path)`, `remove(path)`, `rename(a, b)`, `mkdir(path)`, `getcwd()`,
  `listdir(path)`.

```kirito
var io = import("io")
with io.open("out.txt", "w") as f:
    f.write("hello\n")
for line in io.open("out.txt", "r"):
    io.print(line.strip())
```

## math

Constants `pi`, `e`, `tau`, `inf`, `nan`; functions `sqrt`, `pow`, `exp`, `log` (1- or 2-arg),
`log2`, `log10`, trig (`sin`/`cos`/`tan`/`asin`/...), hyperbolic, `floor`, `ceil`, `trunc`, `gcd`,
`lcm`, `factorial`, `gamma`, `erf`, `isnan`, `isinf`, and more.

## random

Object-based RNG, no global state. `Random([seed])` then: `random()`, `uniform(a, b)`,
`randint(a, b)`, `randrange(...)`, `choice(seq)`, `shuffle(list)`, `sample(seq, k)`.

```kirito
var rng = import("random").Random(42)
rng.randint(1, 6)
```

## itertools

Combinators returning Lists (no lazy generators). `chain(lists)`, `repeat(value, times)`,
`cycle(iter, times)`, `islice(iter, start, stop[, step])`, `accumulate(iter[, func])`,
`product(lists)`, `permutations(items[, r])`, `combinations(items, r)`, `count(start, step, stop)`.

## functools

`reduce(func, iter[, initial])`, `partial(func, bound_args_list)`.

## collections

- `deque([items])` — double-ended queue: `append`, `appendleft`, `pop`, `popleft`, `len`, indexing,
  iteration.
- `Counter([items])` — multiset: `add`, `get`, `items`, `most_common`, `[]`.
- `defaultdict(factory)` — dict that auto-creates missing values via `factory()`.

## statistics

`mean`, `median`, `mode`, `variance`, `pvariance`, `stdev`, `pstdev`.

## string

Constants `ascii_lowercase`, `ascii_uppercase`, `ascii_letters`, `digits`, `hexdigits`, `octdigits`,
`punctuation`, `whitespace`; helper `capwords(s)`.

## textwrap

`wrap(text[, width])`, `fill(text[, width])`, `indent(text, prefix)`, `dedent(text)`.

## base64

`encode(byte_list)` → String, `decode(string)` → byte List (standard alphabet).

## csv

`format_row(fields)`, `parse_row(line)`, `format(rows)`, `parse(text)` (RFC-style quoting).

## heapq

Binary min-heap over a List: `heappush(heap, x)`, `heappop(heap)`, `heapify(items)`,
`nsmallest(n, items)`.

## bisect

`bisect_left(a, x)`, `bisect_right(a, x)`, `insort_left(a, x)`, `insort_right(a, x)` on a sorted List.

## copy

`copy(obj)` (shallow), `deepcopy(obj)` (recursive, for List/Dict/Set).

## enum

`Enum(names)` — a class mapping names↔ordinals: `get(name)`, `name_of(value)`, `names()`,
`values()`, `[name]`, `name in e`.

## matrix

Dense real matrices: `Matrix(rows)`, `zeros`, `ones`, `identity`, element access `m[i, j]`,
`+ - *` (matrix/scalar), `transpose`, `determinant`, `inverse`, `trace`, `apply`.

## json

`parse(text)` (objects → Dict) and `stringify(value)`.

## serialize / dump

- `serialize` — text graph dump/load preserving shared references and cycles (`dumps`/`loads`/
  `save`/`load`).
- `dump` — compact **binary** serialization (a `Dump` blob) preserving references and cycles
  (`dumps`/`loads`/`Dump(bytes)`/`save`/`load`, plus `.bytes()`/`.size()`/`.save()`).

## zlib

`compress`/`decompress` (zlib streams), `deflate`/`inflate` (raw), `adler32`. Self-contained,
interoperable with real zlib.

## hash

`md5(s)`, `sha1(s)`, `sha256(s)` → hex digest strings.

## net

TCP sockets (`connect`/`bind`/`listen`/`accept`/`send`/`recv`) and an HTTP/1.1 client (`http_get`,
`http_post`). HTTPS is optional — build with `-DKIRITO_ENABLE_TLS=ON` (links OpenSSL).

## sys

`getenv`/`setenv`/`unsetenv`/`environ`, `platform`, `exit(code)`.

## time

`time()`, `time_ns()`, `monotonic()`, `perf_counter_ns()`, `sleep(seconds)`, and a Python-like
`datetime`/`now()` with field access and `strftime`.
