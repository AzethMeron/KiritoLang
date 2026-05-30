#include <random>
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;

    // basic round-trips
    CHECK(evalStr(vm, R"(
var z = import("zlib")
z.decompress(z.compress("hello world")) == "hello world"
)") == "True");
    CHECK(evalStr(vm, "var z = import(\"zlib\")\nz.decompress(z.compress(\"\")) == \"\"") == "True");
    CHECK(evalStr(vm, "var z = import(\"zlib\")\nz.decompress(z.compress(\"a\")) == \"a\"") == "True");

    // highly compressible data shrinks a lot
    CHECK(evalStr(vm, R"(
var z = import("zlib")
var data = "x" * 10000
var c = z.compress(data)
len(c) < 100 and z.decompress(c) == data
)") == "True");

    // raw deflate/inflate pair
    CHECK(evalStr(vm, R"(
var z = import("zlib")
var data = "The quick brown fox " * 20
z.inflate(z.deflate(data)) == data
)") == "True");

    // adler32 known values (matches RFC / Python's zlib.adler32)
    CHECK(evalStr(vm, "import(\"zlib\").adler32(\"\")") == "1");
    CHECK(evalStr(vm, "import(\"zlib\").adler32(\"hello\")") == "103547413");
    CHECK(evalStr(vm, "import(\"zlib\").adler32(\"Wikipedia\")") == "300286872");

    // binary-safe data (all byte values) round-trips
    CHECK(evalStr(vm, R"(
var z = import("zlib")
var s = ""
for i in range(256):
    s = s + "\x41"
z.decompress(z.compress(s)) == s
)") == "True");

    // random data of many sizes round-trips (C++ side, with non-text bytes)
    {
        std::mt19937 rng(7);
        for (int trial = 0; trial < 40; ++trial) {
            int n = std::uniform_int_distribution<int>(0, 2000)(rng);
            std::string data;
            for (int i = 0; i < n; ++i) data.push_back(static_cast<char>(rng() & 0xFF));
            Handle src = vm.makeString(data);
            vm.registerGlobal("_rnd", src);
            std::string r = evalStr(vm, "var z = import(\"zlib\")\nz.decompress(z.compress(_rnd)) == _rnd");
            CHECK(r == "True");
        }
    }

    // a large structured document
    CHECK(evalStr(vm, R"(
var z = import("zlib")
var parts = []
for i in range(500):
    parts.append("line " + String(i) + ": some repeated content here\n")
var doc = "".join(parts)
var c = z.compress(doc)
len(c) < len(doc) and z.decompress(c) == doc
)") == "True");

    // hostile / corrupt inputs throw cleanly, never crash
    CHECK_THROWS(vm.runSource("import(\"zlib\").decompress(\"not zlib data\")"));
    CHECK_THROWS(vm.runSource("import(\"zlib\").decompress(\"\")"));
    CHECK_THROWS(vm.runSource("import(\"zlib\").decompress(\"\\x78\\x9c\\xff\\xff\\xff\\xff\")"));  // bad body
    CHECK_THROWS(vm.runSource("import(\"zlib\").inflate(\"\\xff\\xff\\xff\")"));  // bad block type / truncated
    CHECK_THROWS(vm.runSource("import(\"zlib\").compress(42)"));  // wrong type
    CHECK_THROWS(vm.runSource("import(\"zlib\").adler32([1, 2])"));  // wrong type

    // corrupting a valid stream's checksum is detected
    CHECK(evalStr(vm, R"(
var z = import("zlib")
var c = z.compress("important data")
var corrupted = c[0:len(c) - 1] + "\x00"
var ok = "no"
try:
    z.decompress(corrupted)
catch as e:
    ok = "caught"
ok
)") == "caught");

    return RUN_TESTS();
}
