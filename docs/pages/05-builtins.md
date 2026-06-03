# Built-in Functions

These names are available everywhere without an `import`. Each entry lists a signature
(`name(args) → ReturnType`), what it takes, and what it does.

**Keyword arguments everywhere.** Every fixed-arity builtin declares a **signature**, so it accepts
its parameters by name and in any order, with defaults — e.g. `Integer(x = "42")`,
`round(3.14159, ndigits = 2)`, `sorted(xs, reverse = True)`, `pow(2, 10, mod = 1000)`. `inspect(fn)`
prints those parameter names, types, and return type live. Genuinely **variadic** builtins
(`min`, `max`, `zip`, `range`, `sum`) take a positional list and advertise themselves as `name(...)`
under `inspect`; `min`/`max` additionally accept the keyword options `key=` and `default=`.

A leading `*args` below denotes a variadic positional list; `[arg]` denotes an optional argument.

## Types and conversion

The type constructors double as converters (Python style) and are keyword-callable by their
parameter name.

- `type(x) → String` — the type name of `x` (e.g. `"Integer"`, `"List"`, a user class's name).
- `Integer(x) → Integer` — convert to a 64-bit integer. Accepts `Bool` (`True`→`1`), `Float`
  (truncates toward zero; rejects NaN/∞/out-of-range), or a `String` in decimal or `0x`/`0o`/`0b`
  form. Raises on anything else or an unparseable String.
- `Float(x) → Float` — convert to a double. Accepts `Integer`, `Bool`, or a numeric `String`
  (including scientific notation). Raises if a String doesn't parse.
- `String(x) → String` — the `str()` form of any value (the same text `io.print` would emit).
- `Bool(x) → Bool` — the truthiness of `x` (empty collections/`0`/`""`/`None` are `False`).
- `List([iterable]) → List` — an empty list, or a new list built from any iterable. `List(iterable = xs)`.
- `Set([iterable]) → Set` — an empty set, or a set of the distinct elements of an iterable.
- `Dict([iterable]) → Dict` — an empty dict, or a dict from an iterable of `[key, value]` pairs.
- `isinstance(value, type) → Bool` — whether `value` is an instance of `type` — a class value or a
  type-name `String` (`isinstance(x, "Integer")`). Inheritance-aware: a subclass instance satisfies a
  base type.

## Sequences and iteration

- `len(x) → Integer` — number of elements of a collection, or code points of a String.
- `range(stop) → List` / `range(start, stop[, step]) → List` — integers from `start` (default `0`)
  up to but excluding `stop`, stepping by `step` (default `1`, may be negative). Materializes a List;
  a step of `0` raises, and an over-large result raises rather than exhausting memory.
- `enumerate(iterable) → List` — a list of `[index, value]` pairs, indices starting at `0`.
- `zip(*iterables) → List` — a list of `[a, b, …]` tuples drawn position-wise from the inputs,
  truncated to the shortest. Variadic.
- `map(function, iterable) → List` — apply `function` to every element, collecting the results.
- `filter(function, iterable) → List` — keep the elements for which `function(x)` is truthy.
- `reversed(iterable) → List` — the elements in reverse order.
- `sorted(iterable[, key][, reverse]) → List` — a new **stable**-sorted list. `key` is an optional
  function mapping each element to its comparison key (computed once per element); `reverse = True`
  sorts descending. `sorted(xs, key = len, reverse = True)`.
- `sum(iterable) → Number` — the sum of a sequence of numbers; an `Integer` if every element is an
  Integer, otherwise a `Float`. `sum([])` is `0`.
- `min(*args[, key][, default]) → value` / `max(*args[, key][, default]) → value` — the smallest /
  largest of a single iterable (`min(xs)`) or of several positional values (`min(a, b, c)`). `key` is
  an optional function producing the comparison key; `default` is returned when the (single) iterable
  is empty — without it, an empty sequence raises. `max(words, key = len)`.
- `all(iterable) → Bool` — `True` if every element is truthy (vacuously `True` when empty).
- `any(iterable) → Bool` — `True` if at least one element is truthy.

## Numbers

- `abs(x) → Number` — absolute value of an Integer or Float (Integer `abs` wraps two's-complement at
  `INT64_MIN`, consistent with Kirito's fixed-width integers).
- `round(x[, ndigits]) → Number` — with `ndigits` omitted (or `None`), round to the nearest
  `Integer`; with `ndigits` given, round to that many decimal places, yielding a `Float`.
  `round(pi, ndigits = 2)`.
- `divmod(a, b) → List` — `[a // b, a % b]` in one step, using floor semantics.
- `pow(base, exp[, mod]) → Number` — exponentiation; the 3-argument form is modular,
  `(base ** exp) % mod`, computed efficiently. `pow(2, 10, mod = 1000)`.
- `bin(n) → String` — the base-2 text of an Integer with a `0b` prefix (sign-aware).
- `oct(n) → String` — the base-8 text of an Integer with a `0o` prefix (sign-aware).
- `hex(n) → String` — the base-16 text of an Integer with a `0x` prefix (sign-aware).
- `bitand(a, b) → Integer` — bitwise AND of two Integers (Kirito has no `&` operator).
- `bitor(a, b) → Integer` — bitwise OR of two Integers (Kirito has no `|` operator).
- `bitxor(a, b) → Integer` — bitwise XOR of two Integers (Kirito has no `^` operator).
- `bitnot(a) → Integer` — bitwise NOT (`~a`, i.e. `-a - 1`).
- `shl(a, n) → Integer` — shift `a` left by `n ≥ 0` bits.
- `shr(a, n) → Integer` — shift `a` right by `n ≥ 0` bits (arithmetic, sign-preserving).
- `ord(char) → Integer` — the Unicode code point of a single-character String.
- `chr(codepoint) → String` — the single-character String for a code point.
- `format(value[, spec]) → String` — format a value with a mini-format-spec (see below).

## Formatting with `format`

`format(value, spec)` applies a spec of the form `[[fill]align][sign][#][0][width][,][.precision][type]`:

```kirito
format(42, "05d")        # "00042"
format(255, "x")         # "ff"        (also X, b, o, d)
format(255, "#x")        # "0xff"      (# adds the base prefix)
format(3.14159, ".2f")   # "3.14"      (also e, g, %)
format(1234567, ",")     # "1,234,567" (thousands separator)
format(0.25, ".1%")      # "25.0%"
format("hi", "^6")       # "  hi  "    (< left, > right, ^ center)
format("x", "*^7")       # "***x***"   (custom fill)
format(-42, "+06d")      # "-00042"    (sign + zero-pad)
```

## Introspection and modules

- `import(name) → Module` — load and return the named module (cached per VM). `import(name = "io")`.
- `inspect(x) → String` — a human-readable description of the public methods/attributes (with
  signatures, type annotations, and defaults) of a class, instance, module, function, or **native
  object** (e.g. a `Random`, `Matrix`, `BytesIO`, `DateTime`, regex `Pattern`/`Match`, `Socket`,
  `Response`, …) — every native type declares its members for introspection. Native
  functions/modules that declare a signature are shown the same way.
- `id(x) → Integer` — a stable identity for a live object (its arena slot); interned scalars share
  one. Useful for detecting aliasing/shared references.

> `print` and `input` are **not** builtins — they live in the `io` module:
> `import("io").print(...)`. See the [standard library](07-stdlib.md#io) reference.

## Notes

- `divmod`/`//`/`%` use floor semantics — the quotient rounds toward negative infinity and the
  remainder takes the sign of the divisor: `divmod(-7, 3) == [-3, 2]`.
- `range` materializes a List (there are no lazy generators yet), so very large ranges allocate.
- `min`/`max` raise on an empty sequence unless `default` is given; `sum([])` is `0`.
- Passing a non-iterable where an iterable is expected raises a clean `is not iterable` error.
- An unknown keyword, a duplicated argument, a missing required argument, or too many positionals all
  raise a clear, catchable error naming the function and argument.
