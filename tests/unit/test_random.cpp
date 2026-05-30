#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;

    // reproducibility: same seed -> same sequence (no global RNG; each object is independent)
    CHECK(evalStr(vm, R"(
var random = import("random")
var a = random.Random(123)
var b = random.Random(123)
var same = True
var i = 0
while i < 20:
    if a.randint(1, 1000000) != b.randint(1, 1000000):
        same = False
    i = i + 1
same
)") == "True");

    // randint stays within the inclusive range
    CHECK(evalStr(vm, R"(
var rng = import("random").Random(7)
var ok = True
var i = 0
while i < 1000:
    var x = rng.randint(1, 6)
    if x < 1 or x > 6:
        ok = False
    i = i + 1
ok
)") == "True");

    // random() in [0, 1)
    CHECK(evalStr(vm, R"(
var rng = import("random").Random(1)
var ok = True
var i = 0
while i < 1000:
    var x = rng.random()
    if x < 0.0 or x >= 1.0:
        ok = False
    i = i + 1
ok
)") == "True");

    // choice returns a member; shuffle preserves length; sample gives the right count
    CHECK(evalStr(vm, R"(
var rng = import("random").Random(99)
var data = [10, 20, 30, 40, 50]
rng.choice(data) in data
)") == "True");
    CHECK(evalStr(vm, R"(
var rng = import("random").Random(99)
var data = [1, 2, 3, 4, 5, 6, 7, 8]
rng.shuffle(data)
len(data)
)") == "8");
    CHECK(evalStr(vm, R"(
var rng = import("random").Random(99)
len(rng.sample([1, 2, 3, 4, 5], 3))
)") == "3");

    // uniform stays in range
    CHECK(evalStr(vm, R"(
var rng = import("random").Random(5)
var x = rng.uniform(2.0, 3.0)
x >= 2.0 and x <= 3.0
)") == "True");

    // empty-sequence and bad-range errors
    CHECK_THROWS(vm.runSource("import(\"random\").Random(1).choice([])\n"));
    CHECK_THROWS(vm.runSource("import(\"random\").Random(1).randint(5, 1)\n"));

    return RUN_TESTS();
}
