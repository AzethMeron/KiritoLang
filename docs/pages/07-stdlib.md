# Standard Library

Import a module with `import("name")`; each module loads once per VM. Every entry below lists a
signature (`name(args) → ReturnType`), what it takes, and what it does. Fixed-arity functions accept
**keyword arguments** and **defaults**; `inspect(module)` prints the same signatures live.

A leading `*args` denotes a variadic positional list; `[arg]` denotes an optional argument.

## io

Console I/O, files, in-memory buffers, and filesystem helpers.

### Console (interchangeable streams)

`io.print`/`write`/`input`/`read` act on whatever is bound to `io.stdout` / `io.stdin`, which are
rebindable stream objects — assign a `File`, a `BytesIO`, another stream, or any object exposing
`write`/`readline`/`read` to redirect I/O. The originals are kept as `io.__stdout__` /
`io.__stderr__` / `io.__stdin__`.

- `print(*args) → None` — write the arguments space-separated, newline-terminated and flushed, to the current `stdout`.
- `eprint(*args) → None` — like `print`, but to the current `stderr`.
- `write(*args) → None` — write the arguments to `stdout` with no separator and no trailing newline.
- `input([prompt]) → String` — write `prompt` (if given) to `stdout`, then read and return one line from `stdin` (without the newline).
- `read([n]) → String` — read `n` characters from `stdin`, or everything until EOF if `n` is omitted.
- `stdout` / `stderr` / `stdin` — the current standard streams (rebindable). `__stdout__` / `__stderr__` / `__stdin__` hold the originals.

### Files and buffers

- `open(path: String, mode: String = "r") → File` — open a file. Modes: `"r"` read, `"w"` truncate-write, `"a"` append, `"r+"` read/write. Raises if it can't be opened. Usable as a `with` context manager.
- `BytesIO([initial: String]) → BytesIO` — an in-memory read/write byte buffer, usable anywhere a file or stream is expected.

### Filesystem

- `exists(path: String) → Bool` — whether `path` exists.
- `isfile(path: String) → Bool` — whether `path` is a regular file.
- `isdir(path: String) → Bool` — whether `path` is a directory.
- `getsize(path: String) → Integer` — size of the file in bytes (raises if missing).
- `remove(path: String) → Bool` — delete a file; returns whether it succeeded.
- `rename(src: String, dst: String) → None` — rename/move a path (raises on failure).
- `mkdir(path: String) → Bool` — create a directory (and parents); returns success.
- `getcwd() → String` — the current working directory.
- `listdir(path: String) → List` — the entry names directly under `path`.
- `walk(dir: String) → List` — every file path under `dir`, recursively (flattened).

### Path helpers (os.path style)

- `dirname(path: String) → String` — the directory part of `path`.
- `basename(path: String) → String` — the final component of `path`.
- `splitext(path: String) → List` — `[root, ext]`, splitting off the last extension.
- `join(*parts) → String` — join path components with the platform separator.

### File object

Returned by `io.open`. Iterating a file yields its remaining lines.

- `f.read([n]) → String` — read `n` characters, or the whole rest of the file if omitted.
- `f.readline() → String` — read one line (without the trailing newline).
- `f.readlines() → List` — read all remaining lines into a List.
- `f.write(s: String) → None` — write `s` at the current position.
- `f.writelines(lines) → None` — write each String in an iterable.
- `f.seek(pos: Integer) → None` — move the read/write cursor to byte `pos`.
- `f.tell() → Integer` — the current byte position.
- `f.flush() → None` — flush buffered output.
- `f.close() → None` — close the file (also done automatically on `with` exit / collection).

### BytesIO object

