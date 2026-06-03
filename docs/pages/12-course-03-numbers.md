# Lesson 3 — Numbers and Arithmetic

Kirito takes numbers seriously. It keeps **integers** and **floats** as distinct types and follows
Python-3's arithmetic rules, so results are predictable instead of surprising.

## Two number types

`Integer` is a 64-bit whole number; `Float` is a double-precision decimal. Literals decide which you
get:

```kirito
var io = import("io")
var whole = 42          # Integer
var decimal = 42.0      # Float — the .0 makes the difference
io.print(type(whole), type(decimal))   # => Integer Float
```

## The arithmetic operators

```kirito
io.print(7 + 3)         # => 10
io.print(7 - 3)         # => 4
io.print(7 * 3)         # => 21
io.print(7 / 3)         # => 2.33333333333333   division — ALWAYS a Float
io.print(7 // 3)        # => 2     floor division — rounds toward negative infinity
io.print(7 % 3)         # => 1     modulo (remainder)
io.print(2 ** 10)       # => 1024  exponentiation (right-associative)
```

Two rules are worth burning in:

1. **`/` always produces a Float**, even for evenly divisible integers: `6 / 2` is `3.0`, not `3`.
   Use `//` when you want an Integer result.
2. **`//` and `%` use floor semantics** — the quotient rounds toward negative infinity and the
   remainder takes the sign of the divisor:

```kirito
io.print(-7 // 3)       # => -3   (not -2: it rounds DOWN)
io.print(-7 % 3)        # => 2    (sign follows the divisor)
```

`**` is right-associative, so `2 ** 3 ** 2` is `2 ** (3 ** 2)` = `2 ** 9` = `512`.

## Integer literals in any base

Integers can be written in decimal, hexadecimal, octal, or binary. The prefix is case-insensitive:

```kirito
io.print(255, 0xFF, 0o377, 0b11111111)   # => 255 255 255 255  (all the same value)
```

Float literals allow scientific notation:

```kirito
io.print(1e3, 1.5e3, 2e-3)               # => 1000.0 1500.0 0.002
```

## Overflow wraps, it doesn't crash

Integer arithmetic is fixed-width 64-bit with **well-defined two's-complement wraparound** — it
never triggers undefined behavior. You rarely hit this, but it's defined when you do:

```kirito
var biggest = 9223372036854775807        # the largest Integer
io.print(biggest + 1)                     # => -9223372036854775808  (wraps around)
```

## Rounding and absolute value

```kirito
io.print(abs(-5), abs(-2.5))             # => 5 2.5
io.print(round(3.14159))                  # => 3      (to the nearest Integer)
io.print(round(3.14159, ndigits=2))       # => 3.14   (to 2 decimal places -> Float)
io.print(divmod(17, 5))                   # => [3, 2] (quotient and remainder at once)
```

## The `math` module for everything else

Square roots, logs, trig, constants — they live in `math`:

```kirito
var math = import("math")
io.print(math.sqrt(144))                  # => 12.0
io.print(math.pi)                          # => 3.14159265358979  (15 significant digits)
io.print(math.floor(3.7), math.ceil(3.2)) # => 3 4
io.print(math.gcd(24, 36))                 # => 12
io.print(math.factorial(5))                # => 120
```

## Bitwise work is done by functions

Kirito deliberately has **no** `&`, `|`, `^`, `~`, `<<`, `>>` operators — those symbols are easy to
misread. Bitwise operations are explicit, named builtins on Integers instead:

```kirito
io.print(bitand(0b1100, 0b1010))          # => 8   (0b1000)
io.print(bitor(0b1100, 0b1010))           # => 14  (0b1110)
io.print(bitxor(0b1100, 0b1010))          # => 6   (0b0110)
io.print(shl(1, 4))                        # => 16  (1 shifted left 4 bits)
io.print(bin(shl(1, 4)))                   # => 0b10000  (bin() shows the binary form)
```

## Try it

Write a program that, given `total_seconds = 3725`, prints `1h 2m 5s`. Use `//` and `%` to peel off
hours, then minutes, then seconds. (Hint: `3725 // 3600` is the hours; `3725 % 3600` is what's left.)

## What you learned

- `Integer` vs `Float`, and why `/` is always a Float while `//` floors.
- Floor semantics for `//` and `%`, right-associative `**`.
- Hex/octal/binary/scientific literals, defined integer overflow.
- `abs`/`round`/`divmod`, the `math` module, and the named bitwise builtins.
