// Differential + behavioural tests for the bytecode execution engine. The contract is parity with
// the tree-walking evaluator: every program must produce an identical result on both engines. We run
// each snippet on a fresh tree-walker VM and a fresh bytecode VM and compare the stringified result,
// then pin specific values and stress the GC integration of the operand stack.
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

// Run a source chunk on a fresh VM with the chosen engine; return the stringified last value.
static std::string run(const std::string& src, bool bytecode) {
    KiritoVM vm;
    vm.setBytecode(bytecode);  // explicit, so it ignores any KIRITO_BYTECODE in the environment
    Handle h = vm.runSource(src);
    return vm.stringify(h);
}

// Assert the two engines agree, and return the (shared) result so callers can also pin it.
static std::string same(const std::string& src) {
    std::string tree = run(src, false);
    std::string byte = run(src, true);
    if (tree != byte)
        std::printf("FAIL divergence: tree=[%s] byte=[%s]\n  src: %s\n", tree.c_str(), byte.c_str(), src.c_str());
    CHECK(tree == byte);
    return byte;
}

int main() {
    // --- arithmetic, precedence, division kinds, wraparound ---
    CHECK(same("1 + 2 * 3") == "7");
    CHECK(same("2 ** 3 ** 2") == "512");      // right-assoc
    CHECK(same("-2 ** 2") == "-4");           // ** binds tighter than unary minus
    CHECK(same("7 // 2") == "3");
    CHECK(same("-7 // 2") == "-4");           // floor division
    CHECK(same("-7 % 3") == "2");             // modulo sign follows divisor
    CHECK(same("4 / 2") == "2.0");            // / always Float
    CHECK(same("1 / 2") == "0.5");
    same("2 + 3 * 4 - 10 / 5 ** 2");

    // --- comparisons, equality across types, logical, conditional ---
    CHECK(same("1 < 2 and 2 <= 2") == "True");
    CHECK(same("3 > 4 or not False") == "True");
    CHECK(same("1 == \"x\"") == "False");     // mismatched-type equality never raises
    CHECK(same("1 != 2") == "True");
    CHECK(same("\"hi\" if 3 > 2 else \"lo\"") == "hi");
    same("True and 0 or \"fallback\"");        // short-circuit yields the deciding operand
    same("None or 5");
    same("0 and crashIfEvaluated");            // RHS not evaluated -> no NameError

    // --- strings, f-strings, format specs ---
    CHECK(same("\"AB\" * 3 + \"!\"") == "ABABAB!");
    same("var n = \"Kirito\"\nvar p = 3.14159\nf\"{n}:{p:.2f}:{255:#x}:{42:05d}\"");
    CHECK(same("\"Hello\".lower().upper()") == "HELLO");
    same("\"a,b,c\".split(\",\")");

    // --- var / assignment / rebinding / augmented-style ---
    CHECK(same("var x = 10\nx = x + 5\nx") == "15");
    same("var a = 1\nvar b = 2\na = a + b\nb = a * 2\n[a, b]");

    // --- collections: list/set/dict literals, indexing, slicing, membership, methods ---
    CHECK(same("var xs = [1,2,3,4]\nxs.append(9)\nxs") == "[1, 2, 3, 4, 9]");
    CHECK(same("[1,2,3,4,5][1:4]") == "[2, 3, 4]");
    CHECK(same("[1,2,3,4,5][::2]") == "[1, 3, 5]");
    CHECK(same("var d = {\"a\":1}\nd[\"b\"] = 2\nd[\"a\"] + d[\"b\"]") == "3");
    CHECK(same("\"b\" in {\"a\":1,\"b\":2}") == "True");
    CHECK(same("3 in {1,2,3}") == "True");
    same("{1,2,2,3,3,3}");                      // set dedup
    same("var m = {}\nm[1] = \"one\"\nm[2] = \"two\"\nm");

    // --- control flow: while/for, break/continue, nested ---
    CHECK(same("var i=0\nvar t=0\nwhile i<10:\n    i=i+1\n    if i==3:\n        continue\n    if i==7:\n        break\n    t=t+i\nt") == "18");  // 1+2+4+5+6
    CHECK(same("var s=0\nfor n in range(100):\n    s=s+n\ns") == "4950");
    CHECK(same("var out=[]\nfor ch in \"abc\":\n    out.append(ch)\nout") == "[a, b, c]");
    // nested loops with break in the inner loop only
    same("var c=0\nfor i in range(4):\n    for j in range(4):\n        if j==2:\n            break\n        c=c+1\nc");

    // --- functions: closures, defaults, recursion via the bytecode engine ---
    CHECK(same("var sq = Function(v): return v*v\nsq(7)") == "49");
    CHECK(same("var add = Function(a, b=100): return a+b\n[add(1), add(1,2)]") == "[101, 3]");
    // recursion: factorial (each call compiles+runs the body on the bytecode engine)
    CHECK(same("var fac = Function(n):\n    if n <= 1:\n        return 1\n    return n * fac(n-1)\nfac(6)") == "720");
    // a closure capturing an enclosing variable
    same("var make = Function(base):\n    return Function(x): return x + base\nvar f = make(10)\nf(5)");
    // keyword arguments at the call site
    CHECK(same("var f = Function(a, b, c): return a*100 + b*10 + c\nf(c=3, a=1, b=2)") == "123");

    // --- builtins that take callables / iterables (exercise calls inside bytecode) ---
    same("sum([1,2,3,4,5])");
    same("sorted([3,1,2])");
    same("len(\"hello\")");
    same("max(3, 7, 2)");
    same("List(range(3))");                     // a builtin constructor over an iterable
    same("sum(range(10))");

    // --- bare-comma packing -> List, and indexing the result ---
    CHECK(same("var t = 1, 2, 3\nt") == "[1, 2, 3]");

    // --- assert that passes (no throw) leaves the program running ---
    CHECK(same("assert 1+1 == 2\n42") == "42");

    // === behavioural pins (bytecode engine specifically) ===
    CHECK(run("var z = 0\nfor i in range(1000):\n    z = z + i\nz", true) == "499500");
    CHECK(run("Function(n):\n    var s = 0\n    var k = 1\n    while k <= n:\n        s = s + k\n        k = k + 1\n    return s\n", true).rfind("<function", 0) == 0);

    // --- GC integration: many allocations under a tiny threshold must not reap live operand-stack
    // values; the aux-root region keeps them alive. Result must still be exact. ---
    {
        KiritoVM vm;
        vm.setBytecode(true);
        vm.setGcThreshold(64);  // force frequent collections during the loop
        Handle h = vm.runSource(
            "var total = 0\n"
            "for i in range(500):\n"
            "    var pair = [i, i*2]\n"          // fresh allocation every iteration
            "    var d = {\"k\": pair}\n"
            "    total = total + d[\"k\"][1]\n"
            "total");
        CHECK(vm.arena().deref(h).kind() == ValueKind::Integer);
        CHECK(static_cast<const IntVal&>(vm.arena().deref(h)).value() == 249500);  // sum of 2*i, i in 0..499
    }

    // --- a runtime error inside a bytecode body is raised with the same effect as the tree-walker ---
    {
        KiritoVM tw; tw.setBytecode(false);
        KiritoVM bc; bc.setBytecode(true);
        CHECK_THROWS(tw.runSource("undefined_name + 1"));
        CHECK_THROWS(bc.runSource("undefined_name + 1"));
        CHECK_THROWS(bc.runSource("var d = {}\nd[\"missing\"]"));
        CHECK_THROWS(bc.runSource("[1,2,3] + 5"));  // unsupported operand types
    }

    // --- an uncaught throw/assert from the bytecode top level surfaces as an error ---
    {
        KiritoVM bc; bc.setBytecode(true);
        CHECK_THROWS(bc.runSource("assert 1 == 2"));
        CHECK_THROWS(bc.runSource("throw \"boom\""));
    }

    return RUN_TESTS();
}
