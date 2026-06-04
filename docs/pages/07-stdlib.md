# Standard Library

Import a module with `import("name")`; each module loads once per VM. Every entry below lists a
signature (`name(args) → ReturnType`), what it takes, and what it does. Fixed-arity functions accept
**keyword arguments** and **defaults**; `inspect(module)` prints the same signatures live.

A leading `*args` denotes a variadic positional list; `[arg]` denotes an optional argument.

---

## io

Console I/O, files, in-memory buffers, and filesystem helpers.

### Console (interchangeable streams)

`io.print`/`write`/`input`/`read` act on whatever is bound to `io.stdout` / `io.stdin`, which are
rebindable stream objects — assign a `File`, a `BytesIO`, another stream, or any object exposing
`write`/`readline`/`read` to redirect I/O. The originals are kept as `io.__stdout__` /
`io.__stderr__` / `io.__stdin__`.

- `print(*args, stream = io.stdout) → None` — write the arguments space-separated, newline-terminated and flushed.
- `eprint(*args, stream = io.stderr) → None` — like `print`, but defaulting to `stderr`.
- `write(*args, stream = io.stdout) → None` — write the arguments with no separator and no trailing newline.
- `input([prompt], stream = io.stdin) → String` — write `prompt` (if given) to `stdout`, then read and return one line from `stream` (without the newline).
- `read([n], stream = io.stdin) → String` — read `n` characters from `stream`, or everything until EOF if `n` is omitted.

The optional `stream=` keyword sends/takes that one call's output/input to/from any File, `BytesIO`, std stream, or object exposing `write`/`readline` — without rebinding `io.stdout`/`io.stdin`. Omit it to use the current standard stream.
- `stdout` — the current standard output stream (rebindable); `__stdout__` holds the original.
- `stderr` — the current standard error stream (rebindable); `__stderr__` holds the original.
- `stdin` — the current standard input stream (rebindable); `__stdin__` holds the original.

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
- `f.seek(offset: Integer, whence: Integer = 0) → Integer` — move the read/write cursor and return the
  new position. `whence` is `0` (from the start, the default), `1` (relative to the current position),
  or `2` (from the end).
- `f.tell() → Integer` — the current byte position.
- `f.flush() → None` — flush buffered output.
- `f.close() → None` — close the file (also done automatically on `with` exit / collection).

### BytesIO object

- `b.write(s: String) → Integer` — write bytes at the cursor (overwriting/extending); returns the count written.
- `b.read([n]) → String` — read `n` bytes from the cursor, or the rest if omitted.
- `b.readline() → String` — read up to and including the next newline (returned without it).
- `b.getvalue() → String` — the entire buffer contents.
- `b.tell() → Integer` — the current cursor position.
- `b.seek(off[, whence]) → Integer` — move the cursor (whence 0=start, 1=cur, 2=end).
- `b.size() → Integer` — total buffer length in bytes (`len(b)` also works).
- `b.truncate() → Integer` — drop everything after the cursor.

---

## math

Constants and the usual numeric functions. Argument errors raise; results are `Float` unless noted.

- Constants: `pi`, `e`, `tau`, `inf`, `nan` (all `Float`).
- `sqrt(x: Number) → Float` — square root.
- `cbrt(x: Number) → Float` — cube root.
- `sin(x: Number) → Float` — sine (radians).
- `cos(x: Number) → Float` — cosine (radians).
- `tan(x: Number) → Float` — tangent (radians).
- `asin(x: Number) → Float` — arcsine.
- `acos(x: Number) → Float` — arccosine.
- `atan(x: Number) → Float` — arctangent.
- `sinh(x: Number) → Float` — hyperbolic sine.
- `cosh(x: Number) → Float` — hyperbolic cosine.
- `tanh(x: Number) → Float` — hyperbolic tangent.
- `asinh(x: Number) → Float` — inverse hyperbolic sine.
- `acosh(x: Number) → Float` — inverse hyperbolic cosine.
- `atanh(x: Number) → Float` — inverse hyperbolic tangent.
- `atan2(y: Number, x: Number) → Float` — arctangent of `y/x` using the signs of both for the quadrant.
- `hypot(x: Number, y: Number) → Float` — `sqrt(x² + y²)` without overflow.
- `exp(x: Number) → Float` — `e ** x`.
- `expm1(x: Number) → Float` — `exp(x) - 1`, accurate for small `x`.
- `log1p(x: Number) → Float` — `log(1 + x)`, accurate for small `x`.
- `log2(x: Number) → Float` — base-2 logarithm.
- `log10(x: Number) → Float` — base-10 logarithm.
- `log(x: Number, base = None) → Float` — natural log, or log base `base` when given.
- `pow(x: Number, y: Number) → Float` — `x ** y` as a Float (the builtin `pow` does Integer/modular).
- `gamma(x: Number) → Float` — the gamma function.
- `lgamma(x: Number) → Float` — the natural log of the absolute value of gamma.
- `erf(x: Number) → Float` — the error function.
- `erfc(x: Number) → Float` — the complementary error function (`1 - erf(x)`, accurate for large `x`).
- `floor(x: Number) → Integer` — round down to an Integer.
- `ceil(x: Number) → Integer` — round up to an Integer.
- `trunc(x: Number) → Float` — truncate toward zero.
- `fabs(x: Number) → Float` — absolute value as a Float.
- `copysign(x: Number, y: Number) → Float` — `|x|` with the sign of `y`.
- `fmod(x: Number, y: Number) → Float` — C-style floating remainder.
- `degrees(x: Number) → Float` — convert radians to degrees.
- `radians(x: Number) → Float` — convert degrees to radians.
- `isnan(x: Number) → Bool` — whether `x` is NaN.
- `isinf(x: Number) → Bool` — whether `x` is infinite.
- `isfinite(x: Number) → Bool` — whether `x` is finite (neither NaN nor infinite).
- `gcd(a: Integer, b: Integer) → Integer` — greatest common divisor.
- `lcm(a: Integer, b: Integer) → Integer` — least common multiple.
- `factorial(n: Integer) → Integer` — `n!` (raises on negatives / Integer overflow).
- `comb(n: Integer, k: Integer) → Integer` — combinations “n choose k”.
- `perm(n: Integer, k: Integer) → Integer` — permutations.
- `prod(iterable, start = 1) → Number` — product of the elements times `start` (Integer if all Integer, else Float).

---

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
- `r.gauss(mu, sigma) → Float` — a sample from a normal distribution.
- `r.normalvariate(mu, sigma) → Float` — a sample from a normal distribution (an alias of `gauss`).
- `r.expovariate(lambd) → Float` — exponential distribution.

---

## matrix

Dense real matrices — a 2-D `tensor` of doubles, with the familiar matrix API and the
`*`-means-matrix-multiply convention. For complex-valued numbers and matrices, see the `complex`
module below; for arbitrary-rank arrays, see `tensor`.

