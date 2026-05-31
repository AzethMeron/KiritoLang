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
