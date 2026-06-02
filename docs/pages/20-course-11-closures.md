# Lesson 11 — Closures and First-Class Functions

In Kirito, functions are ordinary values. You can store them in variables, pass them to other
functions, return them, and keep a list of them. A function that *remembers* the variables around
where it was created is called a **closure** — and closures unlock some powerful, compact patterns.

## Functions are values

```kirito
var io = import("io")

var double = Function(x): return x * 2
var triple = Function(x): return x * 3

var operations = [double, triple]      # a List of functions
for op in operations:
    io.print(op(10))                    # => 20 / 30
```

## Passing functions as arguments

A function that takes another function is "higher-order". The builtins `map`, `filter`, and
`sorted(key=...)` are the everyday examples:

```kirito
var numbers = [1, 2, 3, 4, 5, 6]

# map: apply a function to every element.
io.print(map(Function(n): return n * n, numbers))    # => [1, 4, 9, 16, 25, 36]

# filter: keep the elements for which the function is truthy.
io.print(filter(Function(n): return n % 2 == 0, numbers))   # => [2, 4, 6]

# sorted with a key function.
io.print(sorted(["bb", "a", "ccc"], key=len))         # => [a, bb, ccc]
```

## Returning a function: the closure

Here's the key move — a function that builds and returns another function, which captures values
from the enclosing scope:

```kirito
var io = import("io")

# make_adder returns a NEW function that adds `amount` to whatever it's given.
var make_adder = Function(amount):
    return Function(x): return x + amount     # the inner function "closes over" amount

var add_ten = make_adder(10)
var add_hundred = make_adder(100)
io.print(add_ten(5))        # => 15
io.print(add_hundred(5))    # => 105
```

`add_ten` and `add_hundred` are two separate functions, each carrying its own captured `amount`. The
inner function kept a live reference to the variable from when it was created — that's the closure.

## Closures with private state

Because a closure can capture a *mutable* binding, you can build little objects with private state
without a class:

```kirito
var io = import("io")

var make_counter = Function():
    var count = 0                 # private to each counter
    return Function():
        count = count + 1         # rebinds the captured `count`
        return count

var tick = make_counter()
io.print(tick(), tick(), tick())  # => 1 2 3
var other = make_counter()
io.print(other())                 # => 1   (independent state)
```

`tick` and `other` each have their own `count` — calling one never affects the other.

## A worked example: composing transformations

```kirito
var io = import("io")

# Build a pipeline that applies a list of single-argument functions in order.
var pipeline = Function(steps):
    return Function(value):
        var current = value
        for step in steps:
            current = step(current)
        return current

var clean = pipeline([
    Function(s): return s.strip(),
    Function(s): return s.lower(),
    Function(s): return s.replace(" ", "_"),
])

io.print(clean("  Hello World  "))     # => hello_world
```

**Walkthrough:** `pipeline` takes a list of step-functions and returns a new function that threads a
value through each step in turn. `clean` is that returned closure, carrying its captured `steps`
list. This is composition as data — you can build, store, and reuse transformations as values, which
is the heart of functional programming.

## Try it

Write `make_multiplier(factor)` that returns a function multiplying its argument by `factor`. Use it
to build `[make_multiplier(2), make_multiplier(10)]` and apply each to `7`. Then write a one-liner
that uses `map` to triple every number in `[1, 2, 3]`.

## What you learned

- Functions are first-class values: store, pass, return, and list them.
- Higher-order builtins: `map`, `filter`, `sorted(key=...)`.
- A closure captures variables from its defining scope — even mutable ones.
- Closures give you private, independent state and let you compose behavior as data.