- `Matrix(rows: List) → Matrix` — build from a nested list of numbers (rows must be equal length).
- `Matrix(rows: Integer, cols: Integer) → Matrix` — a zero matrix of the given shape.
- `zeros(rows: Integer, cols: Integer) → Matrix` — a matrix filled with `0`.
- `ones(rows: Integer, cols: Integer) → Matrix` — a matrix filled with `1`.
- `identity(n: Integer) → Matrix` — the n×n identity.
- `vector(values: List) → Matrix` — a 1×n row vector from a flat list of numbers.

Matrices are arbitrary-shape (any rows × cols). Shape-specific operations (`determinant`, `inverse`,
`trace`) require a square matrix and raise otherwise; `*` requires conformable inner dimensions.

### Matrix object

- `m[i, j] → Float` — element access.
- `m[i] → List` — the whole row `i` as a List of Floats.
- `m[i, j] = v` — element assignment.
- `m1 == m2 → Bool` — element-wise equality (within a small tolerance).
- `m.get(i, j) → Float` — explicit element access (the method form of `m[i, j]`).
- `m.set(i, j, v) → None` — explicit element assignment (the method form of `m[i, j] = v`).
- `m.rows() → Integer` — the number of rows.
- `m.cols() → Integer` — the number of columns.
- `m.shape() → List` — `[rows, cols]`.
- `m.row(i) → List` — the `i`-th row as a List of its elements.
- `m + n`, `m - n`, `m * n` — matrix addition/subtraction, and matrix or scalar multiplication.
- `m.transpose() → Matrix` — the transpose.
- `m.determinant() → Float` — determinant (square matrices).
- `m.inverse() → Matrix` — inverse (raises if singular).
- `m.trace() → Float` — sum of the diagonal.
- `m.sum() → Float` — sum of every element.
- `m.apply(fn) → Matrix` — a new matrix with `fn` applied to each element.

**Vector operations** (a Matrix with one dimension equal to 1 is a vector):

- `u.dot(v) → Float` — the scalar (dot) product of two equal-length vectors (any orientation).
  (`*` is always matrix multiply, never a dot product.)
