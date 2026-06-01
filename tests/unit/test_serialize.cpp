#include <filesystem>
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

// round-trip a value through dumps/loads and stringify the result
static std::string roundTrip(KiritoVM& vm, const std::string& expr) {
    return evalStr(vm, "var s = import(\"serialize\")\ns.loads(s.dumps(" + expr + "))");
}

int main() {
    KiritoVM vm;

    // scalars
    CHECK(roundTrip(vm, "None") == "None");
    CHECK(roundTrip(vm, "True") == "True");
    CHECK(roundTrip(vm, "42") == "42");
    CHECK(roundTrip(vm, "-17") == "-17");
    CHECK(roundTrip(vm, "3.14159") == "3.14159");
    CHECK(roundTrip(vm, "\"héllo \\n world\"") == "héllo \n world");

    // containers
    CHECK(roundTrip(vm, "[1, 2, 3]") == "[1, 2, 3]");
    CHECK(roundTrip(vm, "[[1, 2], [3, [4, 5]]]") == "[[1, 2], [3, [4, 5]]]");
    CHECK(evalStr(vm, "var s = import(\"serialize\")\nlen(s.loads(s.dumps({1, 2, 3})))") == "3");
    CHECK(roundTrip(vm, "{\"a\": [1, 2], \"b\": \"x\"}") != "");  // (key order varies; checked below)
    CHECK(evalStr(vm, "var s = import(\"serialize\")\ns.loads(s.dumps({\"a\": [1, 2], \"b\": 3}))[\"a\"][1]") == "2");

    // structural equality after round-trip
    CHECK(evalStr(vm, R"(
var s = import("serialize")
var x = [1, [2, 3], {"k": 4}]
s.loads(s.dumps(x)) == x
)") == "True");

    // SHARED references are preserved: A appears twice -> one object after load
    CHECK(evalStr(vm, R"(
var s = import("serialize")
var A = [1]
var x = [A, A, [9]]
var y = s.loads(s.dumps(x))
y[0].append(2)
y[1]
)") == "[1, 2]");  // mutating y[0] is visible through y[1] (same object)
    CHECK(evalStr(vm, R"(
var s = import("serialize")
var A = [1]
var y = s.loads(s.dumps([A, A]))
y[0] == y[1]
)") == "True");

    // CYCLES round-trip and remain self-referential
    CHECK(evalStr(vm, R"(
var s = import("serialize")
var c = []
c.append(c)
var d = s.loads(s.dumps(c))
d[0] == d
)") == "True");

    // save / load to a file
    CHECK(evalStr(vm, R"(
var s = import("serialize")
var p = import("io").gettempdir() + "/kirito_serialize_test.dat"
var data = {"name": "Kirito", "scores": [10, 20, 30]}
s.save(data, p)
var loaded = s.load(p)
String(loaded["name"]) + ":" + String(loaded["scores"][2])
)") == "Kirito:30");
    std::filesystem::remove(std::filesystem::temp_directory_path() / "kirito_serialize_test.dat");

    // unsupported types raise
    CHECK_THROWS(vm.runSource("var s = import(\"serialize\")\ns.dumps(Function(x): return x)\n"));

    return RUN_TESTS();
}
