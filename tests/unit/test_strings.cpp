#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;

    // length and indexing (with negative indices)
    CHECK(evalStr(vm, "len(\"hello\")") == "5");
    CHECK(evalStr(vm, "\"abc\"[0]") == "a");
    CHECK(evalStr(vm, "\"abc\"[-1]") == "c");
    CHECK_THROWS(vm.runSource("\"abc\"[9]"));

    // iteration over characters
    CHECK(evalStr(vm, R"(
var s = ""
for c in "abc":
    s = s + c
s
)") == "abc");
    CHECK(evalStr(vm, R"(
var n = 0
for c in "hello":
    n = n + 1
n
)") == "5");

    // REPL keeps bindings across calls (persistent scope)
    {
        KiritoVM r;
        r.runRepl("var x = 1");
        r.runRepl("x = x + 1");
        CHECK(r.stringify(r.runRepl("x")) == "2");
    }

    return RUN_TESTS();
}
