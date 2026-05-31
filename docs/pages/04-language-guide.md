# Language Guide

A reference for Kirito's syntax and semantics.

## Comments and layout

`#` starts a line comment. Blocks use **Python-style significant indentation**: a `:` then a newline
then an indented suite. Tabs and spaces both work, but mixing them ambiguously is an error.

```kirito
if x > 0:
    io.print("positive")    # indented block
```

## Variables and assignment

`var` declares a name in the current scope; bare `=` rebinds the nearest existing binding (a
`NameError` if it doesn't exist). Assignment binds a **name to a value** — it does not copy.

```kirito
var a = 1
a = a + 1          # rebinds a
var b = a          # b and a are independent bindings to the same value
```

Reference semantics: `A = B` makes `A` refer to the same value as `B`; mutating a shared mutable
value (List/Dict/Set/instance) is visible through every name bound to it.

## Types

Dynamically typed, strongly typed. Built-in types:

| Type | Examples |
|------|----------|
| `None` | `None` |
| `Bool` | `True`, `False` |
| `Integer` | `42`, `-7` (64-bit, two's-complement wraparound on overflow) |
| `Float` | `3.14`, `1.0` |
| `String` | `"hi"`, Unicode, code-point indexed |
| `List` | `[1, 2, 3]` (ordered, mutable) |
| `Set` | `{1, 2, 3}` (unique, unordered) |
| `Dict` | `{"a": 1}` (key→value) |

`type(x)` returns the type name as a String. Constructors double as converters: `Integer("42")`,
`Float(3)`, `String(x)`, `Bool(x)`, and `List(iter)`, `Set(iter)`, `Dict(pairs)`.

## Numbers and arithmetic

Python-3 division semantics: `/` always yields a `Float`; `//` is floor division; `%` modulo; `**`
right-associative exponentiation.

```kirito
7 / 2       # 3.5  (Float)
7 // 2      # 3
-7 // 2     # -4   (floors toward negative infinity)
2 ** 10     # 1024
```

Integer overflow wraps (well-defined), it does not trap; arbitrary precision is a future enrichment.

## Strings

Unicode strings, indexed and sliced by **code point** (not byte). Concatenate with `+`, repeat with
`*`. f-strings interpolate expressions:

```kirito
var s = "chrząszcz"
len(s)              # 9 code points
s[0]                # "c"
s[::-1]             # reversed
"ab" * 3            # "ababab"
var n = 5
f"n squared is {n * n}"
```

Methods: `upper`, `lower` (Unicode-aware), `strip`/`lstrip`/`rstrip`, `split`, `join`, `replace`,
`startswith`, `endswith`, `find`, `count`, `format`.

## Collections

```kirito
var xs = [3, 1, 2]
xs.append(4)
xs.sort()                  # in-place, stable
xs[0]                      # 1
xs[1:3]                    # [2, 3]  (slicing)
len(xs)

var d = {"a": 1, "b": 2}
d["c"] = 3
d.get("z", 0)              # default if missing
for k, v in d.items():
    io.print(f"{k}={v}")

var s = {1, 2, 3}
s.add(4)
2 in s                     # True
```

Slicing supports negative indices and steps: `xs[3:-1]`, `xs[::-1]`, `xs[::2]`.

## Control flow

```kirito
if cond:
    ...
elif other:
    ...
else:
    ...

while cond:
    ...
    if done:
        break
    continue

for x in iterable:
    ...
```

Logical operators `and`/`or`/`not` short-circuit and yield an operand. `break`/`continue` outside a
loop and `return` outside a function are rejected at parse time.

## Functions

First-class values created with `Function`. Parameters take **keyword arguments**, **default
values**, and **enforced type annotations**; the return type can be annotated too.

```kirito
var power = Function(base, exp = 2) -> Integer:
    return base ** exp

power(5)               # 25  (default exp)
power(exp = 3, base = 2)   # 8  (keyword args, any order)

var typed = Function(d : Dict) -> Float:
    return Float(len(d))
# typed([1, 2])  -> error: argument 'd' must be Dict, got List
```

Annotations are checked at runtime (inheritance-aware for classes). `Any` or no annotation accepts
anything. Inline form: `Function(x): return x * x`.

## Packing and unpacking

A bare comma sequence packs into a List; the left side of `=`, `var`, and `for` unpacks any iterable,
with one optional starred target absorbing the surplus.

```kirito
var t = 1, 2, 3            # t == [1, 2, 3]
var a, b = 1, 2            # a == 1, b == 2
a, b = b, a               # swap
var first, *rest = [1, 2, 3, 4]   # first == 1, rest == [2, 3, 4]
for k, v in d.items():
    ...
```

## Classes

```kirito
class Animal:
    var _init_ = Function(self, name):
        self.name = name
        self._id = 0          # _name => private (only methods of this class can touch it)
    var speak = Function(self) -> String:
        return "..."

class Dog(Animal):            # inheritance
    var speak = Function(self) -> String:
        return f"{self.name} says woof"
```

Special methods use Python dunder names with **single** underscores: `_init_`, `_str_`,
`_add_`/`_sub_`/`_mul_`/..., `_eq_`/`_lt_`/..., `_neg_`, `_call_`, `_getitem_`/`_setitem_`, `_len_`,
`_contains_`, `_iter_`, `_enter_`/`_exit_`.

### `_super_()` — the parent view

`self._super_()` returns a **parent view of `self`**: the same instance, but method/attribute lookup
begins at the *base of the class whose method is currently running*. Use it to extend (rather than
replace) an inherited method, including the constructor:

```kirito
class Dog(Animal):
    var _init_ = Function(self, name, breed):
        self._super_()._init_(name)        # run Animal's constructor
        self.breed = breed
    var describe = Function(self) -> String:
        return self._super_().describe() + " (" + self.breed + ")"
```

Because resolution starts at the *current method's* class base, each `_super_()` climbs exactly one
level, so multi-level chains (`Puppy → Dog → Animal`) compose correctly.

`_super_()` is only meaningful when the class inherits — calling it from a class with no base raises
`_super_() called in 'X', which does not inherit from any class`.

It is technically a special method, so a class *can* define its own `_super_` and override the
default behaviour — **don't.** Overriding it discards the parent view and breaks every `_super_()`
call in that class's methods; there is no good reason to do it.

## Exceptions

C++-style keywords, Python-style blocks. `throw` raises; `try`/`catch [Type as e]`/`finally` handles.

```kirito
class ValueError:
    var _init_ = Function(self, msg):
        self.msg = msg

try:
    throw ValueError("bad")
catch ValueError as e:
    io.print(e.msg)
finally:
    io.print("always runs")
```

Typed `catch` matches via the class chain (a subclass matches its base). A bare `catch:` catches
everything.

## Context managers

```kirito
with io.open("file.txt", "r") as f:
    for line in f:
        io.print(line)
# f is closed automatically (enter/exit protocol)
```

## Modules

`import("name")` returns a module value; access members with `.`. Modules are resolved from the
bundled stdlib, then from `.ki` files on the import path. Each module loads once per VM.

```kirito
var math = import("math")
math.sqrt(2)
```

## The `discard` statement and warnings

A bare expression whose non-`None` value is dropped triggers a warning (it's probably a mistake).
Use `discard EXPR` to say "I'm intentionally ignoring this result":

```kirito
discard validate(x)    # called for its side effect / exception; result ignored on purpose
```

The interpreter also warns about local variables that are assigned but never used. Warnings go to
stderr and never stop execution; `-w` disables them.
