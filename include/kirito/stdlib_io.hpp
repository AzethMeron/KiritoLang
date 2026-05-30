#ifndef KIRITO_STDLIB_IO_HPP
#define KIRITO_STDLIB_IO_HPP

#include <iostream>
#include <span>
#include <string>

#include "native.hpp"

namespace kirito {

// The `io` standard module, authored via the extension API exactly as a third party would.
class IoModule : public NativeModule {
public:
    std::string name() const override { return "io"; }

    void setup(ModuleBuilder& m) override {
        m.fn("print", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
            for (std::size_t i = 0; i < args.size(); ++i) {
                if (i) std::cout << ' ';
                std::cout << vm.stringify(args[i]);
            }
            std::cout << '\n';
            return vm.none();
        });
        m.fn("input", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            std::string line;
            std::getline(std::cin, line);
            return vm.makeString(std::move(line));
        });
    }
};

}  // namespace kirito

#endif
