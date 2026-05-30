#ifndef KIRITO_STDLIB_IO_HPP
#define KIRITO_STDLIB_IO_HPP

#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"

namespace kirito {

// A file/stream object. Holds an fstream (closed automatically when the value is collected) and
// exposes read/readline/write/close plus enter/exit so it works as a `with` context manager.
class FileVal : public NativeClass<FileVal> {
public:
    static constexpr const char* kTypeName = "File";
    std::fstream stream;
    std::string path;

    FileVal(const std::string& p, std::ios::openmode mode) : path(p) { stream.open(p, mode); }

    // Iterating a file yields its remaining lines (so `for line in f:` works).
    std::optional<std::vector<Handle>> iterate(KiritoVM& vm) override {
        RootScope rs(vm);
        std::vector<Handle> lines;
        std::string line;
        while (std::getline(stream, line)) lines.push_back(rs.add(vm.makeString(line)));
        return lines;
    }

    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override {
        auto bind = [&](const char* nm, NativeFn fn) {
            return vm.alloc(std::make_unique<NativeFunction>(nm, std::move(fn), std::vector<Handle>{self}));
        };
        auto file = [](KiritoVM& vm, Handle self) -> FileVal& {
            return static_cast<FileVal&>(vm.arena().deref(self));
        };
        if (name == "read")
            return bind("read", [self, file](KiritoVM& vm, std::span<const Handle>) {
                std::stringstream ss;
                ss << file(vm, self).stream.rdbuf();
                return vm.makeString(ss.str());
            });
        if (name == "readline")
            return bind("readline", [self, file](KiritoVM& vm, std::span<const Handle>) {
                std::string line;
                std::getline(file(vm, self).stream, line);
                return vm.makeString(std::move(line));
            });
        if (name == "write")
            return bind("write", [self, file](KiritoVM& vm, std::span<const Handle> a) {
                const Object& o = vm.arena().deref(a[0]);
                if (o.kind() != ValueKind::String) throw KiritoError("write expects a String");
                file(vm, self).stream << static_cast<const StrVal&>(o).value();
                return vm.none();
            });
        if (name == "close" || name == "_exit_")
            return bind("close", [self, file](KiritoVM& vm, std::span<const Handle>) {
                file(vm, self).stream.close();
                return vm.none();
            });
        if (name == "readlines")
            return bind("readlines", [self, file](KiritoVM& vm, std::span<const Handle>) -> Handle {
                RootScope rs(vm);
                auto list = std::make_unique<ListVal>();
                std::string line;
                while (std::getline(file(vm, self).stream, line)) list->elems.push_back(rs.add(vm.makeString(line)));
                return vm.alloc(std::move(list));
            });
        if (name == "writelines")
            return bind("writelines", [self, file](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto items = vm.arena().deref(a[0]).iterate(vm);
                auto& f = file(vm, self);
                for (Handle h : items.value()) {
                    const Object& o = vm.arena().deref(h);
                    if (o.kind() != ValueKind::String) throw KiritoError("writelines expects Strings");
                    f.stream << static_cast<const StrVal&>(o).value();
                }
                return vm.none();
            });
        if (name == "flush")
            return bind("flush", [self, file](KiritoVM& vm, std::span<const Handle>) -> Handle {
                file(vm, self).stream.flush();
                return vm.none();
            });
        if (name == "tell")
            return bind("tell", [self, file](KiritoVM& vm, std::span<const Handle>) -> Handle {
                return vm.makeInt(static_cast<int64_t>(file(vm, self).stream.tellg()));
            });
        if (name == "seek")
            return bind("seek", [self, file](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                int64_t pos = static_cast<const IntVal&>(vm.arena().deref(a[0])).value();
                auto& s = file(vm, self).stream;
                s.clear();
                s.seekg(pos);
                s.seekp(pos);
                return vm.none();
            });
        if (name == "_enter_")
            return bind("_enter_", [self](KiritoVM&, std::span<const Handle>) { return self; });
        return Object::getAttr(vm, self, name);
    }
};

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
        m.fn("write", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
            for (Handle h : args) std::cout << vm.stringify(h);
            return vm.none();
        });
        m.fn("open", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.empty() || a.size() > 2) throw KiritoError("open expected 1 or 2 arguments");
            const Object& po = vm.arena().deref(a[0]);
            if (po.kind() != ValueKind::String) throw KiritoError("open path must be a String");
            std::string path = static_cast<const StrVal&>(po).value();
            std::string mode = "r";
            if (a.size() == 2) {
                const Object& mo = vm.arena().deref(a[1]);
                if (mo.kind() != ValueKind::String) throw KiritoError("open mode must be a String");
                mode = static_cast<const StrVal&>(mo).value();
            }
            std::ios::openmode flags{};
            if (mode == "r") flags = std::ios::in;
            else if (mode == "w") flags = std::ios::out | std::ios::trunc;
            else if (mode == "a") flags = std::ios::out | std::ios::app;
            else if (mode == "r+") flags = std::ios::in | std::ios::out;
            else throw KiritoError("unsupported file mode '" + mode + "'");
            auto f = std::make_unique<FileVal>(path, flags);
            if (!f->stream.is_open()) throw KiritoError("could not open file '" + path + "'");
            return vm.alloc(std::move(f));
        });

        auto pathArg = [](KiritoVM& vm, Handle h) -> std::string {
            const Object& o = vm.arena().deref(h);
            if (o.kind() != ValueKind::String) throw KiritoError("expected a path String");
            return static_cast<const StrVal&>(o).value();
        };
        m.fn("exists", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            return vm.makeBool(std::filesystem::exists(pathArg(vm, a[0]), ec));
        });
        m.fn("remove", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            std::filesystem::remove(pathArg(vm, a[0]), ec);
            return vm.makeBool(!ec);
        });
        m.fn("rename", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            std::filesystem::rename(pathArg(vm, a[0]), pathArg(vm, a[1]), ec);
            if (ec) throw KiritoError("rename failed: " + ec.message());
            return vm.none();
        });
        m.fn("mkdir", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            std::filesystem::create_directories(pathArg(vm, a[0]), ec);
            return vm.makeBool(!ec);
        });
        m.fn("getcwd", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            std::error_code ec;
            return vm.makeString(std::filesystem::current_path(ec).string());
        });
        m.fn("listdir", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            RootScope rs(vm);
            auto list = std::make_unique<ListVal>();
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(pathArg(vm, a[0]), ec))
                list->elems.push_back(rs.add(vm.makeString(entry.path().filename().string())));
            return vm.alloc(std::move(list));
        });
    }
};

}  // namespace kirito

#endif