- `b.write(s: String) → Integer` — write bytes at the cursor (overwriting/extending); returns the count written.
- `b.read([n]) → String` — read `n` bytes from the cursor, or the rest if omitted.
- `b.readline() → String` — read up to and including the next newline (returned without it).
- `b.getvalue() → String` — the entire buffer contents.
- `b.tell() → Integer` / `b.seek(off[, whence]) → Integer` — cursor position / move it (whence 0=start, 1=cur, 2=end).
- `b.size() → Integer` — total buffer length in bytes (`len(b)` also works).
- `b.truncate() → Integer` — drop everything after the cursor.

## math

Constants and the usual numeric functions. Argument errors raise; results are `Float` unless noted.

- Constants: `pi`, `e`, `tau`, `inf`, `nan` (all `Float`).
- `sqrt(x: Number) → Float` / `cbrt(x: Number) → Float` — square / cube root.
- `sin` / `cos` / `tan` / `asin` / `acos` / `atan` / `sinh` / `cosh` / `tanh` / `asinh` / `acosh` / `atanh` `(x: Number) → Float` — trigonometric / hyperbolic functions.
- `atan2(y: Number, x: Number) → Float` — arctangent of `y/x` using the signs of both for the quadrant.
- `hypot(x: Number, y: Number) → Float` — `sqrt(x² + y²)` without overflow.
- `exp(x: Number) → Float` / `expm1(x)` / `log1p(x)` / `log2(x)` / `log10(x)` — exponential / log family.
- `log(x: Number, base = None) → Float` — natural log, or log base `base` when given.
- `pow(x: Number, y: Number) → Float` — `x ** y` as a Float (the builtin `pow` does Integer/modular).
- `gamma(x: Number) → Float` / `lgamma(x)` / `erf(x)` — gamma, log-gamma, error function.
- `floor(x: Number) → Integer` / `ceil(x: Number) → Integer` — round down / up to an Integer.
- `trunc(x: Number) → Float` — truncate toward zero.
- `fabs(x: Number) → Float` — absolute value as a Float.
- `copysign(x: Number, y: Number) → Float` — `|x|` with the sign of `y`.
- `fmod(x: Number, y: Number) → Float` — C-style floating remainder.
- `degrees(x: Number) → Float` / `radians(x: Number) → Float` — convert radians↔degrees.
- `isnan(x: Number) → Bool` / `isinf(x) → Bool` / `isfinite(x) → Bool` — float classification.
- `gcd(a: Integer, b: Integer) → Integer` / `lcm(a, b) → Integer` — greatest common divisor / least common multiple.
- `factorial(n: Integer) → Integer` — `n!` (raises on negatives / Integer overflow).
- `comb(n: Integer, k: Integer) → Integer` — combinations “n choose k”.
- `perm(n: Integer, k: Integer) → Integer` — permutations.
- `prod(iterable[, start]) → Number` — product of the elements (Integer if all Integer, else Float).

## random

Object-based RNG — no global state; create a generator and call methods on it.

- `Random([seed: Integer]) → Random` — a Mersenne-Twister generator. With no seed (or `None`) it is seeded from the OS; with a seed it is reproducible.

### Random object

- `r.seed(n: Integer) → None` — reseed.
- `r.random() → Float` — uniform in `[0.0, 1.0)`.
- `r.uniform(a, b) → Float` — uniform in `[a, b]`.
- `r.randint(a, b) → Integer` — uniform integer in `[a, b]` (inclusive).
- `r.randrange(stop)` / `r.randrange(start, stop[, step]) → Integer` — like `range`, a random member.
- `r.choice(seq)` — a random element of a non-empty sequence.
- `r.shuffle(list) → None` — shuffle a List in place.
- `r.sample(seq, k) → List` — `k` distinct elements chosen at random.
- `r.gauss(mu, sigma)` / `r.normalvariate(mu, sigma) → Float` — normal distribution.
- `r.expovariate(lambd) → Float` — exponential distribution.

## matrix

Dense real matrices (no complex numbers).

