# Lesson 7 — Lists

A **List** is an ordered, mutable sequence — the most-used collection in Kirito. It holds values of
any type, grows and shrinks on demand, and supports indexing, slicing, and a generous set of methods.

## Creating and indexing

```kirito
var io = import("io")
var primes = [2, 3, 5, 7, 11]
io.print(primes[0])       # => 2     (indices start at 0)
io.print(primes[-1])      # => 11    (negative counts from the end)
io.print(len(primes))     # => 5
io.print(primes[1:4])     # => [3, 5, 7]   (a slice — a new List)
```

Lists can mix types and nest:

```kirito
var mixed = [1, "two", 3.0, [4, 5]]
io.print(mixed[3][0])     # => 4   (index into the nested list)
```

## Growing and shrinking

```kirito
var stack = []
stack.append(10)          # add to the end
stack.append(20)
stack.append(30)
io.print(stack)           # => [10, 20, 30]
var top = stack.pop()     # remove and return the last item
io.print(top, stack)      # => 30 [10, 20]
stack.insert(0, 5)        # insert at an index
io.print(stack)           # => [5, 10, 20]
stack.remove(10)          # remove the first value equal to 10
io.print(stack)           # => [5, 20]
```

## Searching and counting

```kirito
var letters = ["a", "b", "a", "c", "a"]
io.print("a" in letters)        # => True   (membership test)
io.print(letters.index("b"))    # => 1      (position of the first match)
io.print(letters.count("a"))    # => 3      (how many equal)
```

## Combining lists

`+` concatenates into a new list; `*` repeats; `.extend` appends another list in place:

```kirito
io.print([1, 2] + [3, 4])       # => [1, 2, 3, 4]
io.print([0] * 4)                # => [0, 0, 0, 0]   (handy for fixed-size buffers)
var xs = [1, 2]
xs.extend([3, 4])
io.print(xs)                     # => [1, 2, 3, 4]
```

## Sorting

`.sort()` sorts **in place**; the builtin `sorted(...)` returns a **new** sorted list and leaves the
original alone. Both are *stable* and both accept `key` and `reverse`:

```kirito
var scores = [42, 17, 99, 8, 53]
io.print(sorted(scores))                 # => [8, 17, 42, 53, 99]   (original unchanged)
io.print(sorted(scores, reverse=True))   # => [99, 53, 42, 17, 8]

var words = ["banana", "fig", "cherry"]
io.print(sorted(words, key=len))         # => [fig, banana, cherry]  (by length)

scores.sort()                            # in place
io.print(scores)                         # => [8, 17, 42, 53, 99]
```

`key` is a function applied to each element to produce its sort key (computed once per element). You
can even sort by several keys at once by returning a List as the key — it compares element by
element, like a tuple.

## A worked example: top-N by a computed key

```kirito
var io = import("io")

# Each record is [name, score]; return the names of the top `n` scorers.
var top_scorers = Function(records, n : Integer):
    # Sort by score (the 1-th field) descending, then take the first n names.
    var ranked = sorted(records, key=Function(record): return record[1], reverse=True)
    var winners = []
    for record in ranked[0:n]:
        winners.append(record[0])
    return winners

var leaderboard = [["Ada", 95], ["Alan", 88], ["Grace", 99], ["Edsger", 91]]
io.print(top_scorers(leaderboard, 2))    # => [Grace, Ada]
```

**Walkthrough:** the inline `Function(record): return record[1]` is the sort key — it pulls the score
out of each `[name, score]` pair. `reverse=True` ranks high-to-low, the slice `ranked[0:n]` keeps the
top `n`, and the final loop projects out just the names. (Lesson 11 dives into these inline
functions.)

## Lists are shared, not copied

Recall from Lesson 2: binding a list to a second name shares it. When you want an independent list,
use `.copy()` (a shallow copy) or `copy.deepcopy` for nested structures:

```kirito
var original = [1, 2, 3]
var independent = original.copy()
independent.append(4)
io.print(original, independent)          # => [1, 2, 3] [1, 2, 3, 4]
```

## Try it

Write `running_max(numbers)` that returns a new list where each element is the largest value seen so
far. `running_max([3, 1, 4, 1, 5, 9, 2])` should give `[3, 3, 4, 4, 5, 9, 9]`. (Hint: track a
`best` value and `append` `max(best, value)` each step.)

## What you learned

- Creating, indexing (incl. negative), and slicing lists.
- `append`/`pop`/`insert`/`remove`/`extend`, `in`/`index`/`count`.
- `+`/`*` to combine, `sort` (in place) vs `sorted` (new), with `key` and `reverse`.
- Lists are shared references; `.copy()` makes an independent one.
