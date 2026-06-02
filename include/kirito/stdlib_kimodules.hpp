#ifndef KIRITO_STDLIB_KIMODULES_HPP
#define KIRITO_STDLIB_KIMODULES_HPP

// Standard-library modules authored in idiomatic Kirito and frozen into the binary as source. Each
// is registered with vm.registerSourceModule(name, src) and evaluated once per VM on first import.
// Keeping these in Kirito (rather than C++) dogfoods the language and keeps the modules terse; the
// performance-critical primitives they build on (list/dict/set, sort, string ops) are already native.

#include <string_view>

namespace kirito::kimod {

// --- itertools: lazy-style combinators returning Lists (Kirito has no generators yet) ----------
inline constexpr std::string_view itertools = R"KI(
var count = Function(start = 0, step = 1, stop = None):
    # Bounded count: requires stop (unbounded would never return a List). Returns [start, start+step, ...).
    if stop == None:
        throw "itertools.count needs a stop bound (no lazy generators)"
    var out = []
    var x = start
    while x < stop:
        out.append(x)
        x = x + step
    return out

var repeat = Function(value, times):
    var out = []
    for i in range(times):
        out.append(value)
    return out

var cycle = Function(iterable, times):
    var out = []
    for i in range(times):
        for x in iterable:
            out.append(x)
    return out

var chain = Function(lists):
    var out = []
    for lst in lists:
        for x in lst:
            out.append(x)
    return out

var islice = Function(iterable, start, stop, step = 1):
    var out = []
    var i = 0
    for x in iterable:
        if i >= stop:
            break
        if i >= start and (i - start) % step == 0:
            out.append(x)
        i = i + 1
    return out

var accumulate = Function(iterable, func = None):
    var out = []
    var total = None
    for x in iterable:
        if total == None:
            total = x
        elif func == None:
            total = total + x
        else:
            total = func(total, x)
        out.append(total)
    return out

var product = Function(lists):
    # Cartesian product of a list of lists -> list of lists.
    var out = [[]]
    for pool in lists:
        var nxt = []
        for prefix in out:
            for item in pool:
                var combo = prefix.copy()
                combo.append(item)
                nxt.append(combo)
        out = nxt
    return out

var permutations = Function(items, r = None):
    var n = len(items)
    var k = r
    if k == None:
        k = n
    var result = []
    var helper = Function(chosen, used):
        if len(chosen) == k:
            result.append(chosen.copy())
            return None
        for i in range(n):
            if not used[i]:
                used[i] = True
                chosen.append(items[i])
                helper(chosen, used)
                chosen.pop()
                used[i] = False
    var used0 = []
    for i in range(n):
        used0.append(False)
    helper([], used0)
    return result

var combinations = Function(items, r):
    var n = len(items)
    var result = []
    var helper = Function(start, chosen):
        if len(chosen) == r:
            result.append(chosen.copy())
            return None
        for i in range(start, n):
            chosen.append(items[i])
            helper(i + 1, chosen)
            chosen.pop()
    helper(0, [])
    return result

var takewhile = Function(pred, iterable):
    var out = []
    for x in iterable:
        if not pred(x):
            break
        out.append(x)
    return out

var dropwhile = Function(pred, iterable):
    var out = []
    var dropping = True
    for x in iterable:
        if dropping and pred(x):
            continue
        dropping = False
        out.append(x)
    return out

var filterfalse = Function(pred, iterable):
    var out = []
    for x in iterable:
        if not pred(x):
            out.append(x)
    return out

var compress = Function(data, selectors):
    var out = []
    var i = 0
    for x in data:
        if i < len(selectors) and selectors[i]:
            out.append(x)
        i = i + 1
    return out

var starmap = Function(func, argtuples):
    var out = []
    for args in argtuples:
        out.append(func(args))
    return out

var pairwise = Function(iterable):
    var out = []
    var prev = None
    var first = True
    for x in iterable:
        if not first:
            out.append([prev, x])
        prev = x
        first = False
    return out

var ziplongest = Function(lists, fillvalue = None):
    var longest = 0
    for lst in lists:
        if len(lst) > longest:
            longest = len(lst)
    var out = []
    for i in range(longest):
        var row = []
        for lst in lists:
            if i < len(lst):
                row.append(lst[i])
            else:
                row.append(fillvalue)
        out.append(row)
    return out

var groupby = Function(iterable, key = None):
    # Group CONSECUTIVE elements sharing a key -> list of [key, [members...]] (like Python).
    var out = []
    var haveCur = False
    var curKey = None
    var curGroup = []
    for x in iterable:
        var k = x
        if key != None:
            k = key(x)
        if haveCur and k == curKey:
            curGroup.append(x)
        else:
            if haveCur:
                out.append([curKey, curGroup])
            curKey = k
            curGroup = [x]
            haveCur = True
    if haveCur:
        out.append([curKey, curGroup])
    return out
)KI";

