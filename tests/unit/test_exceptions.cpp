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

    // re-raise from an except handler propagates to the outer try
    CHECK(evalStr(vm, R"(
var r = "none"
try:
    try:
        raise "x"
    except as e:
        raise "wrapped:" + e
except as e2:
    r = e2
r
)") == "wrapped:x");

    // an exception thrown inside an except handler is not swallowed by the same try
    CHECK_THROWS(vm.runSource(R"(
try:
    raise "a"
except as e:
    raise "b"
)"));

    // break inside try still runs finally before leaving the loop
    CHECK(evalStr(vm, R"(
var log = []
var i = 0
while i < 5:
    try:
        if i == 2:
            break
        log.append(i)
    finally:
        log.append("f")
    i = i + 1
log
)") == "[0, f, 1, f, f]");

    // continue inside try runs finally each iteration
    CHECK(evalStr(vm, R"(
var n = 0
var i = 0
while i < 3:
    i = i + 1
    try:
        continue
    finally:
        n = n + 1
n
)") == "3");

    // try with no exception: except is skipped, value is the try body's
    CHECK(evalStr(vm, R"(
try:
    var x = 10
    x + 5
except as e:
    -1
)") == "15");

    // exception thrown while building a value mid-expression is caught cleanly
    CHECK(evalStr(vm, R"(
var bad = Function():
    raise "mid"
var r = "none"
try:
    var x = 1 + bad()
except as e:
    r = e
r
)") == "mid");

    // deeply nested try/finally chain unwinds in the right order
    CHECK(evalStr(vm, R"(
var log = []
try:
    try:
        try:
            raise "deep"
        finally:
            log.append("f3")
    finally:
        log.append("f2")
except as e:
    log.append("caught")
finally:
    log.append("f1")
log
)") == "[f3, f2, caught, f1]");

    // matching by class hierarchy: a derived class is caught by a base handler, but not vice versa
    CHECK(evalStr(vm, R"(
class Animal:
    var _init_ = Function(self):
        self.x = 1
class Dog(Animal):
    var _init_ = Function(self):
        self.x = 2
var r = "none"
try:
    raise Dog()
except Animal as e:
    r = "caught-as-animal"
r
)") == "caught-as-animal");

    // assert raises a catchable exception with its message
    CHECK(evalStr(vm, R"(
var r = "none"
try:
    assert 1 == 2, "math broke"
except as e:
    r = e
r
)") == "math broke");

    // index/key errors are catchable
    CHECK(evalStr(vm, R"(
var r = "none"
try:
    var a = [1, 2, 3]
    var x = a[99]
except as e:
    r = "index-error"
r
)") == "index-error");

    // exception state does not leak: after catching, the VM continues normally
    CHECK(evalStr(vm, R"(
var total = 0
var i = 0
while i < 10:
    try:
        if i % 2 == 0:
            raise "even"
        total = total + i
    except as e:
        total = total + 100
    i = i + 1
total
)") == "525");

    return RUN_TESTS();
}
