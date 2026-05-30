#include <iostream>

#include "kirito.hpp"

// The standalone `ki` interpreter. The language itself is header-only (kirito.hpp); this is the
// only translation unit with a main(). File execution and the REPL get wired up in later milestones.
int main(int argc, char** argv) {
    kirito::KiritoVM vm;

    if (argc < 2) {
        std::cout << "kirito (ki)\nusage: ki <file.ki>\n";
        return 0;
    }

    std::cerr << "ki: running '" << argv[1] << "' is not wired up yet\n";
    return 1;
}
