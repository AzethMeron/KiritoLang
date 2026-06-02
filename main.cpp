#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "kirito.hpp"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

// To add a new module to the interpreter: #include its header and call vm.install<Module>().
// (The bundled stdlib is already registered by the VM; this is where a user wires in their own.)
//   #include "mymodule.hpp"
//   vm.install<MyModule>();

namespace {

#ifdef _WIN32
// Put the Windows console into UTF-8 mode for the program's lifetime, so Kirito's Unicode output
// (code-point strings, etc.) renders correctly instead of as mojibake, and restore it on exit. On
// non-Windows this is unnecessary (terminals are already UTF-8).
struct ConsoleUtf8 {
    UINT out_, in_;
    ConsoleUtf8() : out_(GetConsoleOutputCP()), in_(GetConsoleCP()) {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }
    ~ConsoleUtf8() { SetConsoleOutputCP(out_); SetConsoleCP(in_); }
};
#endif

void usage() {
    std::cout << "kirito (ki) - a high-level scripting language\n"
                 "usage: ki [options] [file.ki [args...]]\n"
                 "  with no file, starts an interactive REPL\n"
                 "options:\n"
                 "  --lib <dir>   add a directory to the module import path (repeatable)\n"
                 "  -h, --help    show this help\n";
}


int repl(kirito::KiritoVM& vm) {
    std::cout << "kirito (ki) REPL - Ctrl-D to exit\n";
    std::string line;
    while (std::cout << ">>> " && std::getline(std::cin, line)) {
        if (line.empty()) continue;

        // Multiline blocks: if a line opens an indented suite (ends with ':', ignoring trailing
        // spaces and a trailing comment), keep reading with a continuation prompt until a blank
        // line, then evaluate the whole buffer. This lets `Function():`, `if`, `for`, `class`, etc.
        // be typed interactively instead of erroring on the missing indented block.
        auto opensBlock = [](const std::string& s) {
            std::size_t end = s.size();
            bool inStr = false;
            char q = 0;
            std::size_t comment = std::string::npos;
            for (std::size_t i = 0; i < s.size(); ++i) {  // find an unquoted '#'
                char c = s[i];
                if (inStr) { if (c == q && s[i - 1] != '\\') inStr = false; }
                else if (c == '"' || c == '\'') { inStr = true; q = c; }
                else if (c == '#') { comment = i; break; }
            }
            if (comment != std::string::npos) end = comment;
            while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t')) --end;
            return end > 0 && s[end - 1] == ':';
        };

        std::string source = line;
        if (opensBlock(line)) {
            std::string more;
            while (std::cout << "... " && std::getline(std::cin, more)) {
                if (more.empty()) break;  // a blank line ends the block
                source += "\n" + more;
            }
        }

        try {
            kirito::Handle result = vm.runRepl(source);
            if (vm.arena().deref(result).kind() != kirito::ValueKind::None)
                std::cout << vm.stringify(result) << "\n";
        } catch (const kirito::KiritoError& e) {
            std::cerr << "error: " << e.what() << " (line " << e.span.line << ":" << e.span.col << ")\n";
        }
    }
    std::cout << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    ConsoleUtf8 utf8Console;  // UTF-8 console for the duration of the program
#endif
    std::vector<std::string> libs;
    std::string file;
    std::vector<std::string> scriptArgs;
    bool warnings = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!file.empty()) {  // everything after the script name belongs to the script
            scriptArgs.push_back(arg);
        } else if (arg == "--lib") {
            if (++i >= argc) { std::cerr << "ki: --lib needs a directory\n"; return 2; }
            libs.emplace_back(argv[i]);
        } else if (arg.rfind("--lib=", 0) == 0) {
            libs.push_back(arg.substr(6));
        } else if (arg == "--no-warn" || arg == "-w") {
            warnings = false;
        } else if (arg == "-h" || arg == "--help") {
            usage();
            return 0;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "ki: unknown option '" << arg << "'\n";
            return 2;
        } else {
            file = arg;
        }
    }

    kirito::KiritoVM vm;
    vm.addLibPath(".");  // current directory is always on the import path
    for (const auto& l : libs) vm.addLibPath(l);

    if (file.empty()) return repl(vm);

    // The script's own directory is also searched for sibling modules.
    std::error_code ec;
    std::filesystem::path scriptPath(file);
    if (scriptPath.has_parent_path()) vm.addLibPath(scriptPath.parent_path().string());

    std::ifstream in(file);
    if (!in) { std::cerr << "ki: cannot open '" << file << "'\n"; return 1; }
    std::stringstream buffer;
    buffer << in.rdbuf();

    vm.setArgs(scriptArgs);  // bound as `arglist` in every module scope

    // Static analysis (non-fatal warnings) before execution: parse once, lint, print to stderr.
    if (warnings) {
        try {
            kirito::Parser parser(kirito::Lexer(buffer.str()).tokenize());
            kirito::ast::Program program = parser.parseProgram();
            kirito::Analyzer analyzer;
            for (const auto& w : kirito::formatWarnings(analyzer.analyze(program), file))
                std::cerr << w << "\n";
        } catch (const kirito::KiritoError&) {
            // A parse error here will be reported (with its span) by runSource below; skip linting.
        }
    }

    try {
        vm.runSource(buffer.str(), file);
    } catch (const kirito::KiritoError& e) {
        // Prefer the file the error was tagged with (e.g. an imported module), falling back to the
        // entry script when the error carries no location of its own.
        const std::string& where = e.file.empty() ? file : e.file;
        std::cerr << where << ":" << e.span.line << ":" << e.span.col << ": error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
