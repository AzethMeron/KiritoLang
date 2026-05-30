#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "kirito.hpp"

// The standalone `ki` interpreter. The language itself is header-only (kirito.hpp); this is the
// only translation unit with a main().
// Read-eval-print loop: evaluate a line in a persistent scope and print non-None results.
static int repl() {
    std::cout << "kirito (ki) REPL — Ctrl-D to exit\n";
    kirito::KiritoVM vm;
    std::string line;
    while (std::cout << ">>> " && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        try {
            kirito::Handle result = vm.runRepl(line);
            if (vm.arena().deref(result).kind() != kirito::ValueKind::None)
                std::cout << vm.stringify(result) << "\n";
        } catch (const kirito::KiritoError& e) {
            std::cerr << "error: " << e.what() << " (line " << e.span.line << ":" << e.span.col
                      << ")\n";
        }
    }
    std::cout << "\n";
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) return repl();

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "ki: cannot open '" << argv[1] << "'\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    kirito::KiritoVM vm;
    try {
        vm.runSource(buffer.str(), argv[1]);  // a file runner discards the program's final value
    } catch (const kirito::KiritoError& e) {
        std::cerr << argv[1] << ":" << e.span.line << ":" << e.span.col << ": error: "
                  << e.what() << "\n";
        return 1;
    }
    return 0;
}
