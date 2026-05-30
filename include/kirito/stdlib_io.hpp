#ifndef KIRITO_STDLIB_IO_HPP
#define KIRITO_STDLIB_IO_HPP

#include <algorithm>
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

// An in-memory binary buffer, like Python's io.BytesIO: an efficient growable byte buffer with a
// read/write cursor. write() appends/overwrites at the cursor; read() consumes from it; getvalue()
// returns the whole contents as a byte String. Useful as a sink for code that "writes a file"
// (e.g. encoding an image) without touching disk.
class BytesIO : public NativeClass<BytesIO> {
public:
    static constexpr const char* kTypeName = "BytesIO";
    std::string buf;
    std::size_t pos = 0;

    BytesIO() = default;
    explicit BytesIO(std::string initial) : buf(std::move(initial)) {}

    std::string str(StringifyCtx&) const override {
        return "BytesIO(" + std::to_string(buf.size()) + " bytes)";
    }

    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override {
        auto bind = [&](const char* nm, NativeFn fn) {
            return vm.alloc(std::make_unique<NativeFunction>(nm, std::move(fn), std::vector<Handle>{self}));
        };
        auto io = [](KiritoVM& vm, Handle self) -> BytesIO& {
            return static_cast<BytesIO&>(vm.arena().deref(self));
        };
        if (name == "write")
            return bind("write", [self, io](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                const Object& o = vm.arena().deref(a[0]);
                if (o.kind() != ValueKind::String) throw KiritoError("BytesIO.write expects a byte String");
                const std::string& data = static_cast<const StrVal&>(o).value();
                auto& b = io(vm, self);
                // overwrite at the cursor, extending the buffer as needed (Python semantics)
                if (b.pos + data.size() > b.buf.size()) b.buf.resize(b.pos + data.size());
                std::copy(data.begin(), data.end(), b.buf.begin() + static_cast<std::ptrdiff_t>(b.pos));
                b.pos += data.size();
                return vm.makeInt(static_cast<int64_t>(data.size()));
            });
        if (name == "read")
            return bind("read", [self, io](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto& b = io(vm, self);
                std::size_t n = b.buf.size() - b.pos;  // read all remaining by default
                if (!a.empty()) {
                    const Object& o = vm.arena().deref(a[0]);
                    if (o.kind() == ValueKind::Integer) {
                        int64_t want = static_cast<const IntVal&>(o).value();
                        if (want >= 0) n = std::min<std::size_t>(n, static_cast<std::size_t>(want));
                    }
                }
                std::string out = b.buf.substr(b.pos, n);
                b.pos += out.size();
                return vm.makeString(std::move(out));
            });
        if (name == "getvalue")
            return bind("getvalue", [self, io](KiritoVM& vm, std::span<const Handle>) -> Handle {
                return vm.makeString(io(vm, self).buf);
            });
        if (name == "tell")
            return bind("tell", [self, io](KiritoVM& vm, std::span<const Handle>) -> Handle {
                return vm.makeInt(static_cast<int64_t>(io(vm, self).pos));
            });
        if (name == "seek")
            return bind("seek", [self, io](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto& b = io(vm, self);
                int64_t off = static_cast<const IntVal&>(vm.arena().deref(a[0])).value();
                int whence = a.size() > 1 ? static_cast<int>(static_cast<const IntVal&>(vm.arena().deref(a[1])).value()) : 0;
                int64_t base = whence == 1 ? static_cast<int64_t>(b.pos)
                             : whence == 2 ? static_cast<int64_t>(b.buf.size()) : 0;
                int64_t np = base + off;
                if (np < 0) np = 0;
                b.pos = static_cast<std::size_t>(np);
                return vm.makeInt(np);
            });
        if (name == "size" || name == "_len_")
            return bind("size", [self, io](KiritoVM& vm, std::span<const Handle>) -> Handle {
                return vm.makeInt(static_cast<int64_t>(io(vm, self).buf.size()));
            });
        if (name == "truncate")
            return bind("truncate", [self, io](KiritoVM& vm, std::span<const Handle>) -> Handle {
                auto& b = io(vm, self);
                b.buf.resize(b.pos);
                return vm.makeInt(static_cast<int64_t>(b.buf.size()));
            });
        if (name == "_enter_")
            return bind("_enter_", [self](KiritoVM&, std::span<const Handle>) { return self; });
        if (name == "_exit_" || name == "close")
            return bind("close", [](KiritoVM& vm, std::span<const Handle>) { return vm.none(); });
        return Object::getAttr(vm, self, name);
    }
};

// The `io` standard module, authored via the extension API exactly as a third party would.
class IoModule : public NativeModule {
public:
    std::string name() const override { return "io"; }

    void setup(ModuleBuilder& m) override {
        // BytesIO([initial]) -> an in-memory binary buffer.
        m.fn("BytesIO", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.empty()) return vm.alloc(std::make_unique<BytesIO>());
            const Object& o = vm.arena().deref(a[0]);
            if (o.kind() != ValueKind::String) throw KiritoError("BytesIO expects a byte String");
            return vm.alloc(std::make_unique<BytesIO>(static_cast<const StrVal&>(o).value()));
        });
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
