#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;

    // parse: objects become Dicts (so a JSON object *is* a Dict)
    CHECK(evalStr(vm, "type(import(\"json\").parse(\"{\\\"a\\\": 1}\"))") == "Dict");
    CHECK(evalStr(vm, "import(\"json\").parse(\"{\\\"a\\\": 1}\")[\"a\"]") == "1");
    CHECK(evalStr(vm, "import(\"json\").parse(\"[1, 2, 3]\")[1]") == "2");

    // all scalar types
    CHECK(evalStr(vm, "import(\"json\").parse(\"true\")") == "True");
    CHECK(evalStr(vm, "import(\"json\").parse(\"false\")") == "False");
    CHECK(evalStr(vm, "import(\"json\").parse(\"null\")") == "None");
    CHECK(evalStr(vm, "import(\"json\").parse(\"42\")") == "42");
    CHECK(evalStr(vm, "import(\"json\").parse(\"3.5\")") == "3.5");
    CHECK(evalStr(vm, "import(\"json\").parse(\"-17\")") == "-17");
    CHECK(evalStr(vm, "import(\"json\").parse(\"\\\"hi\\\"\")") == "hi");

    // nested access
    CHECK(evalStr(vm, "import(\"json\").parse(\"[[1, 2], [3, 4]]\")[1][0]") == "3");
    CHECK(evalStr(vm, R"(
var j = import("json")
var d = j.parse("{\"user\": {\"name\": \"K\", \"tags\": [\"a\", \"b\"]}}")
d["user"]["tags"][1]
)") == "b");

    // the parsed object is a real Dict (mutable)
    CHECK(evalStr(vm, R"(
var d = import("json").parse("{\"x\": 1}")
d["y"] = 2
len(d)
)") == "2");

    // stringify (lists are ordered, deterministic)
    CHECK(evalStr(vm, "import(\"json\").stringify([1, 2, 3])") == "[1, 2, 3]");
    CHECK(evalStr(vm, "import(\"json\").stringify(\"hi\")") == "\"hi\"");
    CHECK(evalStr(vm, "import(\"json\").stringify(42)") == "42");
    CHECK(evalStr(vm, "import(\"json\").stringify(True)") == "true");
    CHECK(evalStr(vm, "import(\"json\").stringify(None)") == "null");
    CHECK(evalStr(vm, "import(\"json\").stringify([1, \"two\", [3]])") == "[1, \"two\", [3]]");

    // round-trip through a nested value
    CHECK(evalStr(vm, R"(
var j = import("json")
var x = j.parse("{\"k\": [1, 2, 3]}")
j.stringify(x["k"])
)") == "[1, 2, 3]");

    // unicode \u escape
    CHECK(evalStr(vm, "import(\"json\").parse(\"\\\"caf\\\\u00e9\\\"\")") == "caf\xc3\xa9");

    // error cases
    CHECK_THROWS(vm.runSource("import(\"json\").parse(\"{invalid}\")\n"));
    CHECK_THROWS(vm.runSource("import(\"json\").parse(\"[1, 2,\")\n"));
    // cyclic structures cannot be serialized
    CHECK_THROWS(vm.runSource("var a = []\na.append(a)\nimport(\"json\").stringify(a)\n"));

    return RUN_TESTS();
}
