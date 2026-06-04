#include <cstdlib>
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;

    // platform is a non-empty String
    CHECK(vm.arena().deref(vm.runSource("import(\"sys\").platform")).kind() == ValueKind::String);
    CHECK(evalStr(vm, "len(import(\"sys\").platform) > 0") == "True");

    // set / get / unset round-trip
    CHECK(evalStr(vm, R"(
var sys = import("sys")
sys.setenv("KIRITO_UNITTEST_VAR", "value123")
sys.getenv("KIRITO_UNITTEST_VAR")
)") == "value123");

    // getenv default for unset names
    CHECK(evalStr(vm, "import(\"sys\").getenv(\"KIRITO_NEVER_SET_XYZ\", \"dflt\")") == "dflt");
    CHECK(evalStr(vm, "import(\"sys\").getenv(\"KIRITO_NEVER_SET_XYZ\")") == "None");

    // unset removes it
    CHECK(evalStr(vm, R"(
var sys = import("sys")
sys.setenv("KIRITO_TMP_X", "1")
sys.unsetenv("KIRITO_TMP_X")
sys.getenv("KIRITO_TMP_X", "removed")
)") == "removed");

    // value set from Kirito is visible to the C process environment
    {
        vm.runSource("import(\"sys\").setenv(\"KIRITO_FROM_KI\", \"seen\")");
        const char* v = std::getenv("KIRITO_FROM_KI");
        CHECK(v != nullptr && std::string(v) == "seen");
    }

    // environ() is a Dict containing a known var we just set
    CHECK(evalStr(vm, R"(
var sys = import("sys")
sys.setenv("KIRITO_ENV_CHECK", "yes")
var e = sys.environ()
e["KIRITO_ENV_CHECK"]
)") == "yes");
    CHECK(evalStr(vm, "type(import(\"sys\").environ())") == "Dict");

    // hostile: wrong argument types throw cleanly, not crash
    CHECK_THROWS(vm.runSource("import(\"sys\").getenv(42)"));
    CHECK_THROWS(vm.runSource("import(\"sys\").setenv(\"X\", 5)"));
    CHECK_THROWS(vm.runSource("import(\"sys\").setenv(1, 2)"));

    // empty value is allowed
    CHECK(evalStr(vm, R"(
var sys = import("sys")
sys.setenv("KIRITO_EMPTY", "")
sys.getenv("KIRITO_EMPTY", "x")
)") == "");

    // overwriting an existing var
    CHECK(evalStr(vm, R"(
var sys = import("sys")
sys.setenv("KIRITO_OVER", "first")
sys.setenv("KIRITO_OVER", "second")
sys.getenv("KIRITO_OVER")
)") == "second");

    // gettempdir: an existing directory (honors TMPDIR), pairs with io for scratch files
    CHECK(evalStr(vm, "var sys = import(\"sys\")\nimport(\"io\").isdir(sys.gettempdir()) and len(sys.gettempdir()) > 0") == "True");

    // joinpath: os.path.join semantics
    CHECK(evalStr(vm, "import(\"sys\").joinpath(\"a\", \"b\", \"c\")") == "a/b/c");
    CHECK(evalStr(vm, "import(\"sys\").joinpath(\"dir\")") == "dir");          // single component
    CHECK(evalStr(vm, "import(\"sys\").joinpath(\"a\", \"/abs\", \"b\")") == "/abs/b");  // absolute resets
    CHECK_THROWS(vm.runSource("import(\"sys\").joinpath()"));                 // needs >=1 part
    CHECK_THROWS(vm.runSource("import(\"sys\").joinpath(1, 2)"));             // non-String parts

    return RUN_TESTS();
}
