// Named (keyword) arguments, default parameter values, and enforcing type annotations.
// Annotations are not hints: a param ': Type' must receive a matching instance (inheritance-aware
// for user classes) and a '-> Type' return annotation is checked on the result.
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

// Each runSource gets a fresh module scope, so every case is a self-contained program whose last
// expression is the value we assert on.
static std::string run(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

int main() {
    // --- named arguments + defaults on NATIVE functions (builtins and module functions) ---
    {
        KiritoVM vm;
        // builtin `round` with a signature: keyword arg and default.
        CHECK(run(vm, "round(3.14159, ndigits=2)") == "3.14");
        CHECK(run(vm, "round(2.7)") == "3");            // default ndigits=None -> Integer
        CHECK(run(vm, "round(2.71828, ndigits=3)") == "2.718");
        // module function `io.open` with a default mode.
        CHECK(run(vm,
            "var io = import(\"io\")\nvar p = io.gettempdir() + \"/kirito_namedargs_native.txt\"\n"
            "var f = io.open(p, mode=\"w\")\ndiscard f.write(\"yo\")\nf.close()\n"
            "var g = io.open(p)\nvar s = g.read()\ng.close()\nio.remove(p)\ns") == "yo");
    }
    // native-signature errors: unknown keyword, duplicate, too many positional, missing required.
    {
        KiritoVM vm;
        CHECK_THROWS(vm.runSource("round(1, bogus=2)"));
        CHECK_THROWS(vm.runSource("var io = import(\"io\")\nio.open(path=\"x\", path=\"y\")"));
        CHECK_THROWS(vm.runSource("var io = import(\"io\")\nio.open(\"a\", \"b\", \"c\")"));
        CHECK_THROWS(vm.runSource("var io = import(\"io\")\nio.open()"));
    }

    // --- named arguments, any order, mixed with positional ---
    {
        KiritoVM vm;
        std::string def = "var sub = Function(a, b):\n    return a - b\n";
        CHECK(run(vm, def + "sub(10, 3)") == "7");
        CHECK(run(vm, def + "sub(a = 10, b = 3)") == "7");
        CHECK(run(vm, def + "sub(b = 3, a = 10)") == "7");   // order-independent
        CHECK(run(vm, def + "sub(10, b = 3)") == "7");        // positional then keyword
    }

    // --- default parameter values ---
    {
        KiritoVM vm;
        std::string def = "var power = Function(base, exp = 2):\n    return base ** exp\n";
        CHECK(run(vm, def + "power(5)") == "25");
        CHECK(run(vm, def + "power(5, 3)") == "125");
        CHECK(run(vm, def + "power(base = 4)") == "16");
        CHECK(run(vm, def + "power(exp = 3, base = 2)") == "8");
    }

    // --- argument-binding errors (caught at Kirito level as String messages) ---
    {
        KiritoVM vm;
        std::string def = "var f = Function(a, b):\n    return a\n";
        auto err = [&](const std::string& call) {
            return run(vm, def + "try:\n    " + call + "\ncatch as e:\n    e\n");
        };
        CHECK(err("f(1, 2, 3)").find("takes 2 positional argument") != std::string::npos);
        CHECK(err("f(1, a = 2)").find("multiple values for argument 'a'") != std::string::npos);
        CHECK(err("f(1, c = 2)").find("unexpected keyword argument 'c'") != std::string::npos);
        CHECK(err("f(1)").find("missing required argument 'b'") != std::string::npos);
        CHECK(err("f(a = 1, 2)").find("positional argument follows keyword") != std::string::npos);
    }

    // --- enforcing parameter annotations (built-in types) ---
    {
        KiritoVM vm;
        std::string def = "var keys = Function(d : Dict):\n    return len(d)\n";
        CHECK(run(vm, def + "keys({1: 2, 3: 4})") == "2");
        auto err = [&](const std::string& call) {
            return run(vm, def + "try:\n    " + call + "\ncatch as e:\n    e\n");
        };
        CHECK(err("keys([1, 2, 3])") == "argument 'd' must be Dict, got List");
        CHECK(err("keys(5)") == "argument 'd' must be Dict, got Integer");
    }

    // --- enforcing return annotations ---
    {
        KiritoVM vm;
        CHECK(run(vm, "var half = Function(n : Integer) -> Float:\n    return n / 2\nhalf(7)") == "3.5");
        std::string bad = "var bad = Function() -> Integer:\n    return \"nope\"\n";
        CHECK(run(vm, bad + "try:\n    bad()\ncatch as e:\n    e\n")
              == "function must return Integer, got String");
    }

    // --- inheritance: a subclass instance satisfies a base-class annotation ---
    {
        KiritoVM vm;
        std::string def =
            "class Animal:\n"
            "    var speak = Function(self):\n"
            "        return \"...\"\n"
            "class Dog(Animal):\n"
            "    var speak = Function(self):\n"
            "        return \"woof\"\n"
            "var hear = Function(a : Animal):\n"
            "    return a.speak()\n";
        CHECK(run(vm, def + "hear(Dog())") == "woof");     // child passes base annotation
        CHECK(run(vm, def + "hear(Animal())") == "...");
        CHECK(run(vm, def + "try:\n    hear(42)\ncatch as e:\n    e\n").find("must be Animal, got Integer")
              != std::string::npos);
    }

    // --- 'Any' / no annotation accept everything ---
    {
        KiritoVM vm;
        std::string def = "var id = Function(x : Any):\n    return x\n";
        CHECK(run(vm, def + "id(\"s\")") == "s");
        CHECK(run(vm, def + "id([1])") == "[1]");
        CHECK(run(vm, "len([1, 2])") == "2");
    }
    // a signatured native takes keyword args by its declared parameter name, and rejects unknown ones
    {
        KiritoVM vm;
        CHECK(run(vm, "len(x = [1, 2])") == "2");  // `len`'s parameter is named `x`
        CHECK(run(vm, "try:\n    len(obj = [1, 2])\ncatch as e:\n    e\n")
                  .find("unexpected keyword argument 'obj'") != std::string::npos);
        // a signatureless native (variadic) still rejects keyword args wholesale
        CHECK(run(vm, "var io = import(\"io\")\ntry:\n    io.print(end = 1)\ncatch as e:\n    e\n")
                  .find("does not accept keyword arguments") != std::string::npos);
    }

    return RUN_TESTS();
}
