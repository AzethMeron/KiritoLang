#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "kirito.hpp"

// The standalone `ki` interpreter. The language itself is header-only (kirito.hpp); this is the
// only translation unit with a main().
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "kirito (ki)\nusage: ki <file.ki>\n";
        return 0;
    }

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