- `Matrix(rows: List) → Matrix` — build from a nested list of numbers (rows must be equal length).
- `Matrix(rows: Integer, cols: Integer) → Matrix` — a zero matrix of the given shape.
- `zeros(rows: Integer, cols: Integer) → Matrix` / `ones(rows, cols) → Matrix` — filled with 0 / 1.
- `identity(n: Integer) → Matrix` — the n×n identity.

### Matrix object

- `m[i, j]` / `m[i, j] = v` — element access / assignment.
- `m.rows() → Integer` / `m.cols() → Integer` / `m.shape() → List` — dimensions.
- `m + n`, `m - n`, `m * n` — matrix addition/subtraction, and matrix or scalar multiplication.
- `m.transpose() → Matrix` — the transpose.
- `m.determinant() → Float` — determinant (square matrices).
- `m.inverse() → Matrix` — inverse (raises if singular).
- `m.trace() → Float` — sum of the diagonal.
- `m.apply(fn) → Matrix` — a new matrix with `fn` applied to each element.

## json

JSON parsing and serialization. `loads`/`dumps` are aliases of `parse`/`stringify`.

- `parse(text: String)` / `loads(text: String)` — parse JSON text into Kirito values (objects → Dict, arrays → List, decodes `\u` escapes and surrogate pairs). Raises a clear error on malformed input.
- `stringify(value, indent: Integer = 0) → String` / `dumps(value, indent = 0) → String` — serialize a value to JSON; compact by default, pretty-printed with `indent > 0`.

## serialize

Human-readable text serialization of an object graph, preserving shared references and cycles.

- `dumps(value) → String` — serialize to a text blob.
- `loads(text: String)` — reconstruct the value graph from a `dumps` blob.
- `save(value, path: String) → None` — `dumps` to a file.
- `load(path: String)` — `loads` from a file.

## dump

Compact **binary** serialization, preserving references and cycles (like a portable `pickle`).

- `dumps(value) → Dump` — serialize to a `Dump` blob value.
- `loads(data)` — reconstruct from a `Dump` or a byte String.
- `Dump(bytes: String) → Dump` — wrap raw bytes as a `Dump`.
- `save(value, path)` / `load(path)` — to/from a file.

## net

TCP sockets, a minimal HTTP/1.1 client, and URL helpers.

- `Socket() → Socket` — a new TCP socket.
- `httpget(url: String) → String` — perform an HTTP GET and return the response body.
- `httppost(url: String, body: String, contenttype: String = "text/plain") → String` — HTTP POST.
- `quote(s: String) → String` / `unquote(s: String) → String` — percent-encode / -decode.
- `urlencode(params: Dict) → String` — build a `k=v&...` query string (keys and values encoded).
- `parseqs(query: String) → Dict` — parse `k=v&...` into a Dict (values decoded).
- `urlsplit(url: String) → Dict` — split a URL into `scheme`/`host`/`port`/`path`/`query`/`fragment`.

### Socket object

- `s.connect(host: String, port: Integer) → None` — connect to a server.
- `s.bind(host: String, port: Integer) → None` — bind a server socket (sets `SO_REUSEADDR`).
- `s.listen([backlog: Integer]) → None` — start listening.
- `s.accept() → Socket` — accept the next connection (a new Socket).
- `s.send(data: String) → Integer` — send all of `data`; returns the byte count.
- `s.recv([n: Integer]) → String` — receive up to `n` bytes (default 4096).
- `s.recvall() → String` — receive until the peer closes.
- `s.close() → None` — close the socket.

## sys

Process environment and platform.

- `platform` — `"linux"` / `"darwin"` / `"windows"` (a `String`).
- `getenv(name: String, default = None)` — an environment variable, or `default` if unset.
- `setenv(name: String, value: String) → None` — set a variable.
- `unsetenv(name: String) → None` — remove a variable.
- `environ() → Dict` — all environment variables.
- `gettempdir() → String` — the system temp directory (honors `TMPDIR`/`TMP`/`TEMP`, falls back to
  `/tmp`). Pairs with `io` to build scratch file paths:
  `io.open(sys.joinpath(sys.gettempdir(), "scratch.txt"), "w")`.