- `u.cross(v) → Matrix` — the cross product of two 3-element vectors (result keeps `u`'s orientation).
- `m.norm() → Float` — the Euclidean (Frobenius) 2-norm `sqrt(Σ xᵢ²)` — the length of a vector.

---

## complex

Complex numbers and complex matrices, implemented in C++ (`std::complex<double>`). Reals coerce to
the real axis, so any function or operator below also accepts plain `Integer`/`Float` arguments.

### Constructors and constants

- `Complex(re: Number, im: Number = 0) → Complex` — build `re + im·i`.
- `of(re: Number, im: Number) → Complex` — the two-argument constructor.
- `real(re: Number) → Complex` — a real number on the complex plane (`re + 0i`).
- `polar(r: Number, theta: Number) → Complex` — from polar form, `r·(cos θ + i·sin θ)`.
- `i`, `zero`, `one` — the imaginary unit, `0`, and `1` as `Complex` values.
- `pi`, `e`, `tau` — the usual real constants (Floats), for convenience.

### Complex object

- `z.re → Float`, `z.im → Float` — the real and imaginary parts (also `z.real`/`z.imag`).
- `z1 + z2`, `z1 - z2`, `z1 * z2`, `z1 / z2`, `z1 ** z2`, `-z` — arithmetic. A `Complex` must be the
  left operand when mixing with a number (`z + 2`, not `2 + z`). Division by zero raises.
- `z1 == z2 → Bool` — equality within a small tolerance; a `Complex` with zero imaginary part also
  equals the matching real number (`Complex(2, 0) == 2`).
- Complex numbers are **unordered**: `<`, `<=`, `>`, `>=` raise.
- `z.conjugate() → Complex` — the complex conjugate.
- `z.modulus() → Float` — the magnitude `|z|` (also `z.magnitude()` / `z.abs()`).
- `z.argument() → Float` — the phase angle in radians (also `z.arg()` / `z.phase()`).
- `z.norm2() → Float` — the squared magnitude `|z|²` (no square root).
- `z.is_zero() → Bool` — True when `z` is (numerically) zero.

### Module functions

Scalar reductions (one per line):

- `modulus(z) → Float` — the magnitude `|z|`.
- `abs(z) → Float` — alias of `modulus`.
- `phase(z) → Float` — the phase angle, in radians.
- `argument(z) → Float` — alias of `phase`.
- `norm2(z) → Float` — the squared magnitude `|z|²`.
- `conjugate(z) → Complex` — the complex conjugate.

The analytic math set — the complex extensions of the `math` functions — each take a `Complex` or a
number and return a `Complex`:

- `exp(z)`
- `log(z)` — natural logarithm (principal branch).
- `log10(z)`
- `sqrt(z)` — principal square root.
- `cbrt(z)` — principal cube root.
- `pow(z, w)` — `z` raised to the power `w`.
- `sin(z)`
- `cos(z)`
- `tan(z)`
- `asin(z)`
- `acos(z)`
- `atan(z)`
- `sinh(z)`
- `cosh(z)`
- `tanh(z)`
- `asinh(z)`
- `acosh(z)`
- `atanh(z)`

### Complex matrices

- `Matrix(rows: List) → ComplexMatrix` — build from a nested list whose cells are `Complex` values
  or numbers (rows must be equal length).
- `zeros(rows, cols) → ComplexMatrix` — a zero matrix.
- `ones(rows, cols) → ComplexMatrix` — a matrix of `1+0i`.
- `identity(n) → ComplexMatrix` — the n×n identity.
- `vector(values: List) → ComplexMatrix` — a 1×n complex row vector.

Like the real `matrix`, complex matrices are arbitrary-shape; `determinant`/`inverse`/`trace` need a
square matrix and raise otherwise.

#### ComplexMatrix object

- `m[i, j] → Complex`, `m[i] → List`, `m[i, j] = v` — element / row access and assignment.
- `m.get(i, j)`, `m.set(i, j, v)`, `m.row(i)`, `m.rows()`, `m.cols()`, `m.shape()`.
- `m1 == m2 → Bool` — element-wise equality (within a small tolerance).
- `m + n`, `m - n`, `m * n` — addition/subtraction and matrix or scalar (`Complex`/number) multiply.
- `m.transpose() → ComplexMatrix` — the transpose.
- `m.conjugate() → ComplexMatrix` — element-wise complex conjugate.
- `m.hermitian() → ComplexMatrix` — the conjugate transpose (also `m.conjugatetranspose()`).
- `m.determinant() → Complex` — determinant via **Gaussian elimination** with partial pivoting.
- `m.inverse() → ComplexMatrix` — inverse via **fast O(n³) Gauss-Jordan** elimination (raises if
  singular).
- `m.trace() → Complex` — sum of the diagonal.

**Vector operations** (a ComplexMatrix with one dimension equal to 1 is a vector):

- `u.dot(v) → Complex` — the scalar (Hermitian inner) product of two equal-length vectors:
  `Σ conj(uᵢ)·vᵢ`, so `u.dot(u) = Σ |uᵢ|²` is real and non-negative. (`*` is always matrix multiply.)
- `u.cross(v) → ComplexMatrix` — the cross product of two 3-element vectors.
- `m.norm() → Float` — the Euclidean 2-norm `sqrt(Σ |zᵢ|²)`.

---

## tensor

Dense **N-dimensional** arrays — the generalization of a matrix to any rank. The element type
(**dtype**) is chosen at construction: `"Float"` (double, the default) or `"Complex"`. The numeric
engine is shared C++ (`src/kirito/tensor.hpp`) and is what the `matrix` and `complex` matrix types are
themselves built on; a 2-D tensor *is* a matrix. It is CPU-only but carries a **reverse-mode autograd**
(see below) and a GPU-forward-compatible single-buffer design.

### Constructors and factories

Each constructor/factory takes an optional **`requiresgrad`** keyword (default `False`) that marks the
result as a differentiable leaf (Float only — see [Autograd](#autograd)).

- `Tensor(data: List, dtype = "Float", requiresgrad = False) → Tensor` — build from a (rectangular)
  nested list; the nesting depth sets the rank. `tensor` is an alias of `Tensor`.
- `zeros(shape: List, dtype = "Float", requiresgrad = False) → Tensor` — a tensor of zeros.
- `ones(shape: List, dtype = "Float", requiresgrad = False) → Tensor` — a tensor of ones.
- `full(shape: List, value: Number, dtype = "Float", requiresgrad = False) → Tensor` — filled with `value`.
- `eye(n: Integer, dtype = "Float", requiresgrad = False) → Tensor` — the n×n identity matrix.
- `arange(stop)` / `arange(start, stop[, step]) → Tensor` — a 1-D ramp (like Python's `range`, as
  Floats).

### Tensor object

- `t.shape() → List`, `t.ndim() → Integer`, `t.size() → Integer`, `t.dtype() → String`.
- `t[i, j, ...] → Number` — a **full** index (one per dimension) returns the scalar element.
- `t[i] → Tensor` — a **partial** index returns the sub-tensor of the remaining axes.
- `t[i, j, ...] = v` — assign an element (full index).
- `a + b`, `a - b`, `a * b`, `a / b` — **element-wise** with NumPy **broadcasting** (axes align from
  the right; each must be equal or 1). `*` is element-wise (Hadamard), **not** matrix multiply.
  Mixing a `Float` and a `Complex` tensor promotes the result to `Complex`. A scalar operand applies
  element-wise (`t * 2`).
- `-t` — element-wise negation.
- `a == b → Bool` — equal shape and element-wise equality (within a tolerance).
- `t.matmul(other) → Tensor` — matrix product (2-D), or **batched** over the leading dimensions for
  rank ≥ 2.
- `t.dot(other) → Number` — the dot product of two 1-D tensors.
- `t.transpose() → Tensor` — reverse all axes (the matrix transpose when 2-D).
- `t.permute(axes: List) → Tensor` — reorder axes by the given permutation.
- `t.reshape(shape: List) → Tensor` — same elements, new shape.
- `t.flatten() → Tensor` — a 1-D copy.
- `t.apply(fn) → Tensor` — a new tensor with `fn` mapped over every element (the element-wise map).
- `t.astype(dtype: String) → Tensor` — convert dtype (`Float → Complex`, or `Complex → Float` keeping
  the real part).
- `t.sum(axis = None)`, `t.mean(axis = None)`, `t.prod(axis = None)` — reduce the whole tensor to a
  scalar, or one `axis` to a lower-rank tensor.
- `t.min(axis = None)`, `t.max(axis = None)` — extremes (whole-tensor or along an axis; raise for a
  `Complex` tensor, which is unordered).

### Indexing & slicing

- `t[i, j, ...]` — integer index (full → element, partial → sub-tensor); `t[i, j] = v` assigns.
- `t[start:stop:step]` — slice the **first** axis (Python semantics; returns a detached copy).
- `t[mask]` — boolean selection where `mask` is a same-shape 0/1 tensor (→ 1-D).
- `t[[i, j, k]]` — fancy index: gather those rows along axis 0.
- `t.slice(start, stop, step, axis = 0) → Tensor` and `t.take(indices, axis = 0) → Tensor` — the
  **autograd-aware** forms of slicing / row-gather (the gradient scatters back).

### Comparisons, logic, selection

- `t.eq/ne/lt/le/gt/ge(other) → Tensor` — element-wise comparisons returning a 0/1 mask (the
  `< <= > >=` operators do the same; `==`/`!=` stay whole-tensor `Bool`). Float only.
- `t.logicaland/logicalor/logicalxor(other)`, `t.logicalnot()` — element-wise logic on 0/1 masks.
- `where(cond, a, b) → Tensor` — element-wise select (`cond` non-zero → `a` else `b`); differentiable.
- `t.clip(lo, hi)`, `t.maximum(other)`, `t.minimum(other)` — clamp / element-wise max / min;
  differentiable.

### More reductions

- `t.argmin(axis = None)`, `t.argmax(axis = None)` — index of the extreme.
- `t.std(axis = None, ddof = 0)`, `t.var(axis = None, ddof = 0)` — standard deviation / variance.
- `t.all(axis = None)`, `t.any(axis = None)` — truth reductions.
- `t.ptp(axis = None)` — max − min; `t.median(axis = None)`.
- `t.cumsum(axis = None)` (differentiable) / `t.cumprod(axis = None)` — cumulative scans
  (`axis = None` flattens first).

### Structural ops

- `t.squeeze(axis = None)`, `t.expanddims(axis)`, `t.swapaxes(a1, a2)`, `t.flip(axis = None)`,
  `t.broadcastto(shape)`, `t.repeat(count, axis = None)`, `t.tile(reps)` — all differentiable except
  `repeat`/`tile`.
- `concatenate(tensors, axis = 0)` (alias `concat`), `stack(tensors, axis = 0)`,
  `split(t, sections, axis = 0)` — join / split a List of tensors (differentiable). `sections` is an
  Integer (equal parts) or a List of sizes.

### Creation helpers

- `linspace(start, stop, num = 50)`, `zeroslike(t)`, `oneslike(t)`, `fulllike(t, value)`,
  `identity(n)` (alias of `eye`), `diag(t, k = 0)` (1-D → diagonal matrix, 2-D → its diagonal),
  `tril(t, k = 0)` / `triu(t, k = 0)` (lower / upper triangle).

### Linear algebra (module functions, 2-D)

- `det(t)`, `inv(t)`, `solve(a, b)`, `trace(t)`, `norm(t, ord = 2)`, `outer(a, b)`, `inner(a, b)`,
  `kron(a, b)`, `cross(a, b)` (3-vectors), and `einsum(spec, *tensors)` — a general Einstein-summation
  (transpose / diagonal / trace / contraction / outer, any subscript string). Work on both dtypes
  where it makes sense (forward only; the differentiable linear algebra is `matmul`/`tensordot`).

### Sorting & search

- `t.sort(axis = None)`, `t.argsort(axis = None)` (default last axis), `unique(t)` (sorted unique 1-D),
  `nonzero(t)` (a List of per-axis index tensors), `searchsorted(a, v)` (insertion indices into a
  sorted 1-D `a`).

### Complex helpers

- `t.real()`, `t.imag()`, `t.angle()` → Float tensors; `t.conj()` → the conjugate (Complex).

### Differentiable element-wise math

Every one of these returns a new tensor with the function applied element-wise, and each is
**differentiable** under autograd (Float only):

- exponential / log: `exp`, `log`, `log10`, `log2`, `softplus`, `erf`
- powers / roots: `sqrt`, `cbrt`, `square`, `reciprocal`, `pow(p)`
- sign / magnitude / rounding: `abs`, `sign`, `floor`, `ceil`, `round`, `trunc` (gradient zero)
- trigonometric: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`
- hyperbolic: `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`
- neural-net: `relu`, `sigmoid`

### tensordot / contract

General tensor contraction over chosen axes (built from `permute`/`reshape`/`matmul`, so it is
differentiable):

- `tensordot(a: Tensor, b: Tensor, axes = 2) → Tensor` — `axes` is an **Integer** `N` (contract the
  last `N` axes of `a` with the first `N` of `b` — `N = 1` is matrix multiply, `N = 2` is the
  Frobenius double-contraction to a scalar) or a **`[a-axes, b-axes]`** pair (each an Integer or a
  List of Integers) naming the axes to pair up.
- `contract(a: Tensor, b: Tensor, aaxes, baxes) → Tensor` — `tensordot` with the two axis lists given
  explicitly.

### Autograd

Reverse-mode automatic differentiation, opt-in and **Float-only**. Tensors **do not** track gradients
by default; mark a leaf with `requiresgrad = True` (constructor keyword) or `t.requiresgrad(True)`
post-creation. Differentiable operations then record a computational graph; `backward()` walks it and
accumulates each tensor's gradient. The graph records *operations*, not where the data lives, so the
design carries forward to a future GPU backend.

Autograd applies to the **Float** dtype only: a **Complex tensor has no gradients** — marking one with
`requiresgrad = True` (constructor keyword or method) raises, and the differentiable element-wise math
methods are Float-only as well. Complex tensors remain a full numeric container (arithmetic, `matmul`,
`tensordot`, reshaping, reductions); use the `complex` module for complex analytic functions.

- `t.requiresgrad() → Bool` — whether `t` tracks gradients; `t.requiresgrad(flag)` sets it (Float
  only; turning it off detaches `t` from the graph).
- `t.grad → Tensor` — the accumulated gradient (same shape as `t`), or `None` before `backward()`.
- `t.backward(seed = None) → None` — propagate gradients back from `t`. With no `seed`, `t` must be a
  scalar (a 0-D or single-element tensor) and the seed is `1`; otherwise pass a seed tensor of `t`'s
  shape. Gradients **accumulate** into `.grad` (call `zerograd()` between steps).
- `t.zerograd() → None` — clear `t.grad`.
- `t.detach() → Tensor` — a copy that shares the data but tracks no gradient (stops gradient flow).
- `nograd()` — a context manager: inside `with tensor.nograd():` no operation tracks gradients (for
  inference or for in-place parameter updates).

**Differentiable ops:** `+ - * /` (with broadcasting), `matmul`, `tensordot`/`contract`, `sum`/`mean`
(whole-tensor or per-axis), `transpose`/`permute`/`reshape`/`flatten`, unary `-`, and the
differentiable math set above. Non-differentiable ops — `apply` (an arbitrary function), `min`/`max`,
`prod`, indexing — detach (stop the gradient). On a grad-tracking tensor, a whole-tensor `sum`/`mean`
returns a 0-D tensor (so the graph continues) rather than a plain `Float`.

```kirito
var io = import("io")
var T = import("tensor")

# fit y = 2x + 1 by gradient descent
var xs = T.Tensor([[0], [1], [2], [3]])
var ys = T.Tensor([[1], [3], [5], [7]])
var w = T.zeros([1, 1], requiresgrad = True)
var b = T.zeros([1, 1], requiresgrad = True)
var step = 0
while step < 500:
    var loss = (xs.matmul(w) + b - ys).square().mean()
    w.zerograd()
    b.zerograd()
    loss.backward()
    with T.nograd():
        w = w - w.grad * 0.05
        b = b - b.grad * 0.05
    w.requiresgrad(True)
    b.requiresgrad(True)
    step = step + 1
io.print(w[0, 0], b[0, 0])      # ~ 2.0  ~ 1.0
```

---

## json

JSON parsing and serialization (flat data interchange — for reference/cycle-preserving snapshots see
`serialize` / `dump` below). `loads` and `dumps` are aliases of `parse` and `stringify`.

- `parse(text: String)` — parse JSON text into Kirito values (objects → Dict, arrays → List, decodes `\u` escapes and surrogate pairs). Raises a clear error on malformed input.
- `loads(text: String)` — an alias of `parse`.
- `stringify(value, indent: Integer = 0) → String` — serialize a value to JSON; compact by default, pretty-printed when the indent width is greater than zero.
- `dumps(value, indent: Integer = 0) → String` — an alias of `stringify`.

---

## serialize

`serialize` and `dump` are **two formats of the same thing** — full object-graph serialization that
preserves shared references and cycles (a `pickle`-style snapshot, unlike `json` which is flat data
interchange with no aliasing). They share one graph walk and reconstruction core and differ only in
output: **`serialize` is human-readable text**, **`dump` is compact binary**. Supported value types:
`None`/`Bool`/`Integer`/`Float`/`String`/`List`/`Dict`/`Set`, **plus user `class` instances**.

A class instance is serialized **by its attributes** by default and reconstructed by looking the
class up by name in the loading VM (so the class must be defined there; `_init_` is *not* re-run). A
class can override this with the **`_getstate_`/`_setstate_` protocol**: `_getstate_(self)` returns
the serializable state to store, and `_setstate_(self, state)` restores it — useful to drop transient
fields (recomputing them on load) or to reduce a value to plain serializable data. A native (C++)
type participates the same way: define `_getstate_`/`_setstate_` and register a reconstructor with
`vm.registerDeserializer(typeName, factory)`. (`json` has no object notion, so it can't serialize
instances.)

Human-readable **text** serialization → a `String`.

- `dumps(value) → String` — serialize to a text blob.
- `loads(text: String)` — reconstruct the value graph from a `dumps` blob.
- `save(value, path: String) → None` — `dumps` to a file.
- `load(path: String)` — `loads` from a file.

---

## dump

Compact **binary** serialization (the binary counterpart of `serialize`), preserving references and
cycles like a portable `pickle`. Produces a `Dump` blob value rather than text.

- `dumps(value) → Dump` — serialize to a `Dump` blob value.
- `loads(data)` — reconstruct from a `Dump` or a byte String.
- `Dump(bytes: String) → Dump` — wrap raw bytes as a `Dump`.
- `save(value, path) → None` — serialize `value` to a file.
- `load(path)` — reconstruct a value from a file written by `save`.

---

## net

TCP sockets, a full-fledged HTTP/1.1 client, and URL helpers.

### HTTP client

- `request(method: String, url: String, options: Dict = None) → Response` — perform any HTTP request.
- `get(url: String, options: Dict = None) → Response` — a `GET` request.
- `post(url: String, options: Dict = None) → Response` — a `POST` request.
- `put(url: String, options: Dict = None) → Response` — a `PUT` request.
- `delete(url: String, options: Dict = None) → Response` — a `DELETE` request.
- `patch(url: String, options: Dict = None) → Response` — a `PATCH` request.
- `head(url: String, options: Dict = None) → Response` — a `HEAD` request.
- `options(url: String, options: Dict = None) → Response` — an `OPTIONS` request.
- `Session() → Session` — a session that persists a cookie jar (`.cookies`) and default headers
  (`.headers`) across requests; has the same verb methods (`s.get(url[, options])`, …).

The `options` Dict may contain: `headers` (Dict), `params` (Dict → query string), `data` (String or
form-Dict), `json` (any value → JSON body + `application/json`), `files` (Dict → `multipart/form-data`
upload; value is content or `[filename, content]`), `auth` (`[user, pass]` → HTTP Basic), `timeout`
(seconds), `allowredirects` (Bool, default `True`) / `maxredirects` (Integer, default 10), `verify`
(Bool, default `True` — TLS certificate verification), and `cookies` (Dict). Redirects are followed,
chunked transfer-encoding is decoded, and `gzip`/`deflate` responses are decompressed automatically.

### Response object

- `r.status` (`Integer`, alias `r.statuscode`), `r.reason` (`String`), `r.ok` (`Bool`, true for a 1xx–3xx status, i.e. `100 ≤ status < 400`).
- `r.url` — the final URL (after any redirects).
- `r.text` — the response body (`String`); `r.body` and `r.content` are aliases of it.
- `r.headers` — a Dict of response headers; `r.header(name)` looks one up **case-insensitively**.
- `r.cookies` — a Dict of cookies set by the server.
- `r.json()` — parse the body as JSON.
- `r.raiseforstatus()` — throw if the status is ≥ 400, else return the response.

### URL helpers (`urllib.parse` style)

- `quote(s: String) → String` — percent-encode a String (UTF-8).
- `unquote(s: String) → String` — percent-decode a String (UTF-8).
- `urlencode(params: Dict) → String` — build a `k=v&...` query string (keys and values encoded).
- `parseqs(query: String) → Dict` — parse `k=v&...` into a Dict (values decoded).
- `urlsplit(url: String) → Dict` — split a URL into `scheme`/`host`/`port`/`path`/`query`/`fragment`.

### Socket object

- `Socket() → Socket` — a new TCP socket.
- `s.connect(host: String, port: Integer) → None` — connect to a server.
- `s.bind(host: String, port: Integer) → None` — bind a server socket (sets `SO_REUSEADDR`).
- `s.listen([backlog: Integer]) → None` — start listening.
- `s.accept() → Socket` — accept the next connection (a new Socket).
- `s.send(data: String) → Integer` — send all of `data`; returns the byte count.
- `s.recv([n: Integer]) → String` — receive up to `n` bytes (default 4096).
- `s.recvall() → String` — receive until the peer closes.
- `s.settimeout(seconds) → None` — bound subsequent send/recv with a timeout.
- `s.close() → None` — close the socket.

---

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
- `joinpath(*parts) → String` — join path components with `/` (`os.path.join` semantics: a later
  component that is absolute resets the result). Needs at least one part.
- `exit(code: Integer = 0)` — terminate the process with the given exit code.

---

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

The UTC fields and epoch seconds are Integer **attributes** (no parentheses):

- `dt.year` — the year.
- `dt.month` — the month (1–12).
- `dt.day` — the day of the month.
- `dt.hour` — the hour (0–23).
- `dt.minute` — the minute (0–59).
- `dt.second` — the second (0–59).
- `dt.weekday` — the day of the week.
- `dt.yearday` — the day of the year.
- `dt.timestamp` — epoch seconds.

Its methods:

- `dt.iso() → String` — ISO-8601 text; `dt.isoformat()` is an alias.
- `dt.format(fmt: String) → String` — strftime-style formatting.
- `dt.add(seconds) → DateTime` — a new DateTime shifted forward by `seconds`.
- `dt.sub(seconds) → DateTime` — a new DateTime shifted back by `seconds`.
- `dt.diff(other) → Integer` — difference (`self - other`) in seconds.

---

## zlib

DEFLATE compression (interoperable with standard zlib), self-contained.

- `compress(data: String) → String` — zlib-format compress.
- `decompress(data: String) → String` — zlib-format decompress (raises on bad data).
- `deflate(data: String) → String` — raw DEFLATE compression (no zlib header).
- `inflate(data: String) → String` — raw DEFLATE decompression (no zlib header).
- `adler32(data: String) → Integer` — Adler-32 checksum.

---

## hash

Cryptographic hash digests (self-contained), returned as lowercase hex Strings.

- `md5(data: String) → String` — MD5 hex digest.
- `sha1(data: String) → String` — SHA-1 hex digest.
- `sha256(data: String) → String` — SHA-256 hex digest.

---

## regex

Regular expressions with a **guaranteed linear-time** match. The engine compiles the pattern to a
bytecode program and simulates a Thompson NFA (Pike's algorithm, tracking capture positions), so
matching is O(text × pattern) with **no catastrophic backtracking** — a pattern like `(a+)+b` against
a long input is instant, not exponential. The cost of that guarantee (the same trade-off RE2 makes)
is that two backtracking-only constructs are deliberately **not supported** and raise a clear error:
**backreferences** (`\1`) and **lookaround** (`(?=…)`, `(?!…)`, `(?<=…)`, `(?<!…)`).

The API mirrors Python's `re`. All positions and spans are **code-point indices** (consistent with
Kirito's String indexing). Flags combine with `+`: `re.IGNORECASE` (alias `re.I`), `re.MULTILINE`
(`re.M`), `re.DOTALL` (`re.S`).

### Supported syntax

Literals and `.` (any char except newline; `\n` too under DOTALL); character classes `[...]`,
`[^...]`, ranges `a-z`; shorthands `\d \D \w \W \s \S` (ASCII); anchors `^ $`, `\b \B`, `\A \z \Z`;
groups `(...)`, non-capturing `(?:...)`, named `(?P<name>...)` or `(?<name>...)`; alternation `|`;
quantifiers `* + ?`, `{n}`, `{n,}`, `{n,m}`, each greedy or **lazy** with a trailing `?`; escapes
`\n \t \r \f \v \a \xHH \uHHHH`, octal `\0`/`\NNN` (and `\b` is a backspace *inside* a class), and
any escaped metacharacter; inline flags `(?i)` / `(?m)` / `(?s)`.

The engine is validated against the full classic Spencer/PCRE/Python-`re` test corpus (run through
Kirito in `tools/tests/scripts/spec_regex_corpus.ki`): zero false positives/negatives, and every
unsupported-feature or invalid pattern is rejected with a clean error rather than crashing.

### Module functions

- `compile(pattern: String, flags: Integer = 0) → Regex` — compile once, reuse many times.
- `match(pattern: String, string: String, flags: Integer = 0)` — match anchored at the start, or `None`.
- `search(pattern: String, string: String, flags: Integer = 0)` — first match anywhere, or `None`.
- `fullmatch(pattern: String, string: String, flags: Integer = 0)` — match that covers the whole string, or `None`.
- `findall(pattern: String, string: String, flags: Integer = 0) → List` — all matches (see Regex.findall for the shape).
- `finditer(pattern: String, string: String, flags: Integer = 0) → List` — a List of `Match` objects.
- `sub(pattern: String, repl, string: String, count: Integer = 0) → String` — substitute matches.
- `split(pattern: String, string: String, maxsplit: Integer = 0) → List` — split on matches.
- `escape(s: String) → String` — backslash-escape regex metacharacters so `s` matches literally.

### Regex object

Returned by `compile`. The search methods take the subject `string` plus an optional start `pos`
(default 0) and end `endpos` (default the string length); `endpos` makes the subject look exactly
that many code points long, so `$`/`\b` anchor there. Attributes: `r.pattern`, `r.groups` (group
count), `r.groupindex` (name → group number).

- `r.match(string[, pos[, endpos]]) → Match` — anchored match at `pos`, or `None`.
- `r.search(string[, pos[, endpos]]) → Match` — first match at/after `pos`, or `None`.
- `r.fullmatch(string[, pos[, endpos]]) → Match` — whole-(sub)string match, or `None`.
- `r.findall(string[, pos[, endpos]]) → List` — with **0** groups: a List of the matched Strings; with **1** group: a List of that group's Strings; with **2+** groups: a List of per-match group Lists.
- `r.finditer(string) → List` — a List of `Match` objects, one per non-overlapping match.
- `r.sub(repl, string[, count]) → String` — replace matches. `repl` is either a template String (`\1`, `\g<name>`, `\g<0>`, `\\`) or a **function** taking a `Match` and returning a String. `count = 0` replaces all.
- `r.split(string[, maxsplit]) → List` — split around matches; any captured groups are interleaved into the result (like Python).
- `r.pattern` — the source pattern String.
- `r.groups` — the number of capturing groups.
- `r.groupindex` — a Dict mapping each named group to its number.

### Match object

Returned by a successful `match`/`search`/`fullmatch` (and by `finditer`):

- `m.group([key]) → String` — the whole match (no arg or `0`), or group `key` (a number or a name); `None` if that group didn't participate. Several keys return a List.
- `m.groups([default]) → List` — all capturing groups (1..n); non-participating groups are `default` (default `None`).
- `m.groupdict([default]) → Dict` — named groups by name.
- `m.start([key]) → Integer` / `m.end([key]) → Integer` — code-point start/end of the whole match or a group (`-1` if absent).
- `m.span([key]) → List` — `[start, end]`.
- `m.string` — the subject the match was found in.

```kirito
var re = import("regex")
var m = re.search("(?P<user>\\w+)@(?P<host>[\\w.]+)", "contact ada@kirito.dev now")
io.print(m.group())            # => ada@kirito.dev
io.print(m.group("user"))      # => ada
io.print(m.groupdict())        # => {user: ada, host: kirito.dev}  (order may vary)

io.print(re.findall("\\d+", "12 and 345"))               # => [12, 345]
io.print(re.sub("\\s+", "_", "a   b  c"))                 # => a_b_c
var rx = re.compile("cat|dog", re.IGNORECASE)
io.print(rx.findall("Cat dog CAT"))                        # => [Cat, dog, CAT]
```

---

The following modules are **authored in Kirito** (frozen source compiled once per VM). Because
Kirito has no lazy generators yet, the iterator-style helpers are **eager** — they return a List
rather than a lazy sequence.

---

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
- `takewhile(pred, iterable) → List` — the leading run of elements while `pred` holds.
- `dropwhile(pred, iterable) → List` — the rest, after that leading run.
- `filterfalse(pred, iterable) → List` — elements where `pred` is falsy.
- `compress(data, selectors) → List` — `data` elements where the matching selector is truthy.
- `starmap(func, argtuples) → List` — call `func` once per tuple, passing the whole tuple as a single
  List argument (Kirito has no `*args` spread): `func(t)` where `t` is `[a, b, …]`.
- `pairwise(iterable) → List` — consecutive overlapping pairs.
- `ziplongest(lists, fillvalue = None) → List` — zip a list-of-iterables, padding short ones with `fillvalue`.
- `groupby(iterable[, key]) → List` — group consecutive elements sharing a key.

---

## functools

- `reduce(func, iterable[, initial])` — fold the two-argument `func` over the iterable left-to-right.
- `partial(func, bound: List) → Function` — pre-bind a list of leading arguments. The result takes a **list** of the remaining arguments and calls `func` with the combined argument list (`func` should accept a single list of arguments).
- `cache(func) → Function` — memoize a single-argument function on its argument.

---

## collections

- `deque([iterable]) → deque` — a double-ended queue.
- `Counter([iterable]) → Counter` — a multiset that tallies its elements.
- `defaultdict(factory) → defaultdict` — a Dict that fills a missing key by calling `factory()`.

### deque object

- `dq.append(x) → None` — add `x` to the right end.
- `dq.appendleft(x) → None` — add `x` to the left end.
- `dq.pop()` — remove and return the rightmost element.
- `dq.popleft()` — remove and return the leftmost element.
- `dq[i]` — the element at index `i`.
- `len(dq) → Integer` — the number of elements.
- iterable — a `for` loop yields the elements left to right.

### Counter object

- `c.add(x) → None` — increment the count for `x`.
- `c.get(x) → Integer` — the count for `x` (`0` if unseen).
- `c[x] → Integer` — index syntax for the count of `x`.
- `c.items() → List` — `[value, count]` pairs.
- `c.mostcommon() → List` — `[value, count]` pairs, highest count first.

### defaultdict object

- `d[k]` — the value for `k`, inserting `factory()` if `k` is absent.
- `d[k] = v` — set the value for `k`.
- `k in d → Bool` — whether `k` is present.
- `d.keys() → List` — all keys.
- `d.values() → List` — all values.
- `d.items() → List` — all `[key, value]` pairs.

---

## statistics

- `mean(data) → Float` — arithmetic mean.
- `median(data) → Float` — middle value.
- `mode(data)` — the single most common value.
- `multimode(data) → List` — all values tied for most common.
- `variance(data) → Float` — the sample variance.
- `stdev(data) → Float` — the sample standard deviation.
- `pvariance(data) → Float` — the population variance.
- `pstdev(data) → Float` — the population standard deviation.
- `quantiles(data[, n]) → List` — cut points dividing `data` into `n` equal groups.

---

## string

- Constants: `ascii_letters`, `ascii_lowercase`, `ascii_uppercase`, `digits`, `hexdigits`, `octdigits`, `punctuation`, `whitespace` (all `String`).
- `capwords(s) → String` — capitalize each whitespace-separated word.

Fuzzy comparison, built on the native `String.levenshtein` edit distance:

- `similarity(a, b) → Float | List` — a `0.0`–`1.0` ratio, `1 - editdistance / longerlength` (two empty strings are `1.0`). `b` may be a single String (returns one `Float`) **or a List of candidate Strings** (returns one score per candidate, computed in a single native call).
- `closest(query, candidates) → String` — the candidate with the smallest edit distance (ties to the earliest), or `None` for an empty list. One native call computes every distance at once.
- `fuzzymatch(query, candidates, cutoff = 0.6) → List` — every `[candidate, score]` pair whose similarity is at least `cutoff`, sorted by score descending (à la `difflib.get_close_matches`).

```kirito
var string = import("string")
string.closest("pyhton", ["python", "ruby", "rust"])   # "python"   (typo correction)
string.similarity("kitten", "sitting")                  # ~0.571
string.similarity("abc", ["abc", "abd", "xyz"])         # [1.0, 0.667, 0.0]  (List form)
```

---

## textwrap

- `wrap(text[, width]) → List` — wrap into a list of lines.
- `fill(text[, width]) → String` — wrap into a single newline-joined String.
- `indent(text, prefix) → String` — prefix each line.
- `dedent(text) → String` — remove the common leading whitespace.

---

## base64

Operates on **byte values** as a `List` of Integers (0–255), not text strings.

- `encode(data: List) → String` — Base64-encode a list of byte values.
- `decode(s: String) → List` — decode Base64 text back to a list of byte values.
- `urlsafeencode(data: List) → String` — encode using the URL-safe alphabet (`-_`).
- `urlsafedecode(s: String) → List` — decode using the URL-safe alphabet (`-_`).

---

## csv

Low-level CSV parsing/formatting (RFC-4180-style quoting). For tabular data analysis, see
[`tabular`](#tabular), which builds on this.

- `parse(text)` — parse CSV text into a List of rows (each a List of fields).
- `parserow(line)` — parse one CSV line into a List of fields.
- `format(rows) → String` — serialize a List of rows to CSV text.
- `formatrow(fields) → String` — serialize one row.

---

## tabular

A dataframe-style (pandas-like) data-analysis library: a labelled 1-D **`Series`** and 2-D **`DataFrame`**, with CSV
I/O, label/position indexing, boolean masking, element-wise arithmetic, aggregations, group-by,
joins, and missing-data handling. Public names follow Kirito's lowercase-no-underscore convention
(`readcsv`, `sortvalues`, `valuecounts`, `resetindex`, ...).

> **Column order from a dict.** Kirito dicts are not insertion-ordered, so
> `DataFrame({"a": ..., "b": ...})` does not guarantee column order. Pass `columns=[...]` (or use
> `readcsv`, whose header row is an ordered List) when order matters.

### Module functions

- `Series(values, index = None, name = None)` — a 1-D labelled column.
- `DataFrame(data = None, columns = None, index = None)` — `data` is a Dict of `column → values`, a
  List of row-Lists (pair with `columns`), or a List of row-Dicts (columns are the key union).
- `readcsv(source, header = True, infer = True)` — build a DataFrame from CSV text (or a filename).
  With `infer`, each cell becomes Integer/Float/Bool/None/String; an empty cell is `None` (missing).
- `merge(left, right, on, how = "inner")` — join two DataFrames on a key column; `how` is
  `"inner"`/`"left"`/`"right"`/`"outer"`.
- `concat(frames)` — stack DataFrames vertically (column union, missing filled with `None`).

### Series

Indexing: `s[label]` (by index label, falling back to position), `s.iat(pos)`. Element-wise `+ - *
/ // %` and comparisons `> >= < <=` against a scalar or another Series (the comparisons return a
**boolean Series** for masking); `s.eq(x)`/`s.ne(x)`/`s.isin(values)`.

- Aggregations (skip missing; Bool counts as 0/1): `sum`, `mean`, `min`, `max`, `median`, `variance`,
  `std`, `prod`, `count`.
- `unique()`, `nunique()`, `valuecounts()` (a Series of counts, descending).
- `apply(fn)`/`map(fn)`, `astype("Integer"|"Float"|"Bool"|"String")`.
- `fillna(value)`, `dropna()`, `head(n=5)`, `tail(n=5)`, `sortvalues(ascending=True)`, `resetindex()`,
  `tolist()`, `copy()`.

### DataFrame

- Selection: `df["col"]` → Series; `df[["a", "b"]]` → DataFrame; `df[boolean_series]` → masked rows;
  `df.iloc[i]`/`df.iloc[[i, j]]` (by position), `df.loc[label]`/`df.loc[[l1, l2]]` (by label);
  `df.column(name)`, `df.at(label, col)`, `df.iat(pos, col)`.
- `df["new"] = series_or_list_or_scalar` adds/replaces a column; `assign(name, value)` returns a copy
  with the column added.
- Shape/views: `shape()` → `[rows, cols]`, `columns`, `index`, `len(df)`, `head`/`tail`/`slice`,
  `rename(columns)`, `drop(columns)`, `setindex(col)`, `resetindex()`, `copy()`, `todict()`,
  `torows()`, `iterrows()`, `tocsv()`.
- Aggregations over **numeric** columns → a Series indexed by column: `sum`, `mean`, `min`, `max`,
  `std`, `count`; `describe()` → a DataFrame of count/mean/std/min/median/max.
- `sortvalues(by, ascending = True)`, `groupby(col)`, `merge(other, on, how)`, `dropna()`,
  `fillna(value)`.

### GroupBy

`df.groupby(col)` returns a grouping with numeric-column reductions `sum`/`mean`/`min`/`max`/`std`/
`count`, `size()` (a Series of group sizes), `agg({col: reducer})` where `reducer` is one of
`"sum"`/`"mean"`/`"min"`/`"max"`/`"std"`/`"count"`/`"median"`, and `apply(fn)` (fn receives each
group's sub-DataFrame).

```kirito
var io = import("io")
var tb = import("tabular")

var df = tb.readcsv("name,dept,salary\nAda,eng,120\nAlan,eng,110\nGrace,ops,95\nEdsger,ops,130")
io.print(df[df["salary"] > 100]["name"].tolist())     # [Ada, Alan, Edsger]
io.print(df.groupby("dept").mean()["salary"].tolist()) # [115.0, 112.5]
io.print(df.sortvalues("salary", ascending=False)["name"].tolist())
```

---

## xml

A small, dependency-free XML parser/serializer in the spirit of Python's `ElementTree`. It parses
elements, attributes, text, nested children, comments, the `<?xml?>` declaration, `<!DOCTYPE>`,
`<![CDATA[…]]>` sections, and the standard entities (`&lt; &gt; &amp; &quot; &apos;` and numeric
`&#65;` / `&#x41;`); it serializes a tree back to XML. The parser is **lenient** — malformed markup
is tolerated rather than raising.

### Module functions

- `parse(text: String) → Element` — parse a document and return its root `Element` (or `None` if the
  text contains no element). `fromstring` is an alias.
- `tostring(element) → String` — serialize an element (and its subtree) back to XML.
- `Element(tag, attrib = None) → Element` — construct an element directly (for building a tree).

### Element

An element exposes `.tag` (String), `.attrib` (a Dict of attribute → value), `.text` (character data
before the first child), `.tail` (character data after this element's end tag, ElementTree-style),
and `.children` (a List of child `Element`s). It is iterable (yields its children), supports `len`
and indexing (`elem[0]`).

- `get(key, default = None)` — an attribute value, or `default` if absent.
- `find(tag)` — the first child with that tag, or `None`.
- `findall(tag)` — a List of all children with that tag.
- `findtext(tag, default = "")` — the text of the first matching child, or `default`.
- `itertext()` — a List of all text in document order (this element's text, then each descendant's
  text and tail, walking the whole subtree).
- `tostring()` — serialize this element (also its `_str_`).

```kirito
var io = import("io")
var xml = import("xml")

var root = xml.parse("<books><book id='1'><title>The Hobbit</title></book>" +
                     "<book id='2'><title>SICP</title></book></books>")
for book in root.findall("book"):
    io.print(book.get("id") + ": " + book.findtext("title"))
# 1: The Hobbit
# 2: SICP
io.print(xml.tostring(root.find("book")))   # <book id="1"><title>The Hobbit</title></book>
```

---

## heapq

A min-heap maintained inside an ordinary List.

- `heapify(items) → List` — return a **new** heap-ordered List from `items` (does not modify `items`).
- `heappush(heap, item) → None` — push onto `heap` in place, keeping the heap invariant.
- `heappop(heap)` — pop and return the smallest element.
- `heapreplace(heap, item)` — pop the smallest, then push `item` (one pass).
- `merge(lists) → List` — merge already-sorted inputs (a List of Lists) into one sorted List.
- `nlargest(n, items) → List` — the `n` largest elements.
- `nsmallest(n, items) → List` — the `n` smallest elements.

---

## bisect

Binary search / ordered insertion into a sorted List.

- `bisectleft(a, x) → Integer` — the leftmost insertion index that keeps `a` sorted.
- `bisectright(a, x) → Integer` — the rightmost insertion index that keeps `a` sorted.
- `insortleft(a, x) → None` — insert `x` into the sorted List `a` at the leftmost valid position.
- `insortright(a, x) → None` — insert `x` into the sorted List `a` at the rightmost valid position.

---

## copy

- `copy(obj)` — a shallow copy.
- `deepcopy(obj)` — a deep copy (handles shared references and cycles).

---

## enum

- `Enum(names: List) → Enum` — build an enumeration mapping each name to its index.

### Enum object

- `e.get(name) → Integer` — the value (index) of a member; raises on an unknown name.
- `e[name] → Integer` — index syntax for the same lookup as `e.get(name)`.
- `e.nameof(value) → String` — the name for a value.
- `e.names() → List` — all member names.
- `e.values() → List` — all member values.
- `name in e` — membership test.

---

## tee

Fan-out streams: clone what you write to a stream into one or more extra streams (for example, mirror
stdout into a log file). A `Tee` implements the `write`/`writelines`/`flush` stream protocol, so it
can be assigned to `io.stdout`/`io.stderr`, and it is a context manager (it flushes on exit).

- `Tee(primary, copies = None) → Tee` — a stream that writes each chunk to every *copy* first, then to
  `primary`. `copies` is a single stream or a List; `primary` may be `None` for a pure fan-out sink.
- `t.write(data) → Integer` — write `data` to every stream (copies first, then primary).
- `t.writelines(lines) → None` — write each String in an iterable to every stream.
- `t.flush() → None` — flush every underlying stream.
- `t.close() → None` — close the Tee (flushes; does not close the copy streams you supplied).
- `t.streams() → List` — the underlying streams in write order (copies, then primary).
- `tee_stdout(copies)` — a context manager that makes `io.stdout` also write to `copies` inside the
  block, restoring the original on exit (the copy streams are never closed — you own them).
- `tee_stderr(copies)` — the same for `io.stderr`.

```kirito
var io = import("io")
var tee = import("tee")
with io.open("session.log", "w") as f:
    with tee.tee_stdout(f):
        io.print("appears on the console and in session.log")
# stdout is restored here
```

---

## arg

A small `argparse`-style command-line parser. Build a `Parser`, declare the arguments on it, then
run `parse` against a list of strings **yourself** — typically the main file's `arglist` (recall
`arglist` is empty in imported modules, so argument handling belongs in the program you run).

- `Parser(description = "") → Parser` — create a parser.

### Parser object

The configuration lives on the instance; the declaration methods each return the parser, so they
chain:

- `p.positional(name, help = "") → Parser` — a required positional argument (consumed in order).
- `p.option(name, default = None, help = "") → Parser` — a `--name VALUE` option. The value is
  converted to the **type of `default`** — an Integer default parses the value as an Integer (a bad
  value raises a clear error), a Float default as a Float, otherwise it stays a String.
- `p.flag(name, help = "") → Parser` — a boolean `--name` flag (default `False`, `True` when present).
- `p.usage() → String` — the generated usage/help text.
- `p.parse(args) → Dict` — parse `args` into a Dict keyed by argument name. Returns **`None`** if
  `-h`/`--help` is present (after printing `usage()`), so the program can stop. Accepts `--name value`,
  `--name=value`, and short `-n value` / `-f` (matched by the name's first letter); extra positionals
  are collected under the `"rest"` key. Raises a clear, catchable error on an unknown option, a
  missing required positional, or a value that can't be converted.

```kirito
var io = import("io")
var arg = import("arg")

var p = arg.Parser("greet someone")
p.positional("name")
p.option("count", 1)        # --count N  (parsed as an Integer, because the default is one)
p.flag("loud")              # --loud

var opts = p.parse(arglist)   # you choose when/what to parse
if opts != None:              # None means --help was shown
    var greeting = f"Hello, {opts['name']}!"
    if opts["loud"]:
        greeting = greeting.upper()
    var i = 0
    while i < opts["count"]:
        io.print(greeting)
        i = i + 1
```

Run as `ki greet.ki Ada --count 2 --loud` → prints `HELLO, ADA!` twice.
