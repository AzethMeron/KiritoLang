# Built-in Types

Kirito is **dynamically typed** (a name can refer to a value of any type) but **strongly typed**
(values are never silently coerced — `1 + "x"` is an error, not `"1x"`). `type(x)` returns a value's
type name as a String; `isinstance(x, T)` tests membership (inheritance-aware).

These are the built-in types. Collections (`List`, `Set`, `Dict`) hold values by reference, so two
names can share one collection and see each other's mutations.

| Type | Literal | Mutable | Hashable | Notes |
|------|---------|---------|----------|-------|
| `None` | `None` | — | yes | the absence of a value; falsy |
| `Bool` | `True` / `False` | — | yes | a subtype of nothing; falsy when `False` |
| `Integer` | `42`, `-7`, `0xFF`, `0b101` | — | yes | 64-bit, two's-complement wraparound |
| `Float` | `3.14`, `1e10`, `2e-3` | — | yes | IEEE-754 double |
| `String` | `"hi"`, `f"{x}"` | no | yes | Unicode, indexed by code point |
| `List` | `[1, 2, 3]` | yes | no | ordered sequence |
| `Set` | `{1, 2, 3}` | yes | no | unordered unique elements |
| `Dict` | `{"a": 1}` | yes | no | insertion-ordered key→value map |