// --- functools ---------------------------------------------------------------------------------
inline constexpr std::string_view functools = R"KI(
var reduce = Function(func, iterable, initial = None):
    var acc = initial
    var first = True
    for x in iterable:
        if first and initial == None:
            acc = x
            first = False
        else:
            acc = func(acc, x)
            first = False
    if acc == None and initial == None:
        throw "reduce of empty sequence with no initial value"
    return acc

var partial = Function(func, bound):
    # partial(f, [a, b]) -> g(rest...) calls f(a, b, rest...). `bound` is a list of leading args.
    return Function(rest):
        var allargs = bound.copy()
        for x in rest:
            allargs.append(x)
        return func(allargs)

var cache = Function(func):
    # Memoize a single-argument function (the argument must be hashable). Returns a wrapper.
    var store = {}
    return Function(x):
        if x not in store:
            store[x] = func(x)
        return store[x]
)KI";

// --- collections: deque, Counter, defaultdict, OrderedDict (as classes) ------------------------
inline constexpr std::string_view collections = R"KI(
class deque:
    var _init_ = Function(self, items = None):
        self._items = []
        if items != None:
            for x in items:
                self._items.append(x)
    var append = Function(self, x):
        self._items.append(x)
    var appendleft = Function(self, x):
        self._items.insert(0, x)
    var pop = Function(self):
        return self._items.pop()
    var popleft = Function(self):
        return self._items.pop(0)
    var _len_ = Function(self) -> Integer:
        return len(self._items)
    var _getitem_ = Function(self, i):
        return self._items[i]
    var _str_ = Function(self) -> String:
        return "deque(" + String(self._items) + ")"
    var _iter_ = Function(self):
        return self._items

class Counter:
    var _init_ = Function(self, items = None):
        self._counts = {}
        if items != None:
            for x in items:
                self.add(x)
    var add = Function(self, x):
        if x in self._counts:
            self._counts[x] = self._counts[x] + 1
        else:
            self._counts[x] = 1
    var get = Function(self, x):
        return self._counts.get(x, 0)
    var items = Function(self):
        return self._counts.items()
    var mostcommon = Function(self):
        var pairs = self._counts.items()
        return sorted(pairs, Function(p): return p[1], True)
    var _getitem_ = Function(self, x):
        return self._counts.get(x, 0)
    var _str_ = Function(self) -> String:
        return "Counter(" + String(self._counts) + ")"

class defaultdict:
    var _init_ = Function(self, factory):
        self._factory = factory
        self._data = {}
    var _getitem_ = Function(self, k):
        if k not in self._data:
            self._data[k] = self._factory()
        return self._data[k]
    var _setitem_ = Function(self, k, v):
        self._data[k] = v
    var _contains_ = Function(self, k):
        return k in self._data
    var keys = Function(self):
        return self._data.keys()
    var values = Function(self):
        return self._data.values()
    var items = Function(self):
        return self._data.items()
    var _str_ = Function(self) -> String:
        return "defaultdict(" + String(self._data) + ")"
)KI";

