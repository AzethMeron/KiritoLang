#ifndef KIRITO_STDLIB_IO_HPP
#define KIRITO_STDLIB_IO_HPP

#include <algorithm>
#include <array>
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

// A uniform byte/character-stream interface implemented by every stream value (File, BytesIO, and
// the standard streams). It is what makes streams *interchangeable*: io.print/write/input/read act
// on whatever object is currently bound to io.stdout / io.stdin, dispatching through this interface
// — so redirecting output to a file (or a BytesIO, or back to the terminal) is just a rebinding.
// A value that is not an IoStream can still serve as stdout/stdin via its `write`/`readline` methods
// (duck typing); this interface is the fast, built-in path.
struct IoStream {
    virtual ~IoStream() = default;
    virtual void streamWrite(const std::string&) { throw KiritoError("stream is not writable"); }
    virtual std::string streamRead(std::optional<std::size_t>) { throw KiritoError("stream is not readable"); }
    virtual std::string streamReadLine() { throw KiritoError("stream is not readable"); }
    virtual void streamFlush() {}
};

// A file/stream object. Holds an fstream (closed automatically when the value is collected) and
// exposes read/readline/write/close plus enter/exit so it works as a `with` context manager.
class FileVal : public NativeClass<FileVal>, public IoStream {
public:
    static constexpr const char* kTypeName = "File";
    std::fstream stream;
    std::string path;

    FileVal(const std::string& p, std::ios::openmode mode) : path(p) { stream.open(p, mode); }

