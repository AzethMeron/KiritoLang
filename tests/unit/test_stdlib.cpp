#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;

    // --- math module ---
    CHECK(evalStr(vm, "import(\"math\").sqrt(16)") == "4.0");
    CHECK(evalStr(vm, "var m = import(\"math\")\nm.sqrt(9)") == "3.0");
    CHECK(evalStr(vm, "import(\"math\").floor(3.7)") == "3");
    CHECK(evalStr(vm, "import(\"math\").ceil(3.2)") == "4");
    CHECK(evalStr(vm, "import(\"math\").factorial(5)") == "120");
    CHECK(evalStr(vm, "import(\"math\").gcd(12, 8)") == "4");
    CHECK(evalStr(vm, "import(\"math\").pow(2, 10)") == "1024.0");
    CHECK(evalStr(vm, "import(\"math\").log2(8)") == "3.0");
    CHECK(evalStr(vm, "var m = import(\"math\")\nm.pi > 3.14 and m.pi < 3.15") == "True");
    CHECK(evalStr(vm, "var m = import(\"math\")\nm.gcd(54, 24)") == "6");

    // --- file io: write then read back ---
    CHECK(evalStr(vm, R"(
var io = import("io")
var f = io.open("/tmp/kirito_stdlib_test.txt", "w")
f.write("hello\n")
f.write("world\n")
f.close()
var g = io.open("/tmp/kirito_stdlib_test.txt", "r")
var content = g.read()
g.close()
content
)") == "hello\nworld\n");

    // --- file as a context manager (auto-close on exit) ---
    CHECK(evalStr(vm, R"(
var io = import("io")
with io.open("/tmp/kirito_stdlib_test2.txt", "w") as f:
    f.write("data 123")
var c = ""
with io.open("/tmp/kirito_stdlib_test2.txt", "r") as g:
    c = g.read()
c
)") == "data 123");

    // --- readline ---
    CHECK(evalStr(vm, R"(
var io = import("io")
var f = io.open("/tmp/kirito_stdlib_test3.txt", "w")
f.write("line1\nline2\n")
f.close()
var g = io.open("/tmp/kirito_stdlib_test3.txt", "r")
var first = g.readline()
g.close()
first
)") == "line1");

    return RUN_TESTS();
}