// --- statistics --------------------------------------------------------------------------------
inline constexpr std::string_view statistics = R"KI(
var _math = import("math")

var mean = Function(data) -> Float:
    if len(data) == 0:
        throw "mean requires at least one data point"
    return Float(sum(data)) / Float(len(data))

var median = Function(data) -> Float:
    var s = sorted(data)
    var n = len(s)
    if n == 0:
        throw "median requires at least one data point"
    if n % 2 == 1:
        return Float(s[n // 2])
    return (Float(s[n // 2 - 1]) + Float(s[n // 2])) / 2.0

var mode = Function(data):
    var counts = {}
    var best = None
    var bestCount = 0
    for x in data:
        var c = counts.get(x, 0) + 1
        counts[x] = c
        if c > bestCount:
            bestCount = c
            best = x
    return best

var variance = Function(data) -> Float:
    var n = len(data)
    if n < 2:
        throw "variance requires at least two data points"
    var m = mean(data)
    var total = 0.0
    for x in data:
        total = total + (Float(x) - m) * (Float(x) - m)
    return total / Float(n - 1)

var pvariance = Function(data) -> Float:
    var n = len(data)
    if n < 1:
        throw "pvariance requires at least one data point"
    var m = mean(data)
    var total = 0.0
    for x in data:
        total = total + (Float(x) - m) * (Float(x) - m)
    return total / Float(n)

var stdev = Function(data) -> Float:
    return _math.sqrt(variance(data))

var pstdev = Function(data) -> Float:
    return _math.sqrt(pvariance(data))

var multimode = Function(data):
    # All values tied for the highest count, in first-seen order.
    var counts = {}
    var order = []
    var best = 0
    for x in data:
        if x not in counts:
            order.append(x)
        var c = counts.get(x, 0) + 1
        counts[x] = c
        if c > best:
            best = c
    var out = []
    for x in order:
        if counts[x] == best:
            out.append(x)
    return out

var quantiles = Function(data, n = 4):
    # Cut points dividing sorted data into n equal groups (exclusive method, like Python default).
    var s = sorted(data)
    var ld = len(s)
    if ld < 2:
        throw "quantiles requires at least two data points"
    var out = []
    var i = 1
    while i < n:
        var pos = Float(i) * Float(ld + 1) / Float(n)
        var lo = Integer(pos)
        if lo < 1:
            out.append(Float(s[0]))
        elif lo >= ld:
            out.append(Float(s[ld - 1]))
        else:
            var frac = pos - Float(lo)
            out.append(Float(s[lo - 1]) + frac * (Float(s[lo]) - Float(s[lo - 1])))
        i = i + 1
    return out
)KI";

// --- string constants and helpers --------------------------------------------------------------
inline constexpr std::string_view string_mod = R"KI(
var ascii_lowercase = "abcdefghijklmnopqrstuvwxyz"
var ascii_uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
var ascii_letters = ascii_lowercase + ascii_uppercase
var digits = "0123456789"
var hexdigits = "0123456789abcdefABCDEF"
var octdigits = "01234567"
var punctuation = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
var whitespace = " \t\n\r"

var capwords = Function(s) -> String:
    var parts = []
    for word in s.split(" "):
        if len(word) > 0:
            parts.append(word[0:1].upper() + word[1:].lower())
        else:
            parts.append(word)
    return " ".join(parts)
)KI";

// --- textwrap ----------------------------------------------------------------------------------
inline constexpr std::string_view textwrap = R"KI(
var wrap = Function(text, width = 70):
    var words = text.split(" ")
    var lines = []
    var current = ""
    for word in words:
        if len(current) == 0:
            current = word
        elif len(current) + 1 + len(word) <= width:
            current = current + " " + word
        else:
            lines.append(current)
            current = word
    if len(current) > 0:
        lines.append(current)
    return lines

var fill = Function(text, width = 70):
    return "\n".join(wrap(text, width))

var indent = Function(text, prefix) -> String:
    var lines = text.split("\n")
    var out = []
    for line in lines:
        if len(line) > 0:
            out.append(prefix + line)
        else:
            out.append(line)
    return "\n".join(out)

var dedent = Function(text) -> String:
    var lines = text.split("\n")
    var minIndent = None
    for line in lines:
        var stripped = line.lstrip()
        if len(stripped) > 0:
            var ind = len(line) - len(stripped)
            if minIndent == None or ind < minIndent:
                minIndent = ind
    if minIndent == None or minIndent == 0:
        return text
    var out = []
    for line in lines:
        if len(line) >= minIndent:
            out.append(line[minIndent:])
        else:
            out.append(line)
    return "\n".join(out)
)KI";

// --- base64 (standard alphabet) ----------------------------------------------------------------
inline constexpr std::string_view base64 = R"KI(
var _alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

var _index = {}
var _i = 0
while _i < len(_alphabet):
    _index[_alphabet[_i]] = _i
    _i = _i + 1

var encode = Function(data) -> String:
    # data: a List of byte values (0..255). Returns the base64 String.
    var out = ""
    var i = 0
    var n = len(data)
    while i < n:
        var b0 = data[i]
        var b1 = 0
        var b2 = 0
        var have = 1
        if i + 1 < n:
            b1 = data[i + 1]
            have = 2
        if i + 2 < n:
            b2 = data[i + 2]
            have = 3
        var triple = b0 * 65536 + b1 * 256 + b2
        out = out + _alphabet[(triple // 262144) % 64]
        out = out + _alphabet[(triple // 4096) % 64]
        if have >= 2:
            out = out + _alphabet[(triple // 64) % 64]
        else:
            out = out + "="
        if have >= 3:
            out = out + _alphabet[triple % 64]
        else:
            out = out + "="
        i = i + 3
    return out

var decode = Function(s):
    # Returns a List of byte values.
    var out = []
    var buffer = 0
    var bits = 0
    for ch in s:
        if ch == "=":
            break
        buffer = buffer * 64 + _index[ch]
        bits = bits + 6
        if bits >= 8:
            bits = bits - 8
            out.append((buffer // (2 ** bits)) % 256)
    return out

# URL-safe variant: '+' -> '-', '/' -> '_'. Encodes/decodes by translating to/from the standard form.
var urlsafeencode = Function(data) -> String:
    return encode(data).replace("+", "-").replace("/", "_")

var urlsafedecode = Function(s):
    return decode(s.replace("-", "+").replace("_", "/"))
)KI";

// --- csv (simple, RFC-style quoting) -----------------------------------------------------------
inline constexpr std::string_view csv = R"KI(
var _needsQuote = Function(field) -> Bool:
    return "," in field or "\"" in field or "\n" in field

var formatrow = Function(fields) -> String:
    var parts = []
    for f in fields:
        var s = String(f)
        if _needsQuote(s):
            s = "\"" + s.replace("\"", "\"\"") + "\""
        parts.append(s)
    return ",".join(parts)

var parserow = Function(line):
    var fields = []
    var current = ""
    var inQuotes = False
    var i = 0
    var n = len(line)
    while i < n:
        var c = line[i]
        if inQuotes:
            if c == "\"":
                if i + 1 < n and line[i + 1] == "\"":
                    current = current + "\""
                    i = i + 1
                else:
                    inQuotes = False
            else:
                current = current + c
        elif c == "\"":
            inQuotes = True
        elif c == ",":
            fields.append(current)
            current = ""
        else:
            current = current + c
        i = i + 1
    fields.append(current)
    return fields

var format = Function(rows) -> String:
    var lines = []
    for row in rows:
        lines.append(formatrow(row))
    return "\n".join(lines)

var parse = Function(text):
    var rows = []
    for line in text.split("\n"):
        if len(line) > 0:
            rows.append(parserow(line))
    return rows
)KI";

// --- heapq (binary min-heap over a List) -------------------------------------------------------
inline constexpr std::string_view heapq = R"KI(
var _siftup = Function(heap, pos):
    var i = pos
    while i > 0:
        var parent = (i - 1) // 2
        if heap[i] < heap[parent]:
            var tmp = heap[i]
            heap[i] = heap[parent]
            heap[parent] = tmp
            i = parent
        else:
            break

var _siftdown = Function(heap):
    var n = len(heap)
    var i = 0
    while True:
        var left = 2 * i + 1
        var right = 2 * i + 2
        var smallest = i
        if left < n and heap[left] < heap[smallest]:
            smallest = left
        if right < n and heap[right] < heap[smallest]:
            smallest = right
        if smallest == i:
            break
        var tmp = heap[i]
        heap[i] = heap[smallest]
        heap[smallest] = tmp
        i = smallest

var heappush = Function(heap, item):
    heap.append(item)
    _siftup(heap, len(heap) - 1)

var heappop = Function(heap):
    var n = len(heap)
    if n == 0:
        throw "heappop from empty heap"
    var top = heap[0]
    var last = heap.pop()
    if len(heap) > 0:
        heap[0] = last
        _siftdown(heap)
    return top

var heapify = Function(items):
    var heap = []
    for x in items:
        heappush(heap, x)
    return heap

var nsmallest = Function(n, items):
    var heap = heapify(items)
    var out = []
    while len(out) < n and len(heap) > 0:
        out.append(heappop(heap))
    return out

var nlargest = Function(n, items):
    var s = sorted(items, None, True)
    return s[0:n]

var heapreplace = Function(heap, item):
    # Pop the smallest then push item (more efficient than separate pop+push).
    if len(heap) == 0:
        throw "heapreplace on empty heap"
    var top = heap[0]
    heap[0] = item
    _siftdown(heap)
    return top

var merge = Function(lists):
    # Merge already-sorted lists into one sorted list.
    var heap = []
    for lst in lists:
        for x in lst:
            heappush(heap, x)
    var out = []
    while len(heap) > 0:
        out.append(heappop(heap))
    return out
)KI";

// --- bisect (binary search on a sorted List) ---------------------------------------------------
inline constexpr std::string_view bisect = R"KI(
var bisectleft = Function(a, x) -> Integer:
    var lo = 0
    var hi = len(a)
    while lo < hi:
        var mid = (lo + hi) // 2
        if a[mid] < x:
            lo = mid + 1
        else:
            hi = mid
    return lo

var bisectright = Function(a, x) -> Integer:
    var lo = 0
    var hi = len(a)
    while lo < hi:
        var mid = (lo + hi) // 2
        if x < a[mid]:
            hi = mid
        else:
            lo = mid + 1
    return lo

var insortleft = Function(a, x):
    a.insert(bisectleft(a, x), x)

var insortright = Function(a, x):
    a.insert(bisectright(a, x), x)
)KI";

// --- copy (shallow / deep) ---------------------------------------------------------------------
inline constexpr std::string_view copy_mod = R"KI(
var copy = Function(obj):
    if type(obj) == "List":
        return obj.copy()
    if type(obj) == "Dict":
        return obj.copy()
    if type(obj) == "Set":
        return obj.copy()
    return obj

var deepcopy = Function(obj):
    var t = type(obj)
    if t == "List":
        var out = []
        for x in obj:
            out.append(deepcopy(x))
        return out
    if t == "Dict":
        var out = {}
        for pair in obj.items():
            out[deepcopy(pair[0])] = deepcopy(pair[1])
        return out
    if t == "Set":
        var out = Set()
        for x in obj:
            out.add(deepcopy(x))
        return out
    return obj
)KI";

// --- enum (a tiny enum factory) ----------------------------------------------------------------
inline constexpr std::string_view enum_mod = R"KI(
class Enum:
    # Enum(["RED", "GREEN", "BLUE"]) -> members accessible as e.get("RED") -> 0, with names()/values().
    var _init_ = Function(self, names):
        self._byName = {}
        self._byValue = {}
        var i = 0
        for name in names:
            self._byName[name] = i
            self._byValue[i] = name
            i = i + 1
    var get = Function(self, name):
        if name not in self._byName:
            throw "no such enum member: " + name
        return self._byName[name]
    var nameof = Function(self, value):
        return self._byValue[value]
    var names = Function(self):
        return self._byName.keys()
    var values = Function(self):
        return self._byValue.keys()
    var _getitem_ = Function(self, name):
        return self.get(name)
    var _contains_ = Function(self, name):
        return name in self._byName
)KI";

// --- tee: fan-out streams — clone output (e.g. stdout) into extra streams; context-manager hooks --
inline constexpr std::string_view tee = R"KI(
# tee — fan-out streams. A Tee writes every chunk to one or more "copy" streams (e.g. a log file)
# *before* passing it to the primary stream (e.g. the original stdout) — so output is saved before
# being handed on. It implements the stream `write`/`writelines`/`flush` protocol, so it can be
# assigned to io.stdout / io.stderr, and it works as a context manager (flushes on exit).
#
# The convenience context managers `tee_stdout` / `tee_stderr` hook the std streams for you:
#
#   var io = import("io")
#   var tee = import("tee")
#   with io.open("session.log", "w") as f:
#       with tee.tee_stdout(f):
#           io.print("appears on the console AND in session.log")
#   # stdout is restored here

var _io = import("io")

# Flush a stream if it supports it (files / BytesIO / std streams do); ignore if it doesn't.
var _maybeFlush = Function(s):
    if s == None:
        return None
    try:
        s.flush()
    catch as e:
        pass
    return None

class Tee:
    # Tee(primary, copies=None): writes go to each copy first, then to `primary`. `copies` may be a
    # single stream or a List of streams. `primary` may be None to make a pure fan-out sink.
    var _init_ = Function(self, primary, copies = None):
        self.primary = primary
        var cs = []
        if copies != None:
            if isinstance(copies, "List"):
                for s in copies:
                    cs.append(s)
            else:
                cs.append(copies)
        self.copies = cs

    # All underlying streams, in write order (copies first, then the primary).
    var streams = Function(self):
        var all = []
        for s in self.copies:
            all.append(s)
        if self.primary != None:
            all.append(self.primary)
        return all

    var write = Function(self, data):
        for s in self.copies:
            s.write(data)
        if self.primary != None:
            self.primary.write(data)
        return len(data)

    var writelines = Function(self, lines):
        for line in lines:
            self.write(line)
        return None

    var flush = Function(self):
        for s in self.copies:
            _maybeFlush(s)
        _maybeFlush(self.primary)
        return None

    var close = Function(self):
        self.flush()
        return None

    var _enter_ = Function(self):
        return self

    var _exit_ = Function(self):
        self.flush()
        return None

# Context manager: within the block, io.stdout also writes to `copies` (a stream or List). The
# original stdout is saved on enter and restored on exit (the copy streams are never closed —
# the caller owns them).
class tee_stdout:
    var _init_ = Function(self, copies):
        self._copies = copies
        self._saved = None

    var _enter_ = Function(self):
        self._saved = _io.stdout
        _io.stdout = Tee(self._saved, self._copies)
        return _io.stdout

    var _exit_ = Function(self):
        _io.stdout.flush()
        _io.stdout = self._saved
        return None

# Same, for io.stderr.
class tee_stderr:
    var _init_ = Function(self, copies):
        self._copies = copies
        self._saved = None

    var _enter_ = Function(self):
        self._saved = _io.stderr
        _io.stderr = Tee(self._saved, self._copies)
        return _io.stderr

    var _exit_ = Function(self):
        _io.stderr.flush()
        _io.stderr = self._saved
        return None
)KI";

}  // namespace kirito::kimod

#endif
