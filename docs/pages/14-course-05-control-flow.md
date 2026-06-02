# Lesson 5 — Booleans and Control Flow

Programs make decisions. This lesson covers comparisons, the logical operators, the `if` family, and
Kirito's `switch` statement for clean multi-way branching.

## Comparisons produce Bools

```kirito
var io = import("io")
io.print(3 < 5)          # => True
io.print(3 == 3.0)       # => True   (Integer and Float compare by value)
io.print("a" < "b")      # => True   (strings compare lexicographically)
io.print(3 != 4)         # => True
io.print(5 >= 5)         # => True
```

## Combining conditions: `and`, `or`, `not`

These are spelled as words, and `and`/`or` **short-circuit** — they stop evaluating as soon as the
answer is known:

```kirito
var age = 25
var has_ticket = True
if age >= 18 and has_ticket:
    io.print("admitted")           # => admitted

io.print(not False)                 # => True
io.print(True or undefined_func())  # => True  (the right side is never evaluated)
```

Short-circuiting is not just an optimization — it lets you guard a risky operation with a cheap
check on its left: `if count > 0 and total / count > threshold:`.

## `if` / `elif` / `else`

Indentation defines the block. Be consistent (pick spaces or tabs and stick with it — Kirito rejects
ambiguous mixing):

```kirito
var score = 72
if score >= 90:
    io.print("A")
elif score >= 80:
    io.print("B")
elif score >= 70:
    io.print("C")            # => C
else:
    io.print("try again")
```

An `if` can stand alone (no `else`), and `elif` chains can be as long as you like — but when you're
dispatching on a single value against many constants, `switch` is cleaner.

## The conditional expression

When you just need to *choose between two values*, the conditional expression
`then if cond else orelse` is more concise than a four-line `if`/`else`. It's an **expression**, so it
produces a value you can assign, return, or pass along:

```kirito
var score = 72
var outcome = "pass" if score >= 60 else "fail"
io.print(outcome)                       # => pass

var n = -5
var sign = "positive" if n > 0 else "non-positive"
io.print(f"the number is {sign}")       # => the number is non-positive
```

Only the chosen branch is evaluated (it short-circuits like `and`/`or`), and you can chain it
right-associatively for a few ranges:

```kirito
var x = 75
var band = "high" if x >= 90 else "mid" if x >= 50 else "low"
io.print(band)                          # => mid
```

Reach for it for a quick two-way pick; for anything with side effects or more than a couple of
branches, a statement `if`/`elif`/`else` reads better.

## `switch` for multi-way branching

`switch` matches a subject against `case` labels. Exactly one arm runs — **there is no
fall-through** — and an optional `default` handles everything else:

```kirito
var describe_day = Function(day : String) -> String:
    switch day:
        case "Sat", "Sun":              # one arm, several labels
            return "weekend"
        case "Mon":
            return "back to work"
        default:
            return "a weekday"

io.print(describe_day("Sun"))           # => weekend
io.print(describe_day("Mon"))           # => back to work
io.print(describe_day("Wed"))           # => a weekday
```

Case labels are constant scalars (Integer, Float, String, Bool, None), matched by exact type *and*
value — so `case 1` does not match `1.0`. Internally the arms compile to a hash jump-table, so a
`switch` with 100 cases dispatches as fast as one with two. `case` and `default` are "soft keywords":
they're only special inside a `switch`, so you can still use them as ordinary variable names
elsewhere.

## A worked example: a tiny grading function two ways

```kirito
var io = import("io")

# As an if/elif chain — natural for ranges.
var letter_grade = Function(score : Integer) -> String:
    if score >= 90:
        return "A"
    elif score >= 80:
        return "B"
    elif score >= 70:
        return "C"
    else:
        return "F"

# As a switch — natural for exact matches.
var traffic_action = Function(light : String) -> String:
    switch light:
        case "red":
            return "stop"
        case "yellow":
            return "slow down"
        case "green":
            return "go"
        default:
            return "broken light — proceed with caution"

io.print(letter_grade(85), traffic_action("red"))   # => B stop
```

**Why two tools?** Ranges (`>= 90`) need comparisons, so an `if`/`elif` chain fits. Matching a value
against a fixed set of constants is exactly what `switch` is for — it reads as a table and runs in
constant time regardless of how many arms it has.

## Try it

Write `season(month)` taking an Integer 1–12 and returning `"winter"`, `"spring"`, `"summer"`, or
`"autumn"`. Try it once with `if`/`elif` and once with a `switch` (grouping months per arm,
`case 12, 1, 2:`). Which reads better here?

## What you learned

- Comparisons produce Bools; `and`/`or`/`not` are words and short-circuit.
- `if`/`elif`/`else` with indentation-defined blocks.
- The conditional expression `then if cond else orelse` for a concise two-way value pick.
- `switch`/`case`/`default`: no fall-through, exact scalar matching, constant-time dispatch.
