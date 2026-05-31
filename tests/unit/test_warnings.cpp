// Static-analysis warnings: unused variables, ignored non-None results, and the `discard` keyword.
#include <string>
#include <vector>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

// Analyze a source string and return the warning messages (without file:line:col prefixes).
static std::vector<std::string> warn(const std::string& src) {
    Parser parser(Lexer(src).tokenize());
    ast::Program program = parser.parseProgram();
    Analyzer analyzer;
    std::vector<std::string> msgs;
    for (const auto& w : analyzer.analyze(program)) msgs.push_back(w.message);
    return msgs;
}
static bool has(const std::vector<std::string>& v, const std::string& needle) {
    for (const auto& s : v) if (s.find(needle) != std::string::npos) return true;
    return false;
}

int main() {
    // --- unused local variable (inside a function) ---
    {
        auto w = warn("var f = Function():\n    var unused = 5\n    return 1\n");
        CHECK(has(w, "variable 'unused' is assigned but never used"));
    }
    // a used local does not warn
    {
        auto w = warn("var f = Function():\n    var x = 5\n    return x\n");
        CHECK(!has(w, "never used"));
    }
    // module-level names are exports, never flagged
    {
        auto w = warn("var exported = 5\n");
        CHECK(!has(w, "never used"));
    }
    // class members are never flagged
    {
        auto w = warn("class C:\n    var method = Function(self):\n        return 1\n");
        CHECK(!has(w, "never used"));
    }
    // parameters are never flagged as unused
    {
        auto w = warn("var f = Function(a, b):\n    return a\n");
        CHECK(!has(w, "never used"));
    }
    // private _names are never flagged
    {
        auto w = warn("var f = Function():\n    var _scratch = 5\n    return 1\n");
        CHECK(!has(w, "never used"));
    }

    // --- ignored non-None result ---
    {
        auto w = warn("var x = 1\nx + 2\n");  // bare arithmetic
        CHECK(has(w, "result of expression is unused"));
    }
    {
        auto w = warn("var a = [1, 2, 3]\na[0]\n");  // bare index
        CHECK(has(w, "result of expression is unused"));
    }
    {
        auto w = warn("var compute = Function() -> Integer:\n    return 5\ncompute()\n");
        CHECK(has(w, "result of expression is unused"));
    }
    // a None-returning function call is NOT flagged
    {
        auto w = warn("var act = Function() -> None:\n    return None\nact()\n");
        CHECK(!has(w, "result of expression is unused"));
    }
    // an unannotated function whose last statement is a bare return is treated as None-returning
    {
        auto w = warn("var act = Function():\n    var x = 1\n    return\nact()\n");
        CHECK(!has(w, "result of expression is unused"));
    }
    // calls to unknown/native functions are left alone (we can't know their return type)
    {
        auto w = warn("var io = import(\"io\")\nio.print(\"hi\")\n");
        CHECK(!has(w, "result of expression is unused"));
    }

    // --- discard suppresses the unused-result warning ---
    {
        auto w = warn("var compute = Function() -> Integer:\n    return 5\ndiscard compute()\n");
        CHECK(!has(w, "result of expression is unused"));
    }
    {
        auto w = warn("var x = 1\ndiscard x + 2\n");
        CHECK(!has(w, "result of expression is unused"));
    }

    // --- discard still runs the expression (side effects preserved) ---
    {
        KiritoVM vm;
        CHECK(vm.stringify(vm.runSource(
            "var io = import(\"io\")\nvar log = []\nvar f = Function():\n    log.append(1)\n    return 9\n"
            "discard f()\nlen(log)")) == "1");
    }

    return RUN_TESTS();
}
