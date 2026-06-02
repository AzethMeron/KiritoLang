#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

int main() {
    KiritoVM vm;
    auto dir = std::filesystem::temp_directory_path() / "kirito_imports_test";
    std::filesystem::create_directories(dir);
    {
        std::ofstream f(dir / "mymod.ki");
        f << "var answer = 42\n"
             "var add = Function(a, b):\n"
             "    return a + b\n"
             "class Greeter:\n"
             "    var _init_ = Function(self, who):\n"
             "        self.who = who\n"
             "    var hello = Function(self):\n"
             "        return \"hi \" + self.who\n";
    }
    vm.addLibPath(dir.string());

    // a .ki file's top-level bindings become the module's members
    CHECK(vm.stringify(vm.runSource("import(\"mymod\").answer")) == "42");
    CHECK(vm.stringify(vm.runSource("import(\"mymod\").add(3, 4)")) == "7");
    CHECK(vm.stringify(vm.runSource("import(\"mymod\").Greeter(\"Kirito\").hello()")) == "hi Kirito");

    // imports are cached (per-VM singleton): same handle each time
    Handle m1 = vm.runSource("import(\"mymod\")");
    Handle m2 = vm.runSource("import(\"mymod\")");
    CHECK(m1 == m2);

    // the name may include the .ki extension: import("mymod.ki") resolves the same file (not
    // mymod.ki.ki) and yields the same module as import("mymod").
    CHECK(vm.stringify(vm.runSource("import(\"mymod.ki\").answer")) == "42");
    CHECK(vm.runSource("import(\"mymod.ki\")") == vm.runSource("import(\"mymod\")"));

    // a missing module is an error
    CHECK_THROWS(vm.runSource("import(\"does_not_exist\")"));

    // native modules still take precedence and work
    CHECK(vm.stringify(vm.runSource("import(\"math\").sqrt(49)")) == "7.0");

    // a parse error inside an imported module is attributed to THAT module's file, not the caller.
    {
        std::ofstream f(dir / "broken.ki");
        f << "var bad = Function(a, : return a\n";  // syntax error on line 1
    }
    bool tagged = false;
    try {
        vm.runSource("import(\"broken\")", "<entry>");
    } catch (const KiritoError& e) {
        tagged = e.file.find("broken.ki") != std::string::npos;
    }
    CHECK(tagged);

    // The module is a per-VM singleton parsed ONCE: a mutation to a module-level binding made
    // through one import is visible through a later import of the same name (shared state).
    {
        KiritoVM v2;
        v2.addLibPath(dir.string());
        CHECK(v2.stringify(v2.runSource("import(\"mymod\").answer")) == "42");
        v2.runSource("var m = import(\"mymod\")\nm.answer = 100");   // mutate the shared module object
        CHECK(v2.stringify(v2.runSource("import(\"mymod\").answer")) == "100");  // seen by a fresh import
    }

    // arglist / argmain: a directly-run file sees the command-line args and argmain == True; an
    // imported module sees argmain == False and an EMPTY arglist (the args belong to the program).
    {
        KiritoVM v3;
        v3.addLibPath(dir.string());
        v3.setArgs({"alpha", "beta"});
        CHECK(v3.stringify(v3.runSource("arglist")) == "[alpha, beta]");
        CHECK(v3.stringify(v3.runSource("argmain")) == "True");
        { std::ofstream f(dir / "argmod.ki"); f << "var seenMain = argmain\nvar seenArgs = arglist\n"; }
        CHECK(v3.stringify(v3.runSource("import(\"argmod\").seenMain")) == "False");
        CHECK(v3.stringify(v3.runSource("import(\"argmod\").seenArgs")) == "[]");
        // a fresh VM with no setArgs: arglist defaults to empty, argmain still True for a run file
        KiritoVM v4;
        CHECK(v4.stringify(v4.runSource("arglist")) == "[]");
        CHECK(v4.stringify(v4.runSource("argmain")) == "True");
    }

    // arglist edge cases: order preserved, indexing/len, and arguments with spaces, an embedded
    // empty string, unicode, and numeric-looking strings (which stay Strings, not Integers).
    {
        KiritoVM v;
        v.setArgs({"a b c", "", "café", "007", "--flag"});
        CHECK(v.stringify(v.runSource("len(arglist)")) == "5");
        CHECK(v.stringify(v.runSource("arglist[0]")) == "a b c");      // a space-containing arg is one element
        CHECK(v.stringify(v.runSource("arglist[1]")) == "");           // empty string preserved
        CHECK(v.stringify(v.runSource("arglist[2]")) == "café");       // unicode
        CHECK(v.stringify(v.runSource("arglist[3]")) == "007");        // stays a String
        CHECK(v.stringify(v.runSource("type(arglist[3])")) == "String");
        CHECK(v.stringify(v.runSource("arglist[4]")) == "--flag");     // option-looking arg is just a string
        // re-setting replaces the argument list for subsequent runs
        v.setArgs({"only"});
        CHECK(v.stringify(v.runSource("arglist")) == "[only]");
    }

    // nested imports: a module imported by an imported module is also not main and sees no args.
    {
        KiritoVM v;
        v.addLibPath(dir.string());
        v.setArgs({"x", "y"});
        { std::ofstream f(dir / "leaf.ki"); f << "var m = argmain\nvar a = arglist\n"; }
        { std::ofstream f(dir / "mid.ki"); f << "var leaf = import(\"leaf\")\nvar m = argmain\nvar a = arglist\n"; }
        CHECK(v.stringify(v.runSource("import(\"mid\").m")) == "False");          // the imported module
        CHECK(v.stringify(v.runSource("import(\"mid\").a")) == "[]");
        CHECK(v.stringify(v.runSource("import(\"mid\").leaf.m")) == "False");     // its transitive import
        CHECK(v.stringify(v.runSource("import(\"mid\").leaf.a")) == "[]");
    }

    // fuzz: a random list of arbitrary-byte arguments round-trips through arglist exactly.
    {
        std::mt19937 rng(0xA2C1);
        for (int iter = 0; iter < 500; ++iter) {
            KiritoVM v;
            std::size_t n = rng() % 8;
            std::vector<std::string> args;
            for (std::size_t i = 0; i < n; ++i) {
                std::string s;
                std::size_t len = rng() % 6;
                for (std::size_t j = 0; j < len; ++j) s += static_cast<char>('a' + (rng() % 26));
                args.push_back(s);
            }
            v.setArgs(args);
            CHECK(v.stringify(v.runSource("len(arglist)")) == std::to_string(args.size()));
            for (std::size_t i = 0; i < args.size(); ++i)
                CHECK(v.stringify(v.runSource("arglist[" + std::to_string(i) + "]")) == args[i]);
        }
    }

    std::filesystem::remove_all(dir);
    return RUN_TESTS();
}
