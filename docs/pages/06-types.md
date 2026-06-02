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
| `Dict` | `{"a": 1}` | yes | no | unordered key→value map (hashable keys) |

Each type's constructor (e.g. `Integer(x)`, `List(iter)`) is a built-in function — see
[Built-in Functions](builtins.html#types-and-conversion).

## None

The single value `None`, returned by functions that don't `return` anything. It is falsy, equal only
to itself, and stringifies as `"None"`.

## Bool

`True` and `False`. Produced by comparisons and the logical operators, and accepted anywhere a truth
value is needed. `Bool(x)` gives the truthiness of any value: `None`, `0`, `0.0`, `""`, and empty
collections are falsy; everything else is truthy. `Bool` is a **distinct type, not an Integer**: it
is not numeric (`True + 1` is a type error) and `True != 1`, in keeping with Kirito's strong typing. Convert explicitly with `Integer(flag)` (`Integer(True) == 1`) when you need to count
or sum truth values.

## Integer

Signed 64-bit integers. Arithmetic wraps on overflow with well-defined two's-complement semantics —
never undefined behavior. Literals may be decimal (`42`, `-7`), hexadecimal (`0xFF`), octal (`0o17`),
or binary (`0b1010`); the base prefix is case-insensitive.

The arithmetic operators are `+`, `-`, `*`, the three division forms below, and `**`
(exponentiation, right-associative: `2 ** 3 ** 2 == 512`).

- `/` always produces a `Float` — even when the operands divide evenly (`7 / 2 == 3.5`,
  `4 / 2 == 2.0`); use `//` when you want an Integer result.
- `//` (floor division), `%` (modulo), and `divmod(a, b)` floor toward negative infinity, so the
  remainder takes the sign of the divisor: `divmod(-7, 3) == [-3, 2]`.
- `**` raises to a power (`2 ** 10 == 1024`).
- `bin(n)` / `oct(n)` / `hex(n)` render an Integer in base 2 / 8 / 16 as a `String`
  (`hex(255) == "0xff"`).
- Kirito has no bitwise *operators*; the builtins `bitand` / `bitor` / `bitxor` / `bitnot` and
  `shl` / `shr` provide bitwise and/or/xor/not and left/right shifts on Integers
  (`bitand(0xFF, 0x0F) == 15`, `shl(1, 8) == 256`).

```kirito
var n = 255
io.print(hex(n), bin(5))      # "0xff" "0b101"
io.print(n // 10, n % 10)     # 25 5
io.print(2 ** 10, 7 / 2)      # 1024 3.5
```

## Float

IEEE-754 doubles, including scientific-notation literals (`1.5e3`, `2e-3`). Mixing an Integer and a
Float promotes to Float. `round(x[, ndigits])`, `abs(x)`, and the `math` module operate on Floats.
Float `nan`/`inf` arise from `math` (`math.inf`, `math.nan`).

## String

Immutable Unicode text, indexed and sliced by **code point** (not byte). `+` concatenates, `*`
repeats, `len` counts code points, `in` tests substrings, and iteration yields characters. Strings
are hashable (usable as Dict keys / Set elements). f-strings (`f"{expr}"`) interpolate, and
`.format()` / the `format` builtin apply the mini-format-spec (fill/align/sign/width/precision/type).

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
| `s.format(...)` | Substitute `{}` (sequential) and `{0}`/`{1}` (indexed) fields with the positional arguments. (Named `{x}` fields are an f-string feature.) |
| `s.isdigit()` / `s.isalpha()` / `s.isalnum()` | Character-class predicates over the whole string. |
| `s.isspace()` / `s.islower()` / `s.isupper()` | Whitespace / case predicates. |
| `s.removeprefix(p)` / `s.removesuffix(p)` | Drop a prefix/suffix if present. |
| `s.ljust(w[, fill])` / `s.rjust(w[, fill])` / `s.center(w[, fill])` | Pad to width `w`. |
| `s.zfill(w)` | Left-pad with zeros to width `w` (sign-aware). |
| `s.partition(sep)` / `s.rpartition(sep)` | Split once into `[head, sep, tail]`. |

## List

An ordered, mutable sequence. Supports indexing (`xs[0]`, negatives count from the end), slicing
(`xs[1:3]`, `xs[::-1]`, `xs[::2]`), `+` concatenation, `*` repetition, `in`, iteration, and
lexicographic comparison (`<`/`<=`/`>`/`>=`, compared element-by-element, then by length when one is
a prefix of the other — which makes multi-key sorts easy via a list-returning `key`).

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
| `s.symmetricdifference(other)` | Elements in exactly one set. |
| `s.issubset(other)` / `s.issuperset(other)` | Containment tests. |
| `s.isdisjoint(other)` | True if the sets share no element. |
| `s.copy()` / `s.clear()` | Shallow copy / remove all. |

## Dict

An **unordered** map from hashable keys to values (iteration order is unspecified — do not rely on
it). Supports `d[key]` get/set, `in` (over keys), `len`, and iteration (over keys). Multi-key
indexing assignment (`m[i, j] = v`) is available to types that define it.

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
| `d.keys()` | List of keys (in unspecified order). |
| `d.values()` | List of values. |
| `d.items()` | List of `[key, value]` pairs. |
| `d.get(key[, default])` | Value for `key`, or `default` (or `None`) if missing. |
| `d.pop(key[, default])` | Remove and return `key`'s value. |
| `d.remove(key)` | Delete `key` (raises if absent; like `pop` but returns nothing). |
| `d.popitem()` | Remove and return the last `[key, value]` pair. |
| `d.setdefault(key[, default])` | Get `key`, inserting `default` first if absent. |
| `d.update(other)` | Merge another Dict (or `[key, value]` pairs) in. |
| `d.copy()` / `d.clear()` | Shallow copy / remove all. |

## User-defined classes

A `class` defines a new type in the same value model as the built-ins — see the
[Language Guide](language-guide.html#classes) for syntax, inheritance, and `self._super_()`.
A class plugs into the evaluator's operator/protocol dispatch by defining **special methods** (the
single-underscore dunders below), so a user type can behave exactly like a built-in. `inspect(x)`
prints a class/instance/module's public API.

## Special methods (operator overloading)

Define any of these as methods on a class to control how instances respond to operators, built-ins,
and language constructs. Each takes `self` as its first parameter. A method that isn't defined means
the corresponding operator raises a clear "no operator" / "not supported" error (except `_ne_`, which
falls back to `not _eq_`). Names use **single** leading/trailing underscores (`_add_`, not `__add__`).

### Construction and display

| Method | Invoked by | Returns |
|--------|-----------|---------|
| `_init_(self, ...)` | `ClassName(...)` (instance creation) | nothing (initializes `self`) |
| `_str_(self)` | `String(x)`, `io.print(x)`, f-strings, nested in a collection's text | a `String` |

### Arithmetic operators

Each is invoked as `x OP y` → `x._op_(y)` (left operand's method); `self` is the left operand, the
argument is the right operand. Return the result value (any type).

| Method | Operator |
|--------|----------|
| `_add_(self, other)` | `x + y` |
| `_sub_(self, other)` | `x - y` |
| `_mul_(self, other)` | `x * y` |
| `_div_(self, other)` | `x / y` |
| `_floordiv_(self, other)` | `x // y` |
| `_mod_(self, other)` | `x % y` |
| `_pow_(self, other)` | `x ** y` |

### Comparison operators

Invoked as `x OP y` → `x._op_(y)`; return a `Bool` (or any truthy/falsy value).

| Method | Operator | Notes |
|--------|----------|-------|
| `_eq_(self, other)` | `x == y` | also drives `in`, `index`, `count`, `remove` on Lists |
| `_ne_(self, other)` | `x != y` | optional — defaults to `not self._eq_(other)` |
| `_lt_(self, other)` | `x < y` | also drives `sorted()`, `min()`, `max()` |
| `_le_(self, other)` | `x <= y` | |
| `_gt_(self, other)` | `x > y` | |
| `_ge_(self, other)` | `x >= y` | |

### Unary operators

| Method | Invoked by | Returns |
|--------|-----------|---------|
| `_neg_(self)` | `-x` | the negated value |
| `_not_(self)` | `not x` | a truth value |

### Container / indexing protocol

| Method | Invoked by | Returns |
|--------|-----------|---------|
| `_getitem_(self, key)` | `x[key]` (variadic: `x[i, j]` passes `i, j`) | the element |
| `_setitem_(self, key, value)` | `x[key] = value` (variadic keys: `m[i, j] = v`) | nothing |
| `_len_(self)` | `len(x)` | an `Integer` |
| `_contains_(self, item)` | `item in x` / `item not in x` | a truth value |
| `_iter_(self)` | `for v in x:`, and any iteration (unpacking, `List(x)`, …) | a List of the elements to yield |

### Callable and context-manager protocol

| Method | Invoked by | Returns |
|--------|-----------|---------|
| `_call_(self, ...)` | `x(...)` (calling an instance) | the call result |
| `_enter_(self)` | entering a `with x as v:` block | the value bound to `v` |
| `_exit_(self)` | leaving a `with` block (normally or via an exception) | ignored |

### Inheritance

| Method | Invoked by | Returns |
|--------|-----------|---------|
| `_super_(self)` | `self._super_()` inside a method | a parent view of `self` whose lookup starts at the base of the current method's class |

`_super_` is provided automatically; defining it is possible but discouraged (it breaks every
`self._super_()` call in the class). See the [Language Guide](language-guide.html#super-the-parent-view).

### Worked example

```kirito
class Vec:
    var _init_ = Function(self, x, y):
        self.x = x
        self.y = y
    var _add_ = Function(self, o):
        return Vec(self.x + o.x, self.y + o.y)
    var _mul_ = Function(self, k):           # scalar multiply
        return Vec(self.x * k, self.y * k)
    var _eq_ = Function(self, o):
        return self.x == o.x and self.y == o.y
    var _lt_ = Function(self, o):            # order by magnitude², enables sorting
        return self.x ** 2 + self.y ** 2 < o.x ** 2 + o.y ** 2
    var _getitem_ = Function(self, i):
        if i == 0:
            return self.x
        return self.y
    var _len_ = Function(self):
        return 2
    var _str_ = Function(self):
        return f"Vec({self.x}, {self.y})"

var a = Vec(1, 2)
var b = Vec(3, 4)
io.print(a + b)                  # Vec(4, 6)   (via _add_ + _str_)
io.print(a * 3)                  # Vec(3, 6)
io.print(a == Vec(1, 2))         # True
io.print(b[0], len(b))           # 3 2
io.print(sorted([b, a])[0])      # Vec(1, 2)   (smaller magnitude first, via _lt_)
```

