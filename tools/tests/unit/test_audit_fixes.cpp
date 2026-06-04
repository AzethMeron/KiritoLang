// C++-level regression tests for the behaviours fixed/added during the deep audit. These drive the
// interpreter through the embedding entry point (runSource) so the fixes are pinned from C++, not
// only from the .ki probe suite: VM-aware container equality, instance _call_ kwargs, `not` ->
// _not_, inspect completeness for native objects, regex pos/endpos + attributes, File.seek whence,
// len(BytesIO), DateTime arithmetic (incl. Float), Dict.update with pairs, range/randrange kwargs,
// and cycle-safe deepcopy.
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string ev(KiritoVM& vm, const std::string& src) { return vm.stringify(vm.runSource(src)); }

int main() {
    // ---- VM-aware container equality honours nested instance _eq_ ----
    {
        KiritoVM vm;
        const std::string cls = "class P:\n    var _init_ = Function(self, v):\n        self.v = v\n"
                                "    var _eq_ = Function(self, o) -> Bool:\n        return self.v == o.v\n";
        CHECK(ev(vm, cls + "[P(1)] == [P(1)]") == "True");
        CHECK(ev(vm, cls + "[P(1)] == [P(2)]") == "False");
        CHECK(ev(vm, cls + "{\"k\": P(3)} == {\"k\": P(3)}") == "True");
        CHECK(ev(vm, cls + "[[P(1)], [P(2)]] == [[P(1)], [P(2)]]") == "True");
        CHECK(ev(vm, cls + "[P(1)] != [P(2)]") == "True");
    }
    // ---- NaN never equals itself, even as the same object (identity short-circuit is container-only) ----
    {
        KiritoVM vm;
        CHECK(ev(vm, "var n = import(\"math\").nan\nn == n") == "False");                    // nan==nan
        CHECK(ev(vm, "var x = [1, 2]\nx == x") == "True");                                   // a list IS itself
    }
    // ---- calling an instance forwards keyword arguments to _call_ ----
    {
        KiritoVM vm;
        const std::string adder = "class Add:\n    var _init_ = Function(self, b):\n        self.b = b\n"
                                  "    var _call_ = Function(self, x):\n        return self.b + x\n"
                                  "var a = Add(10)\n";
        CHECK(ev(vm, adder + "a(5)") == "15");
        CHECK(ev(vm, adder + "a(x = 7)") == "17");
    }
    // ---- `not obj` dispatches a user _not_ ----
    {
        KiritoVM vm;
        const std::string sw = "class Sw:\n    var _init_ = Function(self, on):\n        self.on = on\n"
                               "    var _not_ = Function(self) -> Bool:\n        return not self.on\n";
        CHECK(ev(vm, sw + "not Sw(False)") == "True");
        CHECK(ev(vm, sw + "not Sw(True)") == "False");
        CHECK(ev(vm, sw + "not (not Sw(True))") == "True");
    }
    // ---- inspect() lists native-object attributes that used to be omitted ----
    {
        KiritoVM vm;
        std::string rx = ev(vm, "inspect(import(\"regex\").compile(\"(?P<y>.)\"))");
        CHECK(rx.find("pattern: String") != std::string::npos);
        CHECK(rx.find("groups: Integer") != std::string::npos);
        CHECK(rx.find("groupindex: Dict") != std::string::npos);
        std::string mt = ev(vm, "inspect(import(\"regex\").compile(\"(.)\").search(\"a\"))");
        CHECK(mt.find("string: String") != std::string::npos);
        std::string dt = ev(vm, "inspect(import(\"time\").now())");
        CHECK(dt.find("isoformat() -> String") != std::string::npos);
        CHECK(dt.find("timestamp: Integer") != std::string::npos);
    }
    // ---- regex pos / endpos are honoured (endpos used to be silently ignored) ----
    {
        KiritoVM vm;
        CHECK(ev(vm, "import(\"regex\").compile(\"a+\").search(\"xaaax\", 1).group()") == "aaa");
        CHECK(ev(vm, "import(\"regex\").compile(\"a+\").search(\"aaaa\", 0, 2).group()") == "aa");
        CHECK(ev(vm, "import(\"regex\").compile(\"a$\").search(\"aXa\", 0, 1).group()") == "a");
        CHECK(ev(vm, "import(\"regex\").compile(\"\\\\w\").findall(\"abcd\", 1, 3)") == "[b, c]");
        CHECK(ev(vm, "import(\"regex\").compile(\"(?P<n>\\\\d)\").groupindex") == "{n: 1}");
        CHECK(ev(vm, "import(\"regex\").compile(\"(.)\").search(\"ab\").string") == "ab");
    }
    // ---- File.seek honours whence and returns the new position; len(BytesIO) works ----
    {
        KiritoVM vm;
        std::string tmp = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/ki_af_seek.txt";
        ev(vm, "var f = import(\"io\").open(\"" + tmp + "\", \"w\")\ndiscard f.write(\"0123456789\")\nf.close()");
        CHECK(ev(vm, "var g = import(\"io\").open(\"" + tmp + "\", \"r\")\ndiscard g.seek(5)\nvar r = g.read(2)\ng.close()\nr") == "56");
        CHECK(ev(vm, "var g = import(\"io\").open(\"" + tmp + "\", \"r\")\nvar p = g.seek(3)\ng.close()\np") == "3");
        ev(vm, "import(\"io\").remove(\"" + tmp + "\")");
        CHECK(ev(vm, "len(import(\"io\").BytesIO(\"abcde\"))") == "5");
        CHECK(ev(vm, "import(\"io\").BytesIO(\"l1\\nl2\\n\").readline()") == "l1");
    }
    // ---- DateTime arithmetic incl. Float seconds; isoformat alias ----
    {
        KiritoVM vm;
        CHECK(ev(vm, "import(\"time\").datetime(0).add(86400).day") == "2");
        CHECK(ev(vm, "import(\"time\").datetime(86400).sub(86400).day") == "1");
        CHECK(ev(vm, "import(\"time\").datetime(0).add(3600.0).hour") == "1");          // Float seconds
        CHECK(ev(vm, "import(\"time\").datetime(100).diff(import(\"time\").datetime(40))") == "60");
        CHECK(ev(vm, "var d = import(\"time\").datetime(0)\nd.isoformat() == d.iso()") == "True");
        CHECK(ev(vm, "import(\"time\").datetime(0).yearday") == "1");
    }
    // ---- Dict.update accepts a list of [key, value] pairs ----
    {
        KiritoVM vm;
        CHECK(ev(vm, "var d = {\"a\": 1}\nd.update([[\"b\", 2], [\"c\", 3]])\nd[\"b\"] + d[\"c\"]") == "5");
        CHECK(ev(vm, "var d = {\"a\": 1}\nd.update({\"a\": 9})\nd[\"a\"]") == "9");
    }
    // ---- range / randrange keyword arguments ----
    {
        KiritoVM vm;
        CHECK(ev(vm, "range(start = 2, stop = 8, step = 2)") == "[2, 4, 6]");
        CHECK(ev(vm, "range(stop = 3)") == "[0, 1, 2]");
        CHECK(ev(vm, "range(5, step = 2)") == "[0, 2, 4]");
        CHECK(ev(vm, "var r = import(\"random\").Random(1).randrange(start = 4, stop = 5)\nr") == "4");
    }
    // ---- cycle-safe + shared-ref deepcopy ----
    {
        KiritoVM vm;
        CHECK(ev(vm, "var c = [1]\nc.append(c)\nvar d = import(\"copy\").deepcopy(c)\nid(d[1]) == id(d)") == "True");
        CHECK(ev(vm, "var s = [0]\nvar d = import(\"copy\").deepcopy([s, s])\nid(d[0]) == id(d[1])") == "True");
    }

    return RUN_TESTS();
}
