# Course 2 — Data and Functions

## Lesson 2.1 — Lists

Lists are ordered and mutable:

```kirito
var xs = [3, 1, 2]
xs.append(4)            # [3, 1, 2, 4]
xs.sort()               # [1, 2, 3, 4]  (in place, stable)
io.print(String(xs[0])) # 1
io.print(String(xs[-1]))# 4  (negative index = from the end)
io.print(String(xs[1:3]))   # [2, 3]  (slice)
io.print(String(len(xs)))
```

Slicing accepts negative bounds and a step: `xs[::-1]` reverses, `xs[::2]` takes every other.

## Lesson 2.2 — Dicts and Sets

A Dict maps keys to values; a Set holds unique values:

```kirito
var ages = {"Ann": 30, "Bo": 25}
ages["Cy"] = 41
io.print(String(ages.get("Zoe", 0)))   # 0 — default when missing
for name, age in ages.items():
    io.print(f"{name}: {age}")

var seen = {1, 2, 2, 3}     # {1, 2, 3}
seen.add(4)
io.print(String(3 in seen)) # True
```

## Lesson 2.3 — Strings in depth

Strings are Unicode and indexed by character:

```kirito
var s = "Hello, World"
io.print(s.upper())
io.print(String(s.split(", ")))     # ["Hello", "World"]
io.print("-".join(["a", "b", "c"])) # a-b-c
io.print(String(s.find("World")))   # 7
io.print(f"length: {len(s)}")
```

## Lesson 2.4 — Functions

Functions are values. Define one with `Function` and call it by name:

```kirito
var square = Function(x):
    return x * x
io.print(String(square(9)))   # 81
```

Parameters can have **defaults** and be passed by **name**:

```kirito
var greet = Function(name, greeting = "Hello"):
    return f"{greeting}, {name}!"
io.print(greet("Ada"))                    # Hello, Ada!
io.print(greet("Ada", greeting = "Hi"))   # Hi, Ada!
```

Add **type annotations** to make the contract enforced at runtime:

```kirito
var half = Function(n : Integer) -> Float:
    return n / 2
io.print(String(half(7)))     # 3.5
# half("x")  -> error: argument 'n' must be Integer, got String
```

## Lesson 2.5 — Multiple values

Return several values with a comma (they pack into a List), and unpack on the left:

```kirito
var stats = Function(xs):
    return min(xs), max(xs), sum(xs)
var lo, hi, total = stats([4, 1, 7, 3])
io.print(f"min={lo} max={hi} sum={total}")
```

A starred target grabs the rest: `var first, *others = [1, 2, 3, 4]`.

## Lesson 2.6 — Higher-order functions

Functions can take and return functions. `map`, `filter`, and `sorted` accept a function:

```kirito
var nums = [1, 2, 3, 4, 5, 6]
var evens = filter(Function(x): return x % 2 == 0, nums)
var doubled = map(Function(x): return x * 2, nums)
io.print(String(evens))     # [2, 4, 6]
io.print(String(doubled))   # [2, 4, 6, 8, 10, 12]

var words = ["pear", "fig", "banana"]
io.print(String(sorted(words, Function(w): return len(w))))  # by length
```

## Project 2 — A word-frequency counter

Read text, count how often each word appears, and print the top results.

```kirito
var io = import("io")

var countWords = Function(text : String) -> Dict:
    var counts = {}
    for raw in text.split(" "):
        var word = raw.strip().lower()
        if len(word) > 0:
            counts[word] = counts.get(word, 0) + 1
    return counts

var topN = Function(counts : Dict, n : Integer):
    var pairs = counts.items()
    var ranked = sorted(pairs, Function(p): return p[1], True)   # by count, descending
    return ranked[0:n]

var text = "the cat sat on the mat the cat ran"
var counts = countWords(text)
for word, n in topN(counts, 3):
    io.print(f"{word}: {n}")
```

**Why it works:**
- `countWords` normalizes each word (`strip`, `lower`) and uses `counts.get(word, 0) + 1` so the
  first sighting starts from 0 — no special-casing the missing key.
- `topN` turns the dict into a list of `[word, count]` pairs with `.items()`, then `sorted(..., True)`
  sorts descending by the key function `p[1]` (the count). Slicing `[0:n]` keeps the top `n`.
- The annotations (`: String`, `: Dict`, `: Integer`, `-> Dict`) document and *enforce* the shapes,
  so calling these wrong fails fast with a clear message.

**Extend it:** read the text from a file (`io.open`) and ignore punctuation by `replace`-ing it
before splitting.
