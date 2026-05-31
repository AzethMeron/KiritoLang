# Course 3 — Classes, Errors, and a Capstone

## Lesson 3.1 — Your own types

A `class` bundles data (attributes) with behavior (methods). The first parameter of every method is
the receiver, by convention `self`. `_init_` runs when you call the class to construct an instance:

```kirito
class Point:
    var _init_ = Function(self, x, y):
        self.x = x
        self.y = y
    var distance = Function(self, other) -> Float:
        var dx = self.x - other.x
        var dy = self.y - other.y
        return (dx * dx + dy * dy) ** 0.5

var a = Point(0, 0)
var b = Point(3, 4)
io.print(String(a.distance(b)))     # 5.0
```

## Lesson 3.2 — Operators and printing

Define special methods (single underscores both sides) to make your type behave like a built-in:

```kirito
class Vec:
    var _init_ = Function(self, x, y):
        self.x = x
        self.y = y
    var _add_ = Function(self, other):
        return Vec(self.x + other.x, self.y + other.y)
    var _str_ = Function(self) -> String:
        return f"Vec({self.x}, {self.y})"

var v = Vec(1, 2) + Vec(3, 4)
io.print(String(v))                 # Vec(4, 6)
```

Useful ones: `_eq_`, `_lt_` (sorting/comparison), `_getitem_`/`_setitem_` (indexing), `_len_`,
`_contains_` (`in`), `_iter_` (`for`), `_call_` (make instances callable).

## Lesson 3.3 — Inheritance

A subclass reuses and overrides:

```kirito
class Shape:
    var _init_ = Function(self, name):
        self.name = name
    var area = Function(self) -> Float:
        return 0.0

class Circle(Shape):
    var _init_ = Function(self, r):
        self.name = "circle"
        self.r = r
    var area = Function(self) -> Float:
        return 3.14159 * self.r * self.r

var c = Circle(2)
io.print(f"{c.name}: {c.area()}")
io.print(String(isinstance(c, "Shape")))   # True — inheritance-aware
```

Attributes/methods whose name has a single leading underscore (e.g. `_cache`) are **private**:
reachable only from inside methods of the same class.

To *extend* a parent method instead of fully replacing it, use `self._super_()` — a view of the
instance whose lookup starts at the base class:

```kirito
class Circle(Shape):
    var _init_ = Function(self, r):
        self._super_()._init_("circle")    # reuse Shape's constructor
        self.r = r
```

`_super_()` only works when the class inherits (it throws otherwise) and climbs one level per call,
so it composes through multi-level hierarchies.

## Lesson 3.4 — Errors

Signal a problem with `throw`; handle it with `try`/`catch`/`finally`:

```kirito
class RangeError:
    var _init_ = Function(self, msg):
        self.msg = msg

var checkedSqrt = Function(x : Float) -> Float:
    if x < 0.0:
        throw RangeError("cannot sqrt a negative")
    return x ** 0.5

try:
    io.print(String(checkedSqrt(-1.0)))
catch RangeError as e:
    io.print(f"error: {e.msg}")
finally:
    io.print("done")
```

`catch RangeError as e` matches that class (and its subclasses); a bare `catch:` catches anything.
If you call something only for its exception and ignore the return value, mark it `discard` to keep
the linter quiet:

```kirito
discard checkedSqrt(4.0)
```

## Capstone Project — A tiny RPN calculator

A Reverse Polish Notation calculator evaluates expressions like `3 4 + 5 *` (= 35) using a stack.
This pulls together classes, private members, exceptions, strings, and control flow.

```kirito
var io = import("io")

class CalcError:
    var _init_ = Function(self, msg):
        self.msg = msg

class RPN:
    var _init_ = Function(self):
        self._stack = []                    # private

    var _push = Function(self, x):
        self._stack.append(x)

    var _binop = Function(self, op):
        if len(self._stack) < 2:
            throw CalcError("not enough operands for " + op)
        var b = self._stack.pop()
        var a = self._stack.pop()
        if op == "+":
            self._push(a + b)
        elif op == "-":
            self._push(a - b)
        elif op == "*":
            self._push(a * b)
        elif op == "/":
            if b == 0.0:
                throw CalcError("division by zero")
            self._push(a / b)
        else:
            throw CalcError("unknown operator " + op)

    var eval = Function(self, expr : String) -> Float:
        self._stack = []
        for token in expr.split(" "):
            var t = token.strip()
            if len(t) == 0:
                continue
            if t == "+" or t == "-" or t == "*" or t == "/":
                self._binop(t)
            else:
                self._push(Float(t))
        if len(self._stack) != 1:
            throw CalcError("malformed expression")
        return self._stack[0]

var calc = RPN()
for expr in ["3 4 +", "3 4 + 5 *", "10 2 /", "1 0 /"]:
    try:
        io.print(f"{expr} = {calc.eval(expr)}")
    catch CalcError as e:
        io.print(f"{expr} -> error: {e.msg}")
```

**Walkthrough:**
- The **stack** is a private List `_stack`. Operands are pushed; operators pop two values, combine
  them, and push the result — the essence of RPN.
- `_binop` centralizes the four operators (DRY) and validates: too few operands or a divide-by-zero
  becomes a `CalcError` with an actionable message rather than a crash.
- `eval` tokenizes on spaces, routing each token to either "push a number" or "apply an operator".
  At the end a well-formed expression leaves exactly one value on the stack — anything else is a
  malformed input.
- The driver loop shows the payoff of exceptions: the happy path reads cleanly, and every failure
  mode (including `1 0 /`) is caught and reported uniformly, the VM continuing to the next case.

**Where to go next:** add support for `**` (power) and a `swap` command; let `eval` accept negative
numbers; or wrap it in a REPL using `io.input` so you can type expressions interactively. Then
browse the **Standard Library** page and the `examples/` directory for larger, real programs.
