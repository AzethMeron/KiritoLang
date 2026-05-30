#include <filesystem>
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    KiritoVM vm;
    auto dir = std::filesystem::temp_directory_path() / "kirito_io_test";
    std::filesystem::remove_all(dir);
    std::string base = "var io = import(\"io\")\nvar dir = \"" + dir.string() + "\"\n";

    // mkdir + writelines + line iteration
    CHECK(evalStr(vm, base + R"(
io.mkdir(dir)
with io.open(dir + "/a.txt", "w") as f:
    f.writelines(["one\n", "two\n", "three\n"])
var n = 0
with io.open(dir + "/a.txt", "r") as f:
    for line in f:
        n = n + 1
n
)") == "3");

    // readlines
    CHECK(evalStr(vm, base + R"(
with io.open(dir + "/a.txt", "r") as f:
    var lines = f.readlines()
len(lines)
)") == "3");

    // seek / tell
    CHECK(evalStr(vm, base + R"(
var g = io.open(dir + "/a.txt", "r")
var start = g.tell()
g.seek(4)
var pos = g.tell()
g.close()
String(start) + "," + String(pos)
)") == "0,4");

    // append mode
    CHECK(evalStr(vm, base + R"(
with io.open(dir + "/a.txt", "a") as f:
    f.write("four\n")
var c = 0
with io.open(dir + "/a.txt", "r") as f:
    for line in f:
        c = c + 1
c
)") == "4");

    // filesystem helpers: exists / listdir / rename / remove
    CHECK(evalStr(vm, base + "io.exists(dir + \"/a.txt\")") == "True");
    CHECK(evalStr(vm, base + "io.exists(dir + \"/missing.txt\")") == "False");
    CHECK(evalStr(vm, base + "\"a.txt\" in io.listdir(dir)") == "True");
    CHECK(evalStr(vm, base + R"(
io.rename(dir + "/a.txt", dir + "/b.txt")
io.exists(dir + "/b.txt") and not io.exists(dir + "/a.txt")
)") == "True");
    CHECK(evalStr(vm, base + R"(
io.remove(dir + "/b.txt")
io.exists(dir + "/b.txt")
)") == "False");

    std::filesystem::remove_all(dir);
    return RUN_TESTS();
}