- `joinpath(*parts) → String` — join path components with the platform separator (`os.path.join`
  semantics: a later component that is absolute resets the result). Needs at least one part.
- `exit(code: Integer = 0)` — terminate the process with the given exit code.

## time

Clocks and calendar time.

- `time() → Float` — seconds since the Unix epoch (wall clock).
- `timens() → Integer` — nanoseconds since the epoch.
- `monotonic() → Float` — seconds from a steady clock (for measuring intervals).
- `perfcounterns() → Integer` — nanoseconds from the highest-resolution clock.
- `sleep(seconds: Number) → None` — pause execution.
- `now() → DateTime` — current UTC time.
- `datetime([timestamp: Number]) → DateTime` — a `DateTime` from epoch seconds (current time if omitted).
- `make(year, month, day, hour = 0, minute = 0, second = 0) → DateTime` — build from UTC components.
- `strptime(text: String, format: String) → DateTime` — parse a time string with a strftime-style format.

### DateTime object

- `dt.year` / `month` / `day` / `hour` / `minute` / `second` / `weekday` / `yearday` / `timestamp` — Integer **attributes** (no parentheses) for the UTC fields and epoch seconds.
- `dt.iso()` / `dt.isoformat() → String` — ISO-8601 text.
- `dt.format(fmt: String) → String` — strftime-style formatting.
- `dt.add(seconds)` / `dt.sub(seconds) → DateTime` — a new DateTime shifted by seconds.
- `dt.diff(other) → Integer` — difference (`self - other`) in seconds.

## zlib

DEFLATE compression (interoperable with standard zlib), self-contained.

- `compress(data: String) → String` — zlib-format compress.
- `decompress(data: String) → String` — zlib-format decompress (raises on bad data).
- `deflate(data: String) → String` / `inflate(data: String) → String` — raw DEFLATE (no zlib header).
- `adler32(data: String) → Integer` — Adler-32 checksum.

## hash

Cryptographic hash digests (self-contained), returned as lowercase hex Strings.

- `md5(data: String) → String` — MD5 hex digest.
- `sha1(data: String) → String` — SHA-1 hex digest.
- `sha256(data: String) → String` — SHA-256 hex digest.

---

The following modules are **authored in Kirito** (frozen source compiled once per VM). Because
Kirito has no lazy generators yet, the iterator-style helpers are **eager** — they return a List
rather than a lazy sequence.

## itertools

- `count(start = 0, step = 1, stop = None) → List` — integers from `start` by `step`; supply `stop` since the result is eager.
- `repeat(value, times) → List` — `value` repeated `times` times.
- `cycle(iterable, times) → List` — the iterable repeated `times` times.
- `chain(lists) → List` — concatenate the iterables in a list-of-iterables (`chain([[1,2],[3,4]])`).
- `islice(iterable, start, stop[, step]) → List` — a slice of an iterable.
- `accumulate(iterable[, func]) → List` — running totals (or running `func` reductions).
- `product(lists) → List` — Cartesian product of a list-of-iterables (`product([[1,2],[3,4]])`).
- `permutations(items[, r]) → List` — r-length orderings.
- `combinations(items, r) → List` — r-length combinations.
- `takewhile(pred, iterable) → List` / `dropwhile(pred, iterable) → List` — the prefix where `pred` holds / the rest after it.
- `filterfalse(pred, iterable) → List` — elements where `pred` is falsy.
- `compress(data, selectors) → List` — `data` elements where the matching selector is truthy.
- `starmap(func, argtuples) → List` — `func(*args)` for each argument tuple.
- `pairwise(iterable) → List` — consecutive overlapping pairs.
- `ziplongest(lists, fillvalue = None) → List` — zip a list-of-iterables, padding short ones with `fillvalue`.
- `groupby(iterable[, key]) → List` — group consecutive elements sharing a key.

