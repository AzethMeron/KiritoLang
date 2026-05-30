#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;

    // construction, attributes, and a method using self
    CHECK(evalStr(vm, R"(
class Point:
    var init = Function(self, x, y):
        self.x = x
        self.y = y
    var sum = Function(self):
        return self.x + self.y
var p = Point(3, 4)
p.sum()
)") == "7");

    // attribute read and write
    CHECK(evalStr(vm, R"(
class Box:
    var init = Function(self, v):
        self.v = v
var b = Box(10)
b.v = b.v + 5
b.v
)") == "15");

    // stateful methods mutating self across calls
    CHECK(evalStr(vm, R"(
class Counter:
    var init = Function(self):
        self.n = 0
    var inc = Function(self):
        self.n = self.n + 1
        return self.n
var c = Counter()
c.inc()
c.inc()
c.inc()
)") == "3");

    // inheritance: Dog overrides speak but inherits Animal.init
    CHECK(evalStr(vm, R"(
class Animal:
    var init = Function(self, name):
        self.name = name
    var speak = Function(self):
        return "..."
class Dog(Animal):
    var speak = Function(self):
        return "woof"
var d = Dog("Rex")
d.name + " says " + d.speak()
)") == "Rex says woof");

    // str of an instance
    CHECK(evalStr(vm, R"(
class Widget:
    var init = Function(self):
        self.x = 1
String(Widget())
)") == "<Widget object>");

    // typed except: catch by class, bind the instance, read its attribute
    CHECK(evalStr(vm, R"(
class MyError:
    var init = Function(self, msg):
        self.msg = msg
var r = ""
try:
    raise MyError("oops")
except MyError as e:
    r = e.msg
r
)") == "oops");

    // typed except selects the matching handler
    CHECK(evalStr(vm, R"(
class A:
    var init = Function(self):
        self.k = "a"
class B:
    var init = Function(self):
        self.k = "b"
var caught = "none"
try:
    raise A()
except B as e:
    caught = "B"
except A as e:
    caught = "A"
caught
)") == "A");

    // typed except matches a base class for a derived instance
    CHECK(evalStr(vm, R"(
class Base:
    var init = Function(self):
        self.x = 1
class Derived(Base):
    var init = Function(self):
        self.x = 2
var r = "none"
try:
    raise Derived()
except Base as e:
    r = "base"
r
)") == "base");

    // a wrong-type typed except does NOT catch (propagates)
    CHECK_THROWS(vm.runSource(R"(
class A:
    var init = Function(self):
        self.x = 1
class B:
    var init = Function(self):
        self.x = 2
try:
    raise A()
except B as e:
    var ignore = 1
)"));

    // instances survive heavy GC (method binding + attributes intact)
    {
        KiritoVM g;
        g.setGcThreshold(300);
        CHECK(g.stringify(g.runSource(R"(
class Acc:
    var init = Function(self):
        self.total = 0
    var add = Function(self, x):
        self.total = self.total + x
        return self.total
var a = Acc()
var i = 0
while i < 5000:
    a.add(1)
    var junk = [i, i, i]
    i = i + 1
a.total
)")) == "5000");
    }

    return RUN_TESTS();
}
