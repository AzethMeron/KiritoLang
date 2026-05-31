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

    // format specs (f"{x:05d}") — a `:` after the expression applies a mini-format-spec
    CHECK(evalStr(vm, "var x = 42\nf\"{x:05d}\"") == "00042");
    CHECK(evalStr(vm, "var pi = 3.14159\nf\"{pi:.2f}\"") == "3.14");
    CHECK(evalStr(vm, "var x = 42\nf\"{x:#x}\"") == "0x2a");
    CHECK(evalStr(vm, "var x = 42\nf\"{x:>5}|\"") == "   42|");
    CHECK(evalStr(vm, "f\"{1000000:,}\"") == "1,000,000");
    CHECK(evalStr(vm, "f\"{[1, 2, 3][1]:03d}\"") == "002");
    // a `:` inside a slice or dict literal is NOT a spec separator
    CHECK(evalStr(vm, "var s = \"abcde\"\nf\"{s[1:3]}\"") == "bc");
    CHECK(evalStr(vm, "f\"{ {7: 8}[7] }\"") == "8");
    // surrounding whitespace inside the braces is allowed (f"{ x }")
    CHECK(evalStr(vm, "var x = 5\nf\"{ x }\"") == "5");
    CHECK(evalStr(vm, "var x = 5\nf\"a{  x  }b\"") == "a5b");

    return RUN_TESTS();
}