    void streamWrite(const std::string& s) override { stream << s; }
    std::string streamRead(std::optional<std::size_t> n) override {
        if (!n) { std::stringstream ss; ss << stream.rdbuf(); return ss.str(); }
        std::string buf(*n, '\0');
        stream.read(buf.data(), static_cast<std::streamsize>(*n));
        buf.resize(static_cast<std::size_t>(stream.gcount()));
        return buf;
    }
    std::string streamReadLine() override { std::string line; std::getline(stream, line); return line; }
    void streamFlush() override { stream.flush(); }

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
            return bind("read", [self, file](KiritoVM& vm, std::span<const Handle> a) {
                std::optional<std::size_t> n;
                if (!a.empty()) {
                    int64_t want = argInt(vm, a[0], "read");
                    if (want >= 0) n = static_cast<std::size_t>(want);
                }
                return vm.makeString(file(vm, self).streamRead(n));
            });
        if (name == "readline")
            return bind("readline", [self, file](KiritoVM& vm, std::span<const Handle>) {
                return vm.makeString(file(vm, self).streamReadLine());
            });
        if (name == "write")
            return bind("write", [self, file](KiritoVM& vm, std::span<const Handle> a) {
                file(vm, self).streamWrite(argString(vm, a[0], "write"));
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
class BytesIO : public NativeClass<BytesIO>, public IoStream {
public:
    static constexpr const char* kTypeName = "BytesIO";
    std::string buf;
    std::size_t pos = 0;

    BytesIO() = default;
    explicit BytesIO(std::string initial) : buf(std::move(initial)) {}

    std::string str(StringifyCtx&) const override {
        return "BytesIO(" + std::to_string(buf.size()) + " bytes)";
    }

    void streamWrite(const std::string& data) override {
        if (pos + data.size() > buf.size()) buf.resize(pos + data.size());  // overwrite-at-cursor
        std::copy(data.begin(), data.end(), buf.begin() + static_cast<std::ptrdiff_t>(pos));
        pos += data.size();
    }
    std::string streamRead(std::optional<std::size_t> n) override {
        std::size_t avail = buf.size() - pos;
        std::size_t take = n ? std::min(avail, *n) : avail;
        std::string out = buf.substr(pos, take);
        pos += out.size();
        return out;
    }
    std::string streamReadLine() override {
        std::size_t nl = buf.find('\n', pos);
        std::size_t end = (nl == std::string::npos) ? buf.size() : nl + 1;  // include the newline
        std::string out = buf.substr(pos, end - pos);
        pos = end;
        if (!out.empty() && out.back() == '\n') out.pop_back();  // strip trailing newline, like getline
        return out;
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
                const std::string& data = argString(vm, a[0], "BytesIO.write");
                io(vm, self).streamWrite(data);
                return vm.makeInt(static_cast<int64_t>(data.size()));
            });
        if (name == "read")
            return bind("read", [self, io](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                std::optional<std::size_t> n;
                if (!a.empty()) {
                    const Object& o = vm.arena().deref(a[0]);
                    if (o.kind() == ValueKind::Integer) {
                        int64_t want = static_cast<const IntVal&>(o).value();
                        if (want >= 0) n = static_cast<std::size_t>(want);
                    }
                }
                return vm.makeString(io(vm, self).streamRead(n));
            });
        if (name == "readline")
            return bind("readline", [self, io](KiritoVM& vm, std::span<const Handle>) -> Handle {
                return vm.makeString(io(vm, self).streamReadLine());
            });
        if (name == "flush")
            return bind("flush", [](KiritoVM& vm, std::span<const Handle>) -> Handle { return vm.none(); });
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

// A handle to one of the process's standard streams (stdout/stderr/stdin), wearing the same stream
// protocol as File and BytesIO so the three are interchangeable as io.print/input targets. Closing
// is a no-op — we never close the real cout/cin. Output streams reject reads and vice-versa.
class StdStream : public NativeClass<StdStream>, public IoStream {
public:
    static constexpr const char* kTypeName = "StdStream";
    enum class Dir { Out, Err, In };
    Dir dir;
    explicit StdStream(Dir d) : dir(d) {}

    std::string str(StringifyCtx&) const override {
        return dir == Dir::Out ? "<stdout>" : dir == Dir::Err ? "<stderr>" : "<stdin>";
    }
    void streamWrite(const std::string& s) override {
        if (dir == Dir::In) throw KiritoError("stdin is not writable");
        (dir == Dir::Err ? std::cerr : std::cout) << s;
    }
    void streamFlush() override {
        if (dir == Dir::Err) std::cerr.flush();
        else if (dir == Dir::Out) std::cout.flush();
    }
    std::string streamRead(std::optional<std::size_t> n) override {
        if (dir != Dir::In) throw KiritoError("a write stream is not readable");
        if (!n) { std::stringstream ss; ss << std::cin.rdbuf(); return ss.str(); }
        std::string buf(*n, '\0');
        std::cin.read(buf.data(), static_cast<std::streamsize>(*n));
        buf.resize(static_cast<std::size_t>(std::cin.gcount()));
        return buf;
    }
    std::string streamReadLine() override {
        if (dir != Dir::In) throw KiritoError("a write stream is not readable");
        std::string line; std::getline(std::cin, line); return line;
    }

    // `for line in io.stdin:` reads stdin line-by-line.
    std::optional<std::vector<Handle>> iterate(KiritoVM& vm) override {
        if (dir != Dir::In) return std::nullopt;
        RootScope rs(vm);
        std::vector<Handle> lines; std::string line;
        while (std::getline(std::cin, line)) lines.push_back(rs.add(vm.makeString(line)));
        return lines;
    }

    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override {
        auto bind = [&](const char* nm, NativeFn fn) {
            return vm.alloc(std::make_unique<NativeFunction>(nm, std::move(fn), std::vector<Handle>{self}));
        };
        auto me = [](KiritoVM& vm, Handle self) -> StdStream& { return static_cast<StdStream&>(vm.arena().deref(self)); };
        if (name == "write")
            return bind("write", [self, me](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                me(vm, self).streamWrite(argString(vm, a[0], "write"));
                return vm.none();
            });
        if (name == "read")
            return bind("read", [self, me](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                std::optional<std::size_t> n;
                if (!a.empty()) { int64_t w = argInt(vm, a[0], "read"); if (w >= 0) n = static_cast<std::size_t>(w); }
                return vm.makeString(me(vm, self).streamRead(n));
            });
        if (name == "readline")
            return bind("readline", [self, me](KiritoVM& vm, std::span<const Handle>) -> Handle {
                return vm.makeString(me(vm, self).streamReadLine());
            });
        if (name == "flush")
            return bind("flush", [self, me](KiritoVM& vm, std::span<const Handle>) -> Handle {
                me(vm, self).streamFlush(); return vm.none();
            });
        if (name == "_enter_")
            return bind("_enter_", [self](KiritoVM&, std::span<const Handle>) { return self; });
        if (name == "_exit_" || name == "close")
            return bind("close", [](KiritoVM& vm, std::span<const Handle>) { return vm.none(); });
        return Object::getAttr(vm, self, name);
    }
};

// Route output/input through whatever value is currently bound to a stream slot. The built-in stream
// types take the fast IoStream path; anything else (e.g. a user class) is duck-typed via its
// write / readline / read methods, so it can serve as stdout/stdin too.
inline void streamWriteTo(KiritoVM& vm, Handle target, const std::string& s, bool flush) {
    Object& o = vm.arena().deref(target);
    if (auto* st = dynamic_cast<IoStream*>(&o)) {
        st->streamWrite(s);
        if (flush) st->streamFlush();
        return;
    }
    RootScope rs(vm);
    Handle wf = rs.add(o.getAttr(vm, target, "write"));
    std::array<Handle, 1> args{rs.add(vm.makeString(s))};
    vm.arena().deref(wf).call(vm, args);
}
inline std::string streamReadLineFrom(KiritoVM& vm, Handle target) {
    Object& o = vm.arena().deref(target);
    if (auto* st = dynamic_cast<IoStream*>(&o)) return st->streamReadLine();
    RootScope rs(vm);
    Handle rf = rs.add(o.getAttr(vm, target, "readline"));
    return argString(vm, vm.arena().deref(rf).call(vm, {}), "readline");
}
inline std::string streamReadFrom(KiritoVM& vm, Handle target, std::optional<std::size_t> n) {
    Object& o = vm.arena().deref(target);
    if (auto* st = dynamic_cast<IoStream*>(&o)) return st->streamRead(n);
    RootScope rs(vm);
    Handle rf = rs.add(o.getAttr(vm, target, "read"));
    if (n) { std::array<Handle, 1> a{rs.add(vm.makeInt(static_cast<int64_t>(*n)))};
             return argString(vm, vm.arena().deref(rf).call(vm, a), "read"); }
    return argString(vm, vm.arena().deref(rf).call(vm, {}), "read");
}

// The `io` standard module, authored via the extension API exactly as a third party would.
class IoModule : public NativeModule {
public:
    std::string name() const override { return "io"; }

    void setup(ModuleBuilder& m) override {
        KiritoVM& vm = m.vm();
        // The three standard streams, as rebindable module members. io.print / write / input / read
        // act on whatever io.stdout / io.stdin currently hold, so `io.stdout = io.open("log","w")`
        // redirects output to a file (and rebinding back, or to io.stderr, restores it).
        // __stdout__/__stderr__/__stdin__ keep the originals (like Python) so you can always rebind
        // back to the terminal: `io.stdout = io.__stdout__`.
        Handle out = vm.alloc(std::make_unique<StdStream>(StdStream::Dir::Out));
        Handle err = vm.alloc(std::make_unique<StdStream>(StdStream::Dir::Err));
        Handle in = vm.alloc(std::make_unique<StdStream>(StdStream::Dir::In));
        m.value("stdout", out).value("__stdout__", out);
        m.value("stderr", err).value("__stderr__", err);
        m.value("stdin", in).value("__stdin__", in);
        Handle modH = m.moduleHandle();
        // Resolve a stream slot's *current* binding (looked up fresh each call, so reassignment is
        // honoured). The three slots are always present, so .at() can't miss.
        auto slot = [modH](KiritoVM& vm, const char* name) -> Handle {
            return static_cast<ModuleValue&>(vm.arena().deref(modH)).members.at(name);
        };

        // BytesIO([initial]) -> an in-memory binary buffer.
        m.fn("BytesIO", {{"initial", "String", vm.makeString("")}}, "BytesIO",
             [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.empty()) return vm.alloc(std::make_unique<BytesIO>());
            const Object& o = vm.arena().deref(a[0]);
            if (o.kind() != ValueKind::String) throw KiritoError("BytesIO expects a byte String");
            return vm.alloc(std::make_unique<BytesIO>(static_cast<const StrVal&>(o).value()));
        });
        // print(*args): space-separated, newline-terminated, flushed — to the current stdout.
        m.fn("print", [slot](KiritoVM& vm, std::span<const Handle> args) -> Handle {
            std::string line;
            for (std::size_t i = 0; i < args.size(); ++i) {
                if (i) line += ' ';
                line += vm.stringify(args[i]);
            }
            line += '\n';
            // Flush so output is visible immediately on a pipe/file (a server's readiness banner,
            // progress logs) — not stuck in a fully-buffered block until exit.
            streamWriteTo(vm, slot(vm, "stdout"), line, /*flush=*/true);
            return vm.none();
        });
        // eprint(*args): like print, but to the current stderr (unbuffered by convention).
        m.fn("eprint", [slot](KiritoVM& vm, std::span<const Handle> args) -> Handle {
            std::string line;
            for (std::size_t i = 0; i < args.size(); ++i) {
                if (i) line += ' ';
                line += vm.stringify(args[i]);
            }
            line += '\n';
            streamWriteTo(vm, slot(vm, "stderr"), line, /*flush=*/true);
            return vm.none();
        });
        // input([prompt]): optionally write a prompt to stdout, then read one line from stdin.
        m.fn("input", [slot](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (!a.empty()) streamWriteTo(vm, slot(vm, "stdout"), vm.stringify(a[0]), /*flush=*/true);
            return vm.makeString(streamReadLineFrom(vm, slot(vm, "stdin")));
        });
        // read([n]): read n characters (or everything) from the current stdin.
        m.fn("read", [slot](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::optional<std::size_t> n;
            if (!a.empty()) { int64_t w = argInt(vm, a[0], "read"); if (w >= 0) n = static_cast<std::size_t>(w); }
            return vm.makeString(streamReadFrom(vm, slot(vm, "stdin"), n));
        });
        // write(*args): raw, no separator, no newline — to the current stdout.
        m.fn("write", [slot](KiritoVM& vm, std::span<const Handle> args) -> Handle {
            std::string out;
            for (Handle h : args) out += vm.stringify(h);
            streamWriteTo(vm, slot(vm, "stdout"), out, /*flush=*/false);
            return vm.none();
        });
        m.fn("open", {{"path", "String"}, {"mode", "String", vm.makeString("r")}}, "File",
             [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
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

        auto pathArg = [](KiritoVM& vm, Handle h) -> std::string { return Value(vm, h).asString("path"); };
        m.fn("exists", {{"path", "String"}}, "Bool", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            return val(vm, std::filesystem::exists(pathArg(vm, a[0]), ec));
        });
        m.fn("remove", {{"path", "String"}}, "Bool", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            std::filesystem::remove(pathArg(vm, a[0]), ec);
            return val(vm, !ec);
        });
        m.fn("rename", {{"src", "String"}, {"dst", "String"}}, "", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            std::filesystem::rename(pathArg(vm, a[0]), pathArg(vm, a[1]), ec);
            if (ec) throw KiritoError("rename failed: " + ec.message());
            return none(vm);
        });
        m.fn("mkdir", {{"path", "String"}}, "Bool", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            std::filesystem::create_directories(pathArg(vm, a[0]), ec);
            return val(vm, !ec);
        });
        m.fn("getcwd", {}, "String", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            std::error_code ec;
            return val(vm, std::filesystem::current_path(ec).string());
        });
        m.fn("gettempdir", {}, "String", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            // The system temp directory (honors TMPDIR/TMP/TEMP, falls back to /tmp), like
            // Python's tempfile.gettempdir — a stable scratch location for files.
            std::error_code ec;
            auto p = std::filesystem::temp_directory_path(ec);
            return val(vm, ec ? std::string("/tmp") : p.string());
        });
        m.fn("listdir", {{"path", "String"}}, "List", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            List out(vm);
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(pathArg(vm, a[0]), ec))
                out.add(val(vm, entry.path().filename().string()));
            return out.build();
        });

        // --- path helpers (os.path style) ---
        m.fn("isfile", {{"path", "String"}}, "Bool", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            return val(vm, std::filesystem::is_regular_file(pathArg(vm, a[0]), ec));
        });
        m.fn("isdir", {{"path", "String"}}, "Bool", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            return val(vm, std::filesystem::is_directory(pathArg(vm, a[0]), ec));
        });
        m.fn("dirname", {{"path", "String"}}, "String", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, std::filesystem::path(pathArg(vm, a[0])).parent_path().string());
        });
        m.fn("basename", {{"path", "String"}}, "String", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return val(vm, std::filesystem::path(pathArg(vm, a[0])).filename().string());
        });
        m.fn("splitext", {{"path", "String"}}, "List", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::string path = pathArg(vm, a[0]);
            std::string ext = std::filesystem::path(path).extension().string();
            return List(vm).add(val(vm, path.substr(0, path.size() - ext.size()))).add(val(vm, ext)).build();
        });
        // join(parts...) -> path with the platform separator (uses filesystem's operator/).
        m.fn("join", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            if (a.empty()) return val(vm, "");
            std::filesystem::path p(pathArg(vm, a[0]));
            for (std::size_t i = 1; i < a.size(); ++i) p /= pathArg(vm, a[i]);
            return val(vm, p.string());
        });
        m.fn("getsize", {{"path", "String"}}, "Integer", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            auto sz = std::filesystem::file_size(pathArg(vm, a[0]), ec);
            if (ec) throw KiritoError("getsize: " + ec.message());
            return val(vm, static_cast<int64_t>(sz));
        });
        // walk(dir) -> List of file paths under dir, recursively (flattened; simpler than Python's
        // (dirpath, dirnames, filenames) triples but covers the common "visit every file" case).
        m.fn("walk", {{"dir", "String"}}, "List", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            List out(vm);
            std::error_code ec;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(pathArg(vm, a[0]), ec))
                if (entry.is_regular_file()) out.add(val(vm, entry.path().string()));
            return out.build();
        });
    }
};

}  // namespace kirito

#endif
