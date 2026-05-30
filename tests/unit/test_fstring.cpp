#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;

    CHECK(evalStr(vm, "var n = 7\nf\"n is {n}\"") == "n is 7");
    CHECK(evalStr(vm, "var a = 3\nvar b = 4\nf\"{a} + {b} = {a + b}\"") == "3 + 4 = 7");
    CHECK(evalStr(vm, "f\"{{escaped}} {1 + 1}\"") == "{escaped} 2");
    CHECK(evalStr(vm, "var name = \"World\"\nf\"Hello, {name}!\"") == "Hello, World!");
    CHECK(evalStr(vm, "f\"{[1, 2, 3]}\"") == "[1, 2, 3]");
    CHECK(evalStr(vm, "f\"no placeholders\"") == "no placeholders");
    CHECK(evalStr(vm, "var x = 10\nf\"{x // 3} remainder {x % 3}\"") == "3 remainder 1");
    CHECK(evalStr(vm, "var p = [5, 9]\nf\"sum is {p[0] + p[1]}\"") == "sum is 14");

    return RUN_TESTS();
}
