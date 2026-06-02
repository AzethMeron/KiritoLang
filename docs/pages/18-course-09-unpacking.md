# Lesson 9 — Packing and Unpacking

Kirito lets you move several values around at once. A bare comma **packs** values into a List, and
the left-hand side of an assignment **unpacks** an iterable into separate names. These two moves make
a lot of code shorter and clearer.

## Packing with commas

A comma-separated sequence builds a List:

```kirito
var io = import("io")
var point = 3, 4               # same as [3, 4]
io.print(point)                # => [3, 4]
io.print(type(point))          # => List
```

This is why returning several values from a function "just works" — you'll see it below.

## Unpacking into names

The left side of `var` or `=` can be a list of targets. The right side's items are distributed to
them, position by position:

```kirito
var first, second = [10, 20]
io.print(first, second)        # => 10 20

var a, b, c = "xyz"            # any iterable unpacks — here, a String
io.print(a, b, c)              # => x y z
```

The counts must match — too few or too many values is a clear error, which catches mistakes.

## The swap idiom

Because the right side is fully evaluated before assignment, you can swap two names without a
temporary variable:

```kirito
var left = 1
var right = 2
left, right = right, left
io.print(left, right)          # => 2 1
```

## Starred targets absorb the rest

Exactly one target may be **starred** (`*name`); it soaks up however many items are left over,
always as a List:

```kirito
var head, *tail = [1, 2, 3, 4]
io.print(head, tail)           # => 1 [2, 3, 4]

var *init, last = [1, 2, 3, 4]
io.print(init, last)           # => [1, 2, 3] 4

var lo, *mid, hi = [1, 2, 3, 4, 5]
io.print(lo, mid, hi)          # => 1 [2, 3, 4] 5
```

## Unpacking in `for` loops

This is where unpacking shines. When you iterate pairs, unpack them right in the loop header — no
more `pair[0]` / `pair[1]`:

```kirito
var prices = {"apple": 1.20, "pear": 0.90}
for name, cost in prices.items():
    io.print(f"{name} costs {cost}")
# => one line per entry (Dict order is not guaranteed)

for index, color in enumerate(["red", "green", "blue"]):
    io.print(f"{index}: {color}")
# => 0: red / 1: green / 2: blue   (a List, so order IS guaranteed)
```

## Returning multiple values

A function returns several values by listing them with commas (which packs them into a List); the
caller unpacks:

```kirito
var io = import("io")

# Return both the quotient and the remainder.
var divide = Function(numerator : Integer, denominator : Integer):
    return numerator // denominator, numerator % denominator

var quotient, remainder = divide(17, 5)
io.print(f"17 = 5 * {quotient} + {remainder}")   # => 17 = 5 * 3 + 2
```

**Walkthrough:** `return a, b` packs the two results into a List, and `var quotient, remainder = ...`
unpacks them into two names in one line. This reads far better than returning a list the caller has
to index into. (Kirito's builtin `divmod` does exactly this pairing.)

## Unpack targets can be more than names

A target can be an index or a member, not just a bare name — handy for filling slots in place:

```kirito
var grid = [0, 0, 0]
grid[0], grid[2] = 9, 7
io.print(grid)                 # => [9, 0, 7]
```

## Try it

Write `minmax(numbers)` that returns the smallest and largest in one `return`, and call it with
`var lo, hi = minmax([4, 9, 1, 7])`. (Hint: the builtins `min` and `max` do the heavy lifting;
`return min(numbers), max(numbers)`.)

## What you learned

- A bare comma packs values into a List.
- The left side of `=`/`var`/`for` unpacks any iterable into names (counts checked).
- The swap idiom, starred targets that absorb the surplus.
- `for k, v in d.items()` / `for i, x in enumerate(...)`, and returning multiple values.
