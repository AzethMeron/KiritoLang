#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;

    // raise + catch with binding
    CHECK(evalStr(vm, R"(
try:
    raise "boom"
except as e:
    e
)") == "boom");

    // internal runtime errors are catchable, surfaced as a String message
    CHECK(evalStr(vm, R"(
try:
    var x = 1 / 0
    x
except as e:
    "caught"
)") == "caught");
    CHECK(evalStr(vm, R"(
try:
    var x = undefined_var
except as e:
    e
)") == "name 'undefined_var' is not defined");

    // finally always runs (normal path)
    CHECK(evalStr(vm, R"(
var log = []
try:
    log.append("try")
finally:
    log.append("fin")
log
)") == "[try, fin]");

    // finally runs after a handled exception
    CHECK(evalStr(vm, R"(
var log = []
try:
    raise "e"
except:
    log.append("catch")
finally:
    log.append("fin")
log
)") == "[catch, fin]");

    // a return inside try still runs finally, then returns
    CHECK(evalStr(vm, R"(
var f = Function():
    try:
        return 1
    finally:
        var x = 2
f()
)") == "1");

    // a return inside finally overrides the try's return (Python semantics)
    CHECK(evalStr(vm, R"(
var f = Function():
    try:
        return 1
    finally:
        return 2
f()
)") == "2");

    // nested: inner finally runs, then the exception propagates to the outer handler
    CHECK(evalStr(vm, R"(
var r = 0
try:
    try:
        raise "inner"
    finally:
        r = 1
except as e:
    r = r + 10
r
)") == "11");

    // uncaught raise propagates to the embedder as an error
    CHECK_THROWS(vm.runSource("raise \"unhandled\"\n"));

    // raising a non-string value works too
    CHECK(evalStr(vm, R"(
try:
    raise 42
except as e:
    e + 1
)") == "43");

    return RUN_TESTS();
}
