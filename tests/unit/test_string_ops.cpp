#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;

    // --- Unicode (indexing/length by code point, not byte) ---
    CHECK(evalStr(vm, "len(\"h\xc3\xa9llo\")") == "5");          // héllo: 6 bytes, 5 code points
    CHECK(evalStr(vm, "\"h\xc3\xa9llo\"[1]") == "\xc3\xa9");      // -> é
    CHECK(evalStr(vm, "len(\"ab\xe2\x82\xac\")") == "3");        // ab€ (€ is 3 bytes)
    CHECK(evalStr(vm, "\"ab\xe2\x82\xac\"[2]") == "\xe2\x82\xac");
    CHECK(evalStr(vm, R"(
var n = 0
for c in "ab€":
    n = n + 1
n
)") == "3");

    // --- slicing (String) ---
    CHECK(evalStr(vm, "\"hello\"[1:4]") == "ell");
    CHECK(evalStr(vm, "\"hello\"[:3]") == "hel");
    CHECK(evalStr(vm, "\"hello\"[2:]") == "llo");
    CHECK(evalStr(vm, "\"hello\"[-2:]") == "lo");
    CHECK(evalStr(vm, "\"hello\"[::-1]") == "olleh");
    CHECK(evalStr(vm, "\"hello\"[::2]") == "hlo");

    // --- slicing (List) ---
    CHECK(evalStr(vm, "[1, 2, 3, 4, 5][1:4]") == "[2, 3, 4]");
    CHECK(evalStr(vm, "[1, 2, 3][::-1]") == "[3, 2, 1]");

    // --- in / not in ---
    CHECK(evalStr(vm, "\"ell\" in \"hello\"") == "True");
    CHECK(evalStr(vm, "\"z\" in \"hello\"") == "False");
    CHECK(evalStr(vm, "2 in [1, 2, 3]") == "True");
    CHECK(evalStr(vm, "5 not in [1, 2, 3]") == "True");
    CHECK(evalStr(vm, "\"a\" in {\"a\": 1}") == "True");
    CHECK(evalStr(vm, "2 in {1, 2, 3}") == "True");

    // --- repetition ---
    CHECK(evalStr(vm, "\"ab\" * 3") == "ababab");

    // --- methods ---
    CHECK(evalStr(vm, "\"Hello\".upper()") == "HELLO");
    CHECK(evalStr(vm, "\"Hello\".lower()") == "hello");
    CHECK(evalStr(vm, "\"  hi  \".strip()") == "hi");
    CHECK(evalStr(vm, "len(\"a,b,c\".split(\",\"))") == "3");
    CHECK(evalStr(vm, "\"a,b,c\".split(\",\")[1]") == "b");
    CHECK(evalStr(vm, "\",\".join([\"a\", \"b\", \"c\"])") == "a,b,c");
    CHECK(evalStr(vm, "\"hello\".replace(\"l\", \"L\")") == "heLLo");
    CHECK(evalStr(vm, "\"hello\".startswith(\"he\")") == "True");
    CHECK(evalStr(vm, "\"hello\".endswith(\"lo\")") == "True");
    CHECK(evalStr(vm, "\"hello\".find(\"ll\")") == "2");
    CHECK(evalStr(vm, "\"banana\".count(\"a\")") == "3");

    // --- formatting ---
    CHECK(evalStr(vm, "\"{} + {} = {}\".format(1, 2, 3)") == "1 + 2 = 3");
    CHECK(evalStr(vm, "\"{0} {0}\".format(\"hi\")") == "hi hi");
    CHECK(evalStr(vm, "\"{}%\".format(50)") == "50%");

    return RUN_TESTS();
}
