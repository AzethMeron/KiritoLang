// C++-level regression tests for the deep audit findings. Pin each fix from C++ (independent of the
// .ki probe suite): Integer(String) honours 0x/0o/0b prefixes; List inspect strings advertise the
// actual kwarg names; csv.parse keeps RFC-4180 quoted newlines; net.urlsplit handles bracketed IPv6.
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string ev(KiritoVM& vm, const std::string& src) { return vm.stringify(vm.runSource(src)); }
static bool raises(KiritoVM& vm, const std::string& src) {
    try { vm.runSource(src); return false; } catch (...) { return true; }
}

int main() {
    // ---- Integer(String) accepts 0x/0o/0b prefixes (matching the documented promise) ----
    {
        KiritoVM vm;
        CHECK(ev(vm, "Integer(\"0xFF\")") == "255");
        CHECK(ev(vm, "Integer(\"0o17\")") == "15");
        CHECK(ev(vm, "Integer(\"0b1010\")") == "10");
        CHECK(ev(vm, "Integer(\"-0xFF\")") == "-255");
        CHECK(ev(vm, "Integer(\"+0xFF\")") == "255");
        CHECK(ev(vm, "Integer(\"0X1A\")") == "26");      // uppercase 0X
        CHECK(ev(vm, "Integer(\" 0xFF \")") == "255");   // surrounding whitespace tolerated
        // garbage still rejected
        CHECK(raises(vm, "Integer(\"0xZZ\")"));          // bad hex digit
        CHECK(raises(vm, "Integer(\"42abc\")"));         // trailing garbage
        CHECK(raises(vm, "Integer(\"0x\")"));            // prefix alone, no value
        CHECK(raises(vm, "Integer(\"0b3\")"));           // 3 isn't a binary digit
        // decimal still works as before
        CHECK(ev(vm, "Integer(\"42\")") == "42");
        CHECK(ev(vm, "Integer(\"-42\")") == "-42");
    }

    // ---- List inspect strings match the actual kwarg names (count/index/remove use 'value') ----
    {
        KiritoVM vm;
        // inspect mentions the bound name 'value', not the old 'item'
        CHECK(ev(vm, "\"count(value)\" in inspect([1, 2])") == "True");
        CHECK(ev(vm, "\"index(value, start, end)\" in inspect([1, 2])") == "True");
        CHECK(ev(vm, "\"remove(value)\" in inspect([1, 2])") == "True");
        // and the bindings now actually accept that name
        CHECK(ev(vm, "[1, 2, 2, 3].count(value = 2)") == "2");
        CHECK(ev(vm, "[1, 2, 3].index(value = 2)") == "1");
        CHECK(ev(vm, "var xs = [1, 2, 3]\nxs.remove(value = 2)\nxs") == "[1, 3]");
        // and `item` is now rejected (was never the right name despite the old inspect string)
        CHECK(raises(vm, "[1].count(item = 1)"));
    }

    // ---- csv.parse keeps RFC-4180 quoted newlines as part of the field, not as a row separator ----
    {
        KiritoVM vm;
        CHECK(ev(vm, "import(\"csv\").parse(\"\\\"a\\nb\\\",c\")") == "[[a\nb, c]]");
        // trailing newline doesn't add an empty row
        CHECK(ev(vm, "len(import(\"csv\").parse(\"x,y\\n\"))") == "1");
        // blank middle line IS preserved (an empty row)
        CHECK(ev(vm, "len(import(\"csv\").parse(\"a,b\\n\\nc,d\"))") == "3");
        // doubled-quote escape inside a quoted field still works
        CHECK(ev(vm, "import(\"csv\").parse(\"\\\"he said \\\"\\\"hi\\\"\\\"\\\",ok\")")
              == "[[he said \"hi\", ok]]");
        // round-trip a row with quoteworthy content
        CHECK(ev(vm, "var c = import(\"csv\")\nvar data = [[\"x,y\", \"z\\nw\"], [\"a\", \"b\"]]\n"
                     "c.parse(c.format(data)) == data") == "True");
    }

    // ---- net.urlsplit handles bracketed IPv6 literals; port is the suffix after ']' ----
    {
        KiritoVM vm;
        CHECK(ev(vm, "import(\"net\").urlsplit(\"http://[::1]:8080/path\")[\"host\"]") == "[::1]");
        CHECK(ev(vm, "import(\"net\").urlsplit(\"http://[::1]:8080/path\")[\"port\"]") == "8080");
        CHECK(ev(vm, "import(\"net\").urlsplit(\"http://[::1]:8080/path\")[\"path\"]") == "/path");
        // IPv6 without an explicit port
        CHECK(ev(vm, "import(\"net\").urlsplit(\"http://[2001:db8::1]/\")[\"host\"]")
              == "[2001:db8::1]");
        CHECK(ev(vm, "import(\"net\").urlsplit(\"http://[2001:db8::1]/\")[\"port\"]") == "");
        // ordinary hostnames still work
        CHECK(ev(vm, "import(\"net\").urlsplit(\"http://example.com:80/p\")[\"host\"]")
              == "example.com");
        CHECK(ev(vm, "import(\"net\").urlsplit(\"http://example.com:80/p\")[\"port\"]") == "80");
    }

    // ---- Dict.remove on a missing key raises (doc says: "raises if absent; like pop but returns
    //      nothing"). Before the fix the bool return of DictVal::remove was discarded and the
    //      method was a silent no-op.
    {
        KiritoVM vm;
        // present key: removes and returns None
        CHECK(ev(vm, "var d = {\"a\": 1, \"b\": 2}\nd.remove(\"a\")\nd") == "{b: 2}");
        // missing key: raises
        CHECK(raises(vm, "var d = {\"a\": 1}\nd.remove(\"missing\")"));
        // and the dict is unchanged after the raise
        CHECK(ev(vm, "var d = {\"a\": 1}\ntry:\n    d.remove(\"missing\")\ncatch as e:\n    pass\nd") == "{a: 1}");
        // kwarg form `key=` still works
        CHECK(ev(vm, "var d = {\"x\": 5}\nd.remove(key = \"x\")\nd") == "{}");
    }

    // ---- parseDouble: a subnormal/underflowing float no longer crashes the lexer or the
    //      serializers (std::stod threw std::out_of_range on underflow -> SIGABRT). It now parses
    //      to the (subnormal) value everywhere a double is read from text.
    {
        KiritoVM vm;
        // a subnormal literal lexes instead of aborting; it's tiny and positive
        CHECK(ev(vm, "var x = 5e-324\nx > 0.0 and x < 1e-300") == "True");
        CHECK(ev(vm, "1e-308 > 0.0") == "True");
        // Float(String) accepts a subnormal too
        CHECK(ev(vm, "Float(\"5e-324\") > 0.0") == "True");
        // genuine overflow still raises (unchanged: parseDouble treats ±inf as out-of-range, and
        // the converter surfaces it as a clear conversion error — only underflow was the crash)
        CHECK(raises(vm, "Float(\"1e400\")"));
        // serialize (text) + dump (binary) round-trip a subnormal that std::stod would have rejected
        CHECK(ev(vm, "var s = import(\"serialize\")\nvar t = 1e-308 * 0.001\ns.loads(s.dumps(t)) == t") == "True");
        CHECK(ev(vm, "var d = import(\"dump\")\nvar t = 1e-308 * 0.001\nd.loads(d.dumps(t)) == t") == "True");
    }

    // ---- json emits and re-parses the Python-json non-finite spelling (NaN / Infinity /
    //      -Infinity), so a structure with a non-finite Float round-trips (was lowercase nan/inf,
    //      which json.parse rejected).
    {
        KiritoVM vm;
        CHECK(ev(vm, "import(\"json\").dumps(import(\"math\").inf)") == "Infinity");
        CHECK(ev(vm, "import(\"json\").dumps(-import(\"math\").inf)") == "-Infinity");
        CHECK(ev(vm, "import(\"json\").dumps(import(\"math\").nan)") == "NaN");
        CHECK(ev(vm, "import(\"json\").parse(\"Infinity\") == import(\"math\").inf") == "True");
        CHECK(ev(vm, "import(\"json\").parse(\"-Infinity\") == -import(\"math\").inf") == "True");
        CHECK(ev(vm, "import(\"math\").isnan(import(\"json\").parse(\"NaN\"))") == "True");
        CHECK(ev(vm, "var j = import(\"json\")\nvar m = import(\"math\")\n"
                     "j.loads(j.dumps([m.inf, -m.inf])) == [m.inf, -m.inf]") == "True");
    }
}