Each type's constructor (e.g. `Integer(x)`, `List(iter)`) is a built-in function — see
[Built-in Functions](builtins.html#types-and-conversion).

## None

The single value `None`, returned by functions that don't `return` anything. It is falsy, equal only
to itself, and stringifies as `"None"`.

## Bool

`True` and `False`. Produced by comparisons and the logical operators, and accepted anywhere a truth
value is needed. `Bool(x)` gives the truthiness of any value: `None`, `0`, `0.0`, `""`, and empty
collections are falsy; everything else is truthy. Unlike Python, `Bool` is a **distinct type, not an
Integer**: it is not numeric (`True + 1` is a type error) and `True != 1`, in keeping with Kirito's
strong typing. Convert explicitly with `Integer(flag)` (`Integer(True) == 1`) when you need to count
or sum truth values.

## Integer

Signed 64-bit integers. Arithmetic wraps on overflow (well-defined two's complement, never undefined
behavior). Literals may be decimal, hex (`0xFF`), or binary (`0b1010`).

- `/` always produces a `Float` (`7 / 2 == 3.5`); use `//` for floor division.
- `//`, `%`, and `divmod` follow Python floor semantics: `divmod(-7, 3) == [-3, 2]`.
- `**` is exponentiation (`2 ** 10 == 1024`).
- Bitwise operators and `bin`/`oct`/`hex` work on Integers.

```kirito
var n = 255
io.print(hex(n))        # "0xff"
io.print(n // 10, n % 10)   # 25 5
```

## Float

IEEE-754 doubles, including scientific-notation literals (`1.5e3`, `2e-3`). Mixing an Integer and a
Float promotes to Float. `round(x[, ndigits])`, `abs(x)`, and the `math` module operate on Floats.
Float `nan`/`inf` arise from `math` (`math.inf`, `math.nan`).

## String

Immutable Unicode text, indexed and sliced by **code point** (not byte). `+` concatenates, `*`
repeats, `len` counts code points, `in` tests substrings, and iteration yields characters. Strings
are hashable (usable as Dict keys / Set elements). f-strings (`f"{expr}"`) interpolate, and
`.format()` / the `format` builtin apply Python mini-format-specs.

```kirito
var s = "café"
io.print(len(s), s[3], s[::-1])     # 4 é éfac
io.print("ab" * 3)                  # "ababab"
io.print(", ".join(["a", "b", "c"])) # "a, b, c"
```

### String methods

| Method | Description |
|--------|-------------|
| `s.upper()` / `s.lower()` | Case conversion (Unicode-aware for ASCII/Latin-1/Latin-Extended-A). |
| `s.strip([chars])` / `s.lstrip` / `s.rstrip` | Trim whitespace (or any of `chars`) from both/left/right. |
| `s.split([sep][, maxsplit])` | Split into a List; whitespace runs by default. |
| `s.join(iter)` | Join an iterable of Strings with `s` as the separator. |
| `s.replace(old, new[, count])` | Replace occurrences of `old` with `new`. |
| `s.startswith(p)` / `s.endswith(p)` | Prefix / suffix test. |
| `s.find(sub)` / `s.rfind(sub)` | First / last index of `sub`, or `-1`. |
| `s.index(sub)` / `s.rindex(sub)` | Like find/rfind but raise if absent. |
| `s.count(sub)` | Number of non-overlapping occurrences. |
| `s.format(...)` | Substitute `{}` / `{name}` fields (Python str.format). |
| `s.isdigit()` / `s.isalpha()` / `s.isalnum()` | Character-class predicates over the whole string. |
| `s.isspace()` / `s.islower()` / `s.isupper()` | Whitespace / case predicates. |
| `s.removeprefix(p)` / `s.removesuffix(p)` | Drop a prefix/suffix if present. |
| `s.ljust(w[, fill])` / `s.rjust(w[, fill])` / `s.center(w[, fill])` | Pad to width `w`. |
| `s.zfill(w)` | Left-pad with zeros to width `w` (sign-aware). |
| `s.partition(sep)` / `s.rpartition(sep)` | Split once into `[head, sep, tail]`. |

## List

An ordered, mutable sequence. Supports indexing (`xs[0]`, negatives count from the end), slicing
(`xs[1:3]`, `xs[::-1]`, `xs[::2]`), `+` concatenation, `*` repetition, `in`, iteration, and
lexicographic comparison (`<`/`<=`/`>`/`>=`, element-by-element then by length, like Python — which
makes multi-key sorts easy via a list-returning `key`).

```kirito
var xs = [3, 1, 2]
xs.append(4)
xs.sort()                  # [1, 2, 3, 4], in place, stable
io.print(xs[0], xs[-1], xs[1:3])   # 1 4 [2, 3]
```

### List methods

| Method | Description |
|--------|-------------|
| `xs.append(x)` | Add `x` to the end. |
| `xs.pop([i])` | Remove and return the last element (or index `i`). |
| `xs.insert(i, x)` | Insert `x` before index `i`. |
| `xs.remove(x)` | Remove the first element equal to `x`. |
| `xs.index(x)` | Index of the first element equal to `x` (raises if absent). |
| `xs.count(x)` | Number of elements equal to `x`. |
| `xs.reverse()` | Reverse in place. |
| `xs.sort([key][, reverse])` | Sort in place, **stable**; `key` precomputed once per element. |
| `xs.extend(iter)` | Append all elements of an iterable. |
| `xs.copy()` | A shallow copy. |
| `xs.clear()` | Remove all elements. |

## Set

An unordered collection of unique, hashable values. Supports `in`, `len`, iteration, and the usual
set algebra (also via operators where natural).

```kirito
var a = {1, 2, 3}
var b = {3, 4}
io.print(a.union(b), a.intersection(b))   # {1, 2, 3, 4} {3}
```

### Set methods

| Method | Description |
|--------|-------------|
| `s.add(x)` | Insert `x`. |
| `s.discard(x)` | Remove `x` if present (no error if absent). |
| `s.pop()` | Remove and return an arbitrary element. |
| `s.contains(x)` | Membership test (also `x in s`). |
| `s.union(other)` | Elements in either set. |
| `s.intersection(other)` | Elements in both. |
| `s.difference(other)` | Elements in `s` but not `other`. |
| `s.symmetric_difference(other)` | Elements in exactly one set. |
| `s.issubset(other)` / `s.issuperset(other)` | Containment tests. |
| `s.isdisjoint(other)` | True if the sets share no element. |
| `s.copy()` / `s.clear()` | Shallow copy / remove all. |

## Dict

An insertion-ordered map from hashable keys to values. Supports `d[key]` get/set, `in` (over keys),
`len`, and iteration (over keys). Multi-key indexing assignment (`m[i, j] = v`) is available to
types that define it.

```kirito
var d = {"a": 1, "b": 2}
d["c"] = 3
for k, v in d.items():
    io.print(k, v)
io.print(d.get("z", 0))    # 0 (default)
```

### Dict methods

| Method | Description |
|--------|-------------|
| `d.keys()` | List of keys (in insertion order). |
| `d.values()` | List of values. |
| `d.items()` | List of `[key, value]` pairs. |
| `d.get(key[, default])` | Value for `key`, or `default` (or `None`) if missing. |
| `d.pop(key[, default])` | Remove and return `key`'s value. |
| `d.popitem()` | Remove and return the last `[key, value]` pair. |
| `d.setdefault(key[, default])` | Get `key`, inserting `default` first if absent. |
| `d.update(other)` | Merge another Dict (or `[key, value]` pairs) in. |
| `d.copy()` / `d.clear()` | Shallow copy / remove all. |

## User-defined classes

A `class` defines a new type in the same value model as the built-ins — see the
[Language Guide](language-guide.html#classes). Instances support the same operator and protocol
methods (`_add_`, `_getitem_`, `_iter_`, …), so a class can behave exactly like a built-in
collection. `inspect(x)` prints a class/instance/module's public API.
