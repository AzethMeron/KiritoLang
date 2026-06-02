# Lesson 10 — Functions

Functions name a piece of work so you can reuse it, test it, and read your program at a higher level.
In Kirito a function is a **value** created with `Function(...)` and bound to a name like anything
else.

## Defining and calling

```kirito
var io = import("io")

var greet = Function(name):
    return f"Hello, {name}!"

io.print(greet("Ada"))         # => Hello, Ada!
```

The parameters go in the parentheses; the indented block is the body; `return` hands back a value.

## The default return is `None`

A function with no `return` (or a bare `return`) yields `None`. Such functions are run for their
*effect*, not their value:

```kirito
var log = Function(message):
    io.print(f"[log] {message}")
    # no return -> yields None

var result = log("starting up")     # => [log] starting up
io.print(result)                     # => None
```

## Default parameter values

A parameter can have a default, used when the caller omits it:

```kirito
var power = Function(base, exponent = 2):
    return base ** exponent

io.print(power(5))             # => 25   (exponent defaults to 2)
io.print(power(5, 3))          # => 125
```

## Keyword arguments

You can pass arguments by name, in any order — clearer at the call site and order-independent:

```kirito
var make_label = Function(text, width, fill = "-"):
    return text.center(width, fill)

io.print(make_label("hi", 8))                       # => ---hi---
io.print(make_label(width=8, text="hi", fill="*"))  # => ***hi***
```

Keyword arguments work for built-in and standard-library functions too — that's why you've been
writing `round(x, ndigits=2)` and `sorted(xs, reverse=True)`.

## Functions that call functions

Break a problem into small, single-purpose functions and compose them. Each one is easy to read and
test on its own:

```kirito
var io = import("io")

var is_vowel = Function(ch : String) -> Bool:
    return ch.lower() in "aeiou"

var count_vowels = Function(text : String) -> Integer:
    var n = 0
    for ch in text:
        if is_vowel(ch):
            n = n + 1
    return n

io.print(count_vowels("Hello World"))    # => 3
```

**Walkthrough:** `is_vowel` answers one tiny question; `count_vowels` uses it in a loop. Splitting the
work this way means you can test `is_vowel("e")` in isolation, and `count_vowels` reads almost like
prose. This is the habit that keeps large programs manageable.

## `discard` and `todo`

If you call a function only for its side effect and ignore a non-`None` result, Kirito's analyzer
warns that you dropped a value. Mark it `discard` to say "I meant to":

```kirito
var io = import("io")
discard io.input("Press Enter to continue... ")   # we don't need the line back
```

When you're sketching and haven't written a piece yet, `todo` is a no-op that emits a reminder
warning at its location (like `pass`, but it nags you):

<!--norun (todo emits a warning; shown for illustration)-->
```kirito
var parse_config = Function(path):
    todo "read and validate the config file"
```

## Try it

Write `clamp(value, low, high)` that returns `value` constrained to the range `[low, high]` (i.e. at
least `low`, at most `high`). Give `low` a default of `0` and `high` a default of `100`. Test it with
`clamp(150)`, `clamp(-5)`, and `clamp(42, low=10, high=50)`.

## What you learned

- `Function(params): ... return value` defines a function value; the default return is `None`.
- Default parameter values and keyword arguments (which work on builtins too).
- Composing small single-purpose functions.
- `discard` to intentionally drop a result; `todo` as a nagging placeholder.
