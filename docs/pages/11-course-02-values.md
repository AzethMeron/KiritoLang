# Lesson 2 — Values and Variables

Every program shuffles **values** around. Kirito has a small set of built-in value types, and a
clear, predictable model for the **names** that refer to them. Getting this model straight now saves
confusion later.

## The basic values

```kirito
var io = import("io")

var age = 30            # Integer  — whole numbers
var price = 9.99        # Float    — numbers with a fractional part
var name = "Ada"        # String   — text
var is_admin = True     # Bool     — True or False
var nothing = None      # None     — the "no value" value

io.print(type(age), type(price), type(name), type(is_admin), type(nothing))
# => Integer Float String Bool None
```

`type(x)` returns the name of a value's type as a String — useful when you're not sure what you're
holding.

## `var` declares, `=` updates

`var name = value` **declares** a new name in the current scope. Once declared, a plain `=` (no
`var`) **updates** the existing binding:

```kirito
var counter = 0         # declare
counter = counter + 1   # update — counter is now 1
counter = counter + 1   # update — counter is now 2
io.print(counter)       # => 2
```

Using `=` on a name that was never declared is an error (a `NameError`), not a silent new variable —
this catches typos early:

<!--norun (demonstrates an intentional error)-->
```kirito
total = 5               # error: name 'total' is not defined  (you meant `var total = 5`)
```

## Truthiness

Every value is either "truthy" or "falsy" when used as a condition. The falsy values are `False`,
`None`, zero (`0`, `0.0`), the empty String `""`, and any empty collection. Everything else is
truthy:

```kirito
var name = ""
if name:
    io.print("got a name")
else:
    io.print("name is empty")     # => name is empty (an empty String is falsy)
```

`Bool(x)` makes this explicit, returning the truthiness of any value:

```kirito
io.print(Bool(0), Bool(42), Bool(""), Bool("hi"), Bool([]))
# => False True False True False
```

## Names are bindings, not boxes

This is the one idea to internalize. Assignment **binds a name to a value**; it does not copy the
value. When two names are bound to the same mutable value, they see each other's changes:

```kirito
var first = [1, 2, 3]   # a List (covered in Lesson 7)
var second = first      # second is bound to the SAME list, not a copy
second.append(4)
io.print(first)         # => [1, 2, 3, 4]   — first sees the change too
```

For immutable values (Integer, Float, String, Bool, None) this never surprises you, because you can
never change the value in place — you only ever rebind the name:

```kirito
var x = 10
var y = x
x = 99                  # rebinds x; y still refers to the original 10
io.print(x, y)          # => 99 10
```

When you genuinely want an independent copy of a mutable value, ask for one explicitly (the `copy`
module and `.copy()` methods, covered later).

## Converting between types

The type names double as converters. Each takes a value and returns the converted form:

```kirito
io.print(Integer("42") + 1)     # => 43   (String -> Integer)
io.print(Float(7))              # => 7.0  (Integer -> Float)
io.print(String(3.5) + "!")     # => 3.5! (anything -> String)
io.print(Integer(3.9))          # => 3    (Float -> Integer, truncates toward zero)
```

A conversion that can't succeed raises a clear error rather than guessing:

<!--norun (intentional conversion error)-->
```kirito
Integer("not a number")         # error: cannot convert String to Integer
```

## Try it

Declare `temp_celsius = 100`, convert it to Fahrenheit (`c * 9 / 5 + 32`), and print a sentence like
`100C is 212.0F`. Notice which operations give you an Integer and which give a Float — the next
lesson explains exactly why.

## What you learned

- The built-in values: `Integer`, `Float`, `String`, `Bool`, `None`.
- `var` declares; `=` updates an existing name (and errors if it doesn't exist).
- Assignment binds names to values — mutable values are shared, not copied.
- Truthiness rules, and the type-name converters.
