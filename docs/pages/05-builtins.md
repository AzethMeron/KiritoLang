# Built-in Functions

These names are available everywhere without an `import`.

Most fixed-arity builtins (and stdlib module functions) declare a **signature**, so they accept
**keyword arguments** and **defaults** just like Kirito functions — e.g. `round(3.14159, ndigits=2)`,
`sorted(xs, reverse=True)`, `pow(2, 10, mod=1000)`, `io.open(path, mode="w")`. `inspect(fn)` shows a
function's parameters, types, and return type. Genuinely variadic builtins (`min`, `max`, `zip`,
`range`, `io.print`) take positional arguments only.

## Types and conversion

| Function | Description |
|----------|-------------|
| `type(x)` | Type name of `x` as a String (e.g. `"Integer"`). |
| `Integer(x)` | Convert Bool/Float/String to Integer. |
| `Float(x)` | Convert Integer/Bool/String to Float. |
| `String(x)` | The `str()` form of any value. |
| `Bool(x)` | Truthiness of `x`. |
| `List([iter])` | Empty list, or a list from an iterable. |
| `Set([iter])` | Empty set, or a set from an iterable. |
| `Dict([pairs])` | Empty dict, or a dict from `[key, value]` pairs. |
| `isinstance(x, T)` | True if `x` is an instance of type `T` (a class value or a type-name String); inheritance-aware. |

## Sequences and iteration

| Function | Description |
|----------|-------------|
| `len(x)` | Number of elements / code points. |
| `range(stop)` / `range(start, stop[, step])` | List of integers. |
| `enumerate(iter)` | List of `[index, value]` pairs. |
| `zip(a, b, ...)` | List of tuples, truncated to the shortest input. |
| `map(f, iter)` | Apply `f` to each element → List. |
| `filter(f, iter)` | Keep elements where `f(x)` is truthy → List. |
| `reversed(iter)` | Elements in reverse order → List. |
| `sorted(iter[, key][, reverse])` | New stable-sorted List. |
| `sum(iter)` | Sum of numbers (Integer if all Integer). |
| `min(iter)` / `max(iter)` | Smallest / largest (also variadic: `min(a, b, c)`). |
| `all(iter)` / `any(iter)` | Truthiness over a sequence. |

## Numbers

| Function | Description |
|----------|-------------|
| `abs(x)` | Absolute value. |
| `round(x[, ndigits])` | Round to `ndigits` decimals (default 0). |
| `divmod(a, b)` | `[a // b, a % b]`. |
| `pow(b, e)` / `pow(b, e, m)` | Exponentiation; 3-arg form is modular `(b**e) % m`. |
| `bin(n)` / `oct(n)` / `hex(n)` | Base-2/8/16 string with `0b`/`0o`/`0x` prefix (sign-aware). |
| `ord(ch)` | Unicode code point of a single-character String. |
| `chr(cp)` | Single-character String for a code point. |
| `format(value[, spec])` | Format a value with a Python mini-format-spec (see below). |

## Formatting with `format`

`format(value, spec)` applies a Python-style format spec
`[[fill]align][sign][#][0][width][,][.precision][type]`:

```kirito
format(42, "05d")        # "00042"
format(255, "x")         # "ff"        (also X, b, o, d)
format(3.14159, ".2f")   # "3.14"      (also e, g, %)
format(1234567, ",")     # "1,234,567" (thousands separator)
format(0.25, ".1%")      # "25.0%"
format("hi", "^6")       # "  hi  "    (< left, > right, ^ center)
format("x", "*^7")       # "***x***"   (custom fill)
format(-42, "+06d")      # "-00042"    (sign + zero-pad)
```

## I/O and modules

| Function | Description |
|----------|-------------|
| `import(name)` | Load and return a module. |
| `inspect(x)` | A String describing the public methods/attributes (with signatures and type annotations) of a class, instance, module, or function. |

> `print` and `input` are **not** builtins — they live in the `io` module: `import("io").print(...)`.

## Notes

- `divmod`/`//`/`%` use Python floor semantics: `divmod(-7, 3) == [-3, 2]`.
- `range` materializes a List (there are no lazy generators yet), so very large ranges allocate.
- `min`/`max` raise on an empty sequence; `sum([])` is `0`.
- Passing a non-iterable where an iterable is expected raises a clean `is not iterable` error.
