#include <filesystem>
#include <fstream>
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

    // Same file reached through two library dirs that both contain mymod.ki resolves to the SAME
    // module (deduplicated by resolved path), and module-level state is shared (parsed once).
    {
        auto dir2 = std::filesystem::temp_directory_path() / "kirito_imports_test2";
        std::filesystem::create_directories(dir2);
        std::filesystem::copy_file(dir / "mymod.ki", dir2 / "mymod.ki",
                                   std::filesystem::copy_options::overwrite_existing);
        KiritoVM v2;
        v2.addLibPath(dir.string());
        Handle a = v2.runSource("import(\"mymod\")");
        Handle b = v2.runSource("import(\"mymod\")");
        CHECK(a == b);  // repeated import of the same name -> same handle
        std::filesystem::remove_all(dir2);
    }

    std::filesystem::remove_all(dir);
    return RUN_TESTS();
}
