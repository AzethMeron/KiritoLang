# Course 1 — Basics

Welcome to the Kirito course. Each lesson builds on the last; type the examples into the REPL
(`ki`) or save them to a `.ki` file and run them. By the end you'll have written several complete
programs.

Throughout, we use the `io` module for input/output:

```kirito
var io = import("io")
io.print("Hello, Kirito!")
```

## Lesson 1.1 — Values and variables

Kirito has integers, floats, strings, and booleans. `var` introduces a new name:

```kirito
var age = 30           # Integer
var price = 9.99       # Float
var name = "Ada"       # String
var ok = True          # Bool
io.print(f"{name} is {age}")
```

A name already declared is updated with plain `=` (no `var`):

```kirito
age = age + 1          # 31
```

**Try it:** declare `radius = 5`, then compute and print the area of a circle (`3.14159 * radius * radius`).

## Lesson 1.2 — Arithmetic, the careful way

Kirito keeps integers and floats distinct. Division with `/` always gives a Float; use `//` for
integer floor division:

```kirito
io.print(String(7 / 2))     # 3.5
io.print(String(7 // 2))    # 3
io.print(String(7 % 2))     # 1   (remainder)
io.print(String(2 ** 8))    # 256 (power)
```

Mixing a String with a number is an error, not a silent coercion — convert explicitly:

```kirito
var n = 42
io.print("answer = " + String(n))   # String(n) converts
```

## Lesson 1.3 — Making decisions

```kirito
var score = 72
if score >= 90:
    io.print("A")
elif score >= 80:
    io.print("B")
elif score >= 70:
    io.print("C")
else:
    io.print("try again")
```

Indentation defines the block — be consistent. Combine conditions with `and`, `or`, `not`:

```kirito
if score >= 60 and not score > 100:
    io.print("valid pass")
```

## Lesson 1.4 — Repeating work

`while` loops while a condition holds; `for` walks an iterable. `range(n)` gives `0..n-1`:

```kirito
var total = 0
for i in range(1, 11):       # 1..10
    total = total + i
io.print(String(total))      # 55

var i = 0
while i < 3:
    io.print(f"tick {i}")
    i = i + 1
```

`break` exits a loop; `continue` skips to the next iteration.

## Project 1 — FizzBuzz

Putting it together. Print numbers 1..20, but "Fizz" for multiples of 3, "Buzz" for multiples of 5,
and "FizzBuzz" for multiples of both:

```kirito
var io = import("io")
for n in range(1, 21):
    var out = ""
    if n % 3 == 0:
        out = out + "Fizz"
    if n % 5 == 0:
        out = out + "Buzz"
    if out == "":
        out = String(n)
    io.print(out)
```

**Why it works:** we build the output String piece by piece. The key trick is leaving `out` empty
and only falling back to the number when neither rule fired — so multiples of 15 naturally get
"FizzBuzz" because both `if`s run. Note there's no `else` chain: the two conditions are independent.

**Extend it:** make the limit configurable by reading `argv[0]` (`var limit = Integer(argv[0])`).
