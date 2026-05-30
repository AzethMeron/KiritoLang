#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "kirito.hpp"

// To add a new module to the interpreter: #include its header and call vm.install<Module>().
// (The bundled stdlib is already registered by the VM; this is where a user wires in their own.)
//   #include "mymodule.hpp"
//   vm.install<MyModule>();

namespace {

void usage() {
    std::cout << "kirito (ki) — a Python-like scripting language\n"
                 "usage: ki [options] [file.ki [args...]]\n"
                 "  with no file, starts an interactive REPL\n"
                 "options:\n"
                 "  --lib <dir>   add a directory to the module import path (repeatable)\n"
                 "  -h, --help    show this help\n";
}

// Expose the script's own arguments to Kirito as a global list `argv`.
void setArgv(kirito::KiritoVM& vm, const std::vector<std::string>& args) {
    auto list = std::make_unique<kirito::ListVal>();
    for (const auto& a : args) list->elems.push_back(vm.makeString(a));
    vm.registerGlobal("argv", vm.arena().alloc(std::move(list)));
}

int repl(kirito::KiritoVM& vm) {
    std::cout << "kirito (ki) REPL — Ctrl-D to exit\n";
    std::string line;
    while (std::cout << ">>> " && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        try {
            kirito::Handle result = vm.runRepl(line);
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
    std::vector<std::string> libs;
    std::string file;
    std::vector<std::string> scriptArgs;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!file.empty()) {  // everything after the script name belongs to the script
            scriptArgs.push_back(arg);
        } else if (arg == "--lib") {
            if (++i >= argc) { std::cerr << "ki: --lib needs a directory\n"; return 2; }
            libs.emplace_back(argv[i]);
        } else if (arg.rfind("--lib=", 0) == 0) {
            libs.push_back(arg.substr(6));
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

    setArgv(vm, scriptArgs);
    try {
        vm.runSource(buffer.str(), file);
    } catch (const kirito::KiritoError& e) {
        std::cerr << file << ":" << e.span.line << ":" << e.span.col << ": error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
