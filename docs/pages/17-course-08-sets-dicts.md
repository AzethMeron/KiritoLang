# Lesson 8 — Sets and Dictionaries

Beyond the ordered List, Kirito gives you two collections built around fast lookup: the **Set**
(a collection of distinct values) and the **Dict** (a mapping from keys to values). Both answer "is
this here?" in roughly constant time.

## Sets: membership and uniqueness

A Set holds each value at most once and tests membership instantly:

```kirito
var io = import("io")
var seen = {3, 1, 4, 1, 5, 9, 2, 6, 5}   # duplicates collapse
io.print(len(seen))                       # => 7   (distinct values: 1 2 3 4 5 6 9)
io.print(3 in seen)                        # => True
io.print(7 in seen)                        # => False

seen.add(7)
seen.discard(3)                            # remove if present (no error if absent)
io.print(7 in seen, 3 in seen)             # => True False
```

> The literal `{}` is an empty **Dict**, not a Set — use `Set()` for an empty set. A brace literal
> with values like `{1, 2}` is a Set.

### Set algebra

Sets support the classic mathematical operations as methods:

```kirito
var a = {1, 2, 3, 4}
var b = {3, 4, 5, 6}
io.print(sorted(a.union(b)))            # => [1, 2, 3, 4, 5, 6]
io.print(sorted(a.intersection(b)))     # => [3, 4]
io.print(sorted(a.difference(b)))       # => [1, 2]
io.print(a.issubset({1, 2, 3, 4, 5}))   # => True
```

(We wrap results in `sorted(...)` only to get a stable order for printing — sets themselves are
unordered.)

A common use is **de-duplication**: `List(Set(items))` gives the distinct items.

## Dicts: keys to values

A Dict maps keys to values. Create one with `{key: value, ...}`, look up with `d[key]`, and assign
with `d[key] = value`:

```kirito
var ages = {"Ada": 36, "Alan": 41}
io.print(ages["Ada"])           # => 36
ages["Grace"] = 45              # add a new entry
ages["Ada"] = 37                # update an existing one
io.print(ages["Grace"], ages["Ada"])    # => 45 37
io.print("Alan" in ages)        # => True  (membership tests keys)
io.print(len(ages))             # => 3
```

Looking up a missing key is an error. When a key might be absent, use `.get`, which returns a
default instead of raising:

```kirito
io.print(ages.get("Edsger"))             # => None  (absent -> None by default)
io.print(ages.get("Edsger", 0))          # => 0     (your own default)
```

### Iterating a Dict

`.keys()`, `.values()`, and `.items()` give you the three views:

```kirito
var inventory = {"apples": 12, "pears": 7}
for name in inventory.keys():
    io.print(name)                       # apples and pears, in some order
for pair in inventory.items():
    io.print(f"{pair[0]}: {pair[1]}")    # each pair, in some order
```

A Dict does **not** promise any particular iteration order, so don't rely on one — sort the keys
first if you need a deterministic order (`for name in sorted(inventory.keys()):`). The next lesson
lets you write `for name, count in inventory.items()` to unpack each pair directly.

## A worked example: word frequency

```kirito
var io = import("io")

var word_counts = Function(text : String) -> Dict:
    var counts = {}
    for word in text.lower().split(" "):
        if len(word) == 0:
            continue
        # setdefault returns the current count (inserting 0 the first time we see a word).
        counts[word] = counts.setdefault(word, 0) + 1
    return counts

var tally = word_counts("the cat the dog the bird")
io.print(tally["the"])           # => 3
io.print(tally["cat"])           # => 1
```

**Walkthrough:** `setdefault(word, 0)` is the idiom that makes counting clean: the first time a word
appears it inserts `0` and returns it; thereafter it returns the running count. We add one and store
it back. The result is a Dict from each word to how often it occurred — the backbone of search
engines, histograms, and analytics. (The `collections` module's `Counter` automates exactly this.)

## Try it

Given two lists of names, print (1) the names in *both* (intersection), and (2) the names in the
first but not the second (difference). Build a Set from each list and use the set-algebra methods.

## What you learned

- Sets store distinct values with instant membership and set-algebra methods.
- `{}` is an empty Dict; `Set()` is an empty Set.
- Dicts map keys to values: `d[k]`, `d[k] = v`, `in`, `.get`, `.keys/.values/.items`.
- The `setdefault` counting idiom.
