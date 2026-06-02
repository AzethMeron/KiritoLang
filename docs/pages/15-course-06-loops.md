# Lesson 6 — Loops and Iteration

Loops are how programs do repetitive work. Kirito has two: `while` for "keep going until a condition
changes", and `for` for "do this once per item". A handful of built-in helpers make `for` loops
expressive.

## `while`

A `while` loop runs its body as long as the condition is truthy:

```kirito
var io = import("io")
var countdown = 3
while countdown > 0:
    io.print(countdown)
    countdown = countdown - 1
io.print("liftoff!")
# => 3 / 2 / 1 / liftoff!  (one per line)
```

The body must eventually make the condition false, or the loop runs forever — `countdown =
countdown - 1` is what guarantees progress here.

## `for ... in`

A `for` loop walks any **iterable** — a range, a List, a String, a Set, a Dict — binding each item to
a name in turn:

```kirito
for letter in "hi!":
    io.print(letter)           # => h / i / !

for item in ["apple", "pear"]:
    io.print(item)             # => apple / pear
```

## `range` generates number sequences

`range` is the workhorse of counting loops:

```kirito
for i in range(3):             # 0, 1, 2  — stop is exclusive
    io.print(i)

var total = 0
for n in range(1, 11):         # 1 through 10
    total = total + n
io.print(total)                # => 55

for even in range(0, 10, 2):   # start, stop, step
    io.print(even)             # => 0 2 4 6 8
```

## `break` and `continue`

`break` exits the loop immediately; `continue` skips to the next iteration:

```kirito
# Find the first multiple of 7 above 20.
var found = None
for n in range(21, 100):
    if n % 7 == 0:
        found = n
        break                  # stop as soon as we have it
io.print(found)                # => 21

# Print only the odd numbers.
for n in range(10):
    if n % 2 == 0:
        continue               # skip the evens
    io.print(n)                # => 1 3 5 7 9
```

## `enumerate` — index *and* item

When you need the position as well as the value, `enumerate` pairs them so you don't manage a
counter by hand:

```kirito
for pair in enumerate(["red", "green", "blue"]):
    var index = pair[0]
    var color = pair[1]
    io.print(f"{index}: {color}")
# => 0: red / 1: green / 2: blue
```

(In the next lesson on unpacking you'll write this as the cleaner `for index, color in
enumerate(...)`.)

## `zip` — walk two sequences together

`zip` pairs items from several iterables, stopping at the shortest:

```kirito
var names = ["Ada", "Alan", "Grace"]
var ages = [36, 41, 45]
for pair in zip(names, ages):
    io.print(f"{pair[0]} is {pair[1]}")
# => Ada is 36 / Alan is 41 / Grace is 45
```

## A worked example: an averaging loop

```kirito
var io = import("io")

var average = Function(numbers) -> Float:
    if len(numbers) == 0:
        return 0.0                       # avoid dividing by zero
    var running_total = 0
    for value in numbers:
        running_total = running_total + value
    return running_total / len(numbers)  # / always yields a Float

io.print(average([10, 20, 30, 40]))      # => 25.0
io.print(average([]))                     # => 0.0
```

**Walkthrough:** this is the classic *accumulator* pattern — initialize a total to zero, add each
item as the loop visits it, then use it after the loop. The empty-input guard up front keeps the
final division safe. (Kirito's builtin `sum` does the adding for you — `sum(numbers) /
len(numbers)` — but writing the loop once makes the pattern stick.)

## Try it

Write a loop that prints a 9×9 multiplication table, neatly aligned. (Hint: a `for` inside a `for`;
build each row as a String with an f-string format spec like `{product:4d}` for width-4 columns, and
`io.print` the row once per outer iteration.)

## What you learned

- `while` for condition-driven loops; `for ... in` for item-driven loops.
- `range(stop)`, `range(start, stop)`, `range(start, stop, step)`.
- `break` and `continue` to alter loop flow.
- `enumerate` for index+item and `zip` to walk sequences in parallel.
- The accumulator pattern.
