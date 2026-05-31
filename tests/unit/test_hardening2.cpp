// Hardening round 2: resource-exhaustion guards (huge repetition/padding/range raise instead of
// OOMing the host) and correct source spans on uncaught throw/assert exceptions.
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string run(KiritoVM& vm, const std::string& src) { return vm.stringify(vm.runSource(src)); }
static bool raises(KiritoVM& vm, const std::string& src) {
    try { vm.runSource(src); return false; } catch (const KiritoError&) { return true; }
}
static std::string errOf(KiritoVM& vm, const std::string& src) {
    try { vm.runSource(src); return ""; } catch (const KiritoError& e) { return e.what(); }
}

int main() {
    // --- resource-exhaustion guards: absurd sizes raise, legitimate sizes work ---
    {
        KiritoVM vm;
        CHECK(raises(vm, "\"ab\" * 100000000000"));         // ~200 GB string
        CHECK(raises(vm, "\"x\" * 9223372036854775807"));    // INT64_MAX
        CHECK(raises(vm, "\"x\".ljust(10000000000)"));
        CHECK(raises(vm, "\"x\".rjust(10000000000)"));
        CHECK(raises(vm, "\"x\".center(10000000000)"));
        CHECK(raises(vm, "\"5\".zfill(10000000000)"));
        CHECK(raises(vm, "format(5, \"9999999999d\")"));
        CHECK(raises(vm, "range(10000000000)"));
        CHECK(raises(vm, "range(0, -10000000000, -1)"));
        CHECK(errOf(vm, "\"ab\" * 100000000000").find("too large") != std::string::npos);
        CHECK(errOf(vm, "range(10000000000)").find("range too large") != std::string::npos);
        // legitimate sizes still work
        CHECK(run(vm, "len(\"ab\" * 1000)") == "2000");
        CHECK(run(vm, "len(range(1000))") == "1000");
        CHECK(run(vm, "\"x\".ljust(5)") == "x    ");
        CHECK(run(vm, "format(5, \"08d\")") == "00000005");
        CHECK(run(vm, "\"ab\" * 0") == "");
        CHECK(run(vm, "\"ab\" * -5") == "");
    }

    // --- deep nesting raises (does not overflow the C++ stack) across every recursive operation ---
    {
        KiritoVM vm;
        // build a 50000-deep acyclic list: a = [a_prev]
        const char* build =
            "var a = [1]\nvar i = 0\nwhile i < 50000:\n    a = [a]\n    i = i + 1\n";
        CHECK(raises(vm, std::string(build) + "String(a)\n"));                       // stringify
        CHECK(raises(vm, std::string(build) + "import(\"json\").dumps(a)\n"));        // json write
        CHECK(raises(vm, std::string(build) + "import(\"serialize\").dumps(a)\n"));   // text serialize
        CHECK(raises(vm, std::string(build) + "import(\"dump\").dumps(a)\n"));        // binary dump
        // deeply nested JSON text must not overflow the parser
        std::string deep(20000, '[');
        deep += std::string(20000, ']');
        vm.registerGlobal("_deep", vm.makeString(deep));
        CHECK(raises(vm, "import(\"json\").parse(_deep)"));
        CHECK(errOf(vm, "import(\"json\").parse(_deep)").find("too deep") != std::string::npos);
        // a reasonably nested structure still works
        CHECK(run(vm, "String([[1, [2, [3]]]])") == "[[1, [2, [3]]]]");
        CHECK(run(vm, "import(\"json\").parse(\"[[1],[2,[3]]]\")[1][1][0]") == "3");
    }

    // --- pathologically deep SOURCE must raise (parser/analyzer/evaluator), not crash ---
    {
        KiritoVM vm;
        CHECK(raises(vm, "var x = " + std::string(5000, '(') + "1" + std::string(5000, ')')));   // parens
        CHECK(raises(vm, "var x = " + std::string(5000, '[') + std::string(5000, ']')));          // list literal
        CHECK(raises(vm, "var x = " + std::string(5000, '-') + "1"));                              // unary chain
        // a deep left-leaning binary tree parses iteratively but would overflow eval; it must raise
        std::string deepSum = "var x = 1";
        for (int i = 0; i < 8000; ++i) deepSum += " + 1";
        CHECK(raises(vm, deepSum + "\n"));
        // a deep `not` chain
        std::string notChain = "var x = ";
        for (int i = 0; i < 6000; ++i) notChain += "not ";
        notChain += "True\n";
        CHECK(raises(vm, notChain));
        // moderate nesting still works
        CHECK(run(vm, "((((1 + 2)) * 3))") == "9");
        CHECK(run(vm, "not not True") == "True");
    }

    // --- scientific-notation numeric literals lex as Float ---
    {
        KiritoVM vm;
        CHECK(run(vm, "String(1e3)") == "1000.0");
        CHECK(run(vm, "String(1.5e3)") == "1500.0");
        CHECK(run(vm, "String(2e-3)") == "0.002");
        CHECK(run(vm, "String(1E5)") == "100000.0");
        CHECK(run(vm, "type(1e10)") == "Float");
        CHECK(run(vm, "1e2 == 100.0") == "True");
        // a number followed by an `e...` identifier must NOT be swallowed
        CHECK(run(vm, "var elephant = 7\n3 + elephant") == "10");
    }

    // --- Float->Integer conversion of non-finite/out-of-range raises (was float-cast-overflow UB) ---
    {
        KiritoVM vm;
        CHECK(raises(vm, "var m = import(\"math\")\nInteger(m.inf)\n"));
        CHECK(raises(vm, "var m = import(\"math\")\nInteger(m.nan)\n"));
        CHECK(raises(vm, "Integer(1e300)"));
        CHECK(raises(vm, "var m = import(\"math\")\nm.floor(m.inf)\n"));
        CHECK(raises(vm, "var m = import(\"math\")\nm.ceil(1e300)\n"));
        CHECK(raises(vm, "var m = import(\"math\")\nround(m.inf)\n"));
        CHECK(raises(vm, "round(1e300)"));
        // in-range conversions still work
        CHECK(run(vm, "Integer(3.9)") == "3");
        CHECK(run(vm, "Integer(-3.9)") == "-3");
        CHECK(run(vm, "import(\"math\").floor(3.7)") == "3");
        CHECK(run(vm, "round(2.4)") == "2");
        CHECK(run(vm, "String(round(3.14159, 2))") == "3.14");
    }

    // --- abs(INT64_MIN) is well-defined (was UB via std::llabs) ---
    {
        KiritoVM vm;
        CHECK(run(vm, "abs(-5)") == "5");
        CHECK(run(vm, "abs(5)") == "5");
        CHECK(run(vm, "String(abs(-2.5))") == "2.5");
        // INT64_MIN wraps to itself (defined two's-complement), not UB
        CHECK(run(vm, "abs(-9223372036854775807 - 1)") == "-9223372036854775808");
    }

    // --- comb/perm: results that fit int64 don't spuriously overflow (incremental division) ---
    {
        KiritoVM vm;
        CHECK(run(vm, "import(\"math\").comb(30, 15)") == "155117520");
        CHECK(run(vm, "import(\"math\").comb(20, 10)") == "184756");
        CHECK(run(vm, "import(\"math\").comb(50, 25)") == "126410606437752");
        CHECK(run(vm, "import(\"math\").comb(62, 31)") == "465428353255261088");
        CHECK(run(vm, "import(\"math\").comb(10, 0)") == "1");
        CHECK(run(vm, "import(\"math\").comb(10, 10)") == "1");
        CHECK(run(vm, "import(\"math\").perm(10, 3)") == "720");
        // a result that genuinely exceeds int64 still raises
        CHECK(raises(vm, "import(\"math\").comb(70, 35)"));
    }

    // --- Integer()/Float() reject trailing garbage (no silent prefix parse) ---
    {
        KiritoVM vm;
        CHECK(raises(vm, "Integer(\"0x1F\")"));
        CHECK(raises(vm, "Integer(\"42abc\")"));
        CHECK(raises(vm, "Integer(\"12.5\")"));
        CHECK(raises(vm, "Integer(\"\")"));
        CHECK(raises(vm, "Integer(\"  \")"));
        CHECK(raises(vm, "Float(\"1.5x\")"));
        CHECK(raises(vm, "Float(\"abc\")"));
        // valid forms (with optional surrounding whitespace) still work
        CHECK(run(vm, "Integer(\"42\")") == "42");
        CHECK(run(vm, "Integer(\"  -7  \")") == "-7");
        CHECK(run(vm, "String(Float(\"1.5\"))") == "1.5");
        CHECK(run(vm, "String(Float(\"  2.0 \"))") == "2.0");
        CHECK(run(vm, "Integer(\"100\") + Float(\"0.5\") == 100.5") == "True");
    }

    // --- uncaught throw/assert carry the correct source span ---
    {
        KiritoVM vm;
        // line 1 col 1: throw at the top
        try { vm.runSource("throw \"boom\"\n"); CHECK(false); }
        catch (const KiritoError& e) {
            CHECK(std::string(e.what()).find("boom") != std::string::npos);
            CHECK(e.span.line == 1);
            CHECK(e.span.col == 1);
        }
        // throw on line 3
        try { vm.runSource("var a = 1\nvar b = 2\nthrow \"x\"\n"); CHECK(false); }
        catch (const KiritoError& e) { CHECK(e.span.line == 3); CHECK(e.span.col == 1); }
        // assert carries its span
        try { vm.runSource("assert 1 == 2, \"nope\"\n"); CHECK(false); }
        catch (const KiritoError& e) {
            CHECK(std::string(e.what()).find("nope") != std::string::npos);
            CHECK(e.span.line == 1);
        }
        // a throw deep inside a called function reports the throw site, not 0:0
        try { vm.runSource("var f = Function():\n    throw \"deep\"\nf()\n"); CHECK(false); }
        catch (const KiritoError& e) { CHECK(e.span.line == 2); CHECK(e.span.col == 5); }
        // a throw that propagates through a try/finally still reports the original site
        try { vm.runSource("try:\n    throw \"t\"\nfinally:\n    var x = 1\n"); CHECK(false); }
        catch (const KiritoError& e) { CHECK(e.span.line == 2); }
    }

    return RUN_TESTS();
}