## functools

- `reduce(func, iterable[, initial])` — fold the two-argument `func` over the iterable left-to-right.
- `partial(func, bound: List) → Function` — pre-bind a list of leading arguments. The result takes a **list** of the remaining arguments and calls `func` with the combined argument list (`func` should accept a single list of arguments).
- `cache(func) → Function` — memoize a single-argument function on its argument.

## collections

- `deque([iterable]) → deque` — a double-ended queue with `append`, `appendleft`, `pop`, `popleft`, `len`, indexing, and iteration.
- `Counter([iterable]) → Counter` — a multiset/tally with `add`, `get`, `items`, `mostcommon`, and indexing.
- `defaultdict(factory) → defaultdict` — a Dict that fills a missing key by calling `factory()`.

## statistics

- `mean(data) → Float` — arithmetic mean.
- `median(data) → Float` — middle value.
- `mode(data)` / `multimode(data)` — most common value / all most-common values.
- `variance(data) → Float` / `stdev(data) → Float` — sample variance / standard deviation.
- `pvariance(data) → Float` / `pstdev(data) → Float` — population variance / standard deviation.
- `quantiles(data[, n]) → List` — cut points dividing `data` into `n` equal groups.

## string

- Constants: `ascii_letters`, `ascii_lowercase`, `ascii_uppercase`, `digits`, `hexdigits`, `octdigits`, `punctuation`, `whitespace` (all `String`).
- `capwords(s) → String` — capitalize each whitespace-separated word.

## textwrap

- `wrap(text[, width]) → List` — wrap into a list of lines.
- `fill(text[, width]) → String` — wrap into a single newline-joined String.
- `indent(text, prefix) → String` — prefix each line.
- `dedent(text) → String` — remove the common leading whitespace.

## base64

Operates on **byte values** as a `List` of Integers (0–255), not text strings.

- `encode(data: List) → String` — Base64-encode a list of byte values.
- `decode(s: String) → List` — decode Base64 text back to a list of byte values.
- `urlsafeencode(data: List) → String` / `urlsafedecode(s: String) → List` — same, with the URL-safe alphabet (`-_`).

## csv

- `parse(text)` — parse CSV text into a List of rows (each a List of fields).
- `parserow(line)` — parse one CSV line into a List of fields.
- `format(rows) → String` — serialize a List of rows to CSV text.
- `formatrow(fields) → String` — serialize one row.

## heapq

A min-heap maintained inside an ordinary List.

- `heapify(items) → List` — return a **new** heap-ordered List from `items` (does not modify `items`).
- `heappush(heap, item) → None` — push onto `heap` in place, keeping the heap invariant.
- `heappop(heap)` — pop and return the smallest element.
- `heapreplace(heap, item)` — pop the smallest, then push `item` (one pass).
- `merge(*lists) → List` — merge sorted inputs into one sorted List.
- `nlargest(n, items) → List` / `nsmallest(n, items) → List` — the n largest / smallest elements.

## bisect

Binary search / ordered insertion into a sorted List.

- `bisectleft(a, x) → Integer` / `bisectright(a, x) → Integer` — leftmost / rightmost insertion index keeping `a` sorted.
- `insortleft(a, x) → None` / `insortright(a, x) → None` — insert `x` into the sorted List `a`.

## copy

- `copy(obj)` — a shallow copy.
- `deepcopy(obj)` — a deep copy (handles shared references and cycles).

## enum

- `Enum(names: List) → Enum` — build an enumeration mapping each name to its index.

### Enum object

- `e.get(name) → Integer` / `e[name]` — the value (index) of a member; raises on an unknown name.
- `e.nameof(value) → String` — the name for a value.
- `e.names() → List` / `e.values() → List` — all member names / values.
- `name in e` — membership test.
