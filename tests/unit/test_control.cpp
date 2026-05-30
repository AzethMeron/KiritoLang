#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;

    // while loop
    CHECK(evalStr(vm, R"(
var a = 0
while a < 5:
    a = a + 1
a
)") == "5");

    // if / elif / else picks the right branch
    CHECK(evalStr(vm, R"(
var x = 2
var r = 0
if x == 1:
    r = 10
elif x == 2:
    r = 20
else:
    r = 30
r
)") == "20");

    // break exits the loop early
    CHECK(evalStr(vm, R"(
var i = 0
var s = 0
while True:
    i = i + 1
    if i > 3:
        break
    s = s + i
s
)") == "6");

    // continue skips the rest of the iteration
    CHECK(evalStr(vm, R"(
var i = 0
var s = 0
while i < 5:
    i = i + 1
    if i == 3:
        continue
    s = s + i
s
)") == "12");

    // and/or short-circuit: the right operand is not evaluated, and the deciding operand is returned
    CHECK(evalStr(vm, "True or undefined_name") == "True");
    CHECK(evalStr(vm, "False and undefined_name") == "False");
    CHECK(evalStr(vm, "1 and 2") == "2");
    CHECK(evalStr(vm, "0 or 3") == "3");

    // not
    CHECK(evalStr(vm, "not 0") == "True");
    CHECK(evalStr(vm, "not 1") == "False");
    CHECK(evalStr(vm, "not \"\"") == "True");

    return RUN_TESTS();
}
