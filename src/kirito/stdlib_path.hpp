#ifndef KIRITO_STDLIB_PATH_HPP
#define KIRITO_STDLIB_PATH_HPP

#include <filesystem>
#include <span>
#include <string>

#include "collections.hpp"
#include "native.hpp"

namespace kirito {

// The `path` module — Kirito's os.path. It is the single home for path operations, so callers never
// have to remember whether a helper lives in `io` or `sys`:
//   * pure path-string manipulation: join / dirname / basename / splitext
//   * read-only filesystem queries about a path: exists / isfile / isdir / getsize
// `io` owns actual I/O and filesystem *mutation* (open/print/input, remove/rename/mkdir/chmod,
// getcwd/listdir/walk); `path` owns everything about interpreting or querying a path.
//
// Path strings use '/' on EVERY platform (results are identical cross-platform, unlike
// std::filesystem's '\' on Windows). A '\' in the input is still accepted as a separator by the
// splitting helpers, so native Windows paths (from io.getcwd/io.listdir) still split correctly.
class PathModule : public NativeModule {
public:
    std::string name() const override { return "path"; }

    void setup(ModuleBuilder& m) override {
        auto pathArg = [](KiritoVM& vm, Handle h) -> std::string { return Value(vm, h).asString("path"); };

        // join(parts...) -> the parts joined with '/'. A later component that is absolute (starts
        // with '/') resets the result. Like os.path.join it needs at least one component (raises
        // otherwise). Variadic. (A leading '\' is NOT treated as absolute — only '/' resets.)
        m.fn("join", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "join");
            if (args.empty()) throw KiritoError("join expected at least one path component");
            std::string out = args[0].asString("join");
            for (std::size_t i = 1; i < a.size(); ++i) {
                std::string part = args[i].asString("join");
                if (!part.empty() && part[0] == '/') out = part;            // absolute resets
                else if (out.empty() || out.back() == '/') out += part;     // no double separator
                else out += "/" + part;
            }
            return val(vm, out);
        });

        // dirname/basename/splitext are plain '/'- or '\'-based string ops (NOT std::filesystem,
        // whose separator and semantics are platform-dependent). They return literal substrings of
        // the input — they do not rewrite separators — so only basename (the final component) is
        // guaranteed free of a backslash; a '\' already inside a retained prefix is preserved.
        m.fn("dirname", {{"path", "String"}}, "String", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::string p = pathArg(vm, a[0]);
            std::size_t s = p.find_last_of("/\\");
            if (s == std::string::npos) return val(vm, std::string());
            std::size_t end = s;   // strip the separator run back to the parent (os.path.dirname)
            while (end > 0 && (p[end - 1] == '/' || p[end - 1] == '\\')) --end;
            // an all-separator prefix is the root: keep one separator (dirname("/a") -> "/") rather
            // than dropping it to "".
            return val(vm, end == 0 ? p.substr(0, s + 1) : p.substr(0, end));
        });
        m.fn("basename", {{"path", "String"}}, "String", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::string p = pathArg(vm, a[0]);
            std::size_t s = p.find_last_of("/\\");
            return val(vm, s == std::string::npos ? p : p.substr(s + 1));
        });
        m.fn("splitext", {{"path", "String"}}, "List", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::string path = pathArg(vm, a[0]);
            std::size_t sep = path.find_last_of("/\\");
            std::size_t baseStart = (sep == std::string::npos) ? 0 : sep + 1;
            // an extension dot must come AFTER at least one non-dot char in the final component, so a
            // whole leading run of dots is skipped — ".bashrc"/".."/"...x" have no extension, matching
            // os.path.splitext (its rule protects the leading dot *run*, not just the first char).
            std::size_t scan = baseStart;
            while (scan < path.size() && path[scan] == '.') ++scan;
            std::size_t dot = path.find_last_of('.');
            if (dot == std::string::npos || dot < scan)
                return List(vm).add(val(vm, path)).add(val(vm, std::string())).build();
            return List(vm).add(val(vm, path.substr(0, dot))).add(val(vm, path.substr(dot))).build();
        });

        // Filesystem queries. exists/isfile/isdir are tolerant (a missing/inaccessible path is simply
        // False, never a raise); getsize raises on a missing/non-regular file (there is no sensible
        // size to return).
        m.fn("exists", {{"path", "String"}}, "Bool", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            return val(vm, std::filesystem::exists(pathArg(vm, a[0]), ec));
        });
        m.fn("isfile", {{"path", "String"}}, "Bool", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            return val(vm, std::filesystem::is_regular_file(pathArg(vm, a[0]), ec));
        });
        m.fn("isdir", {{"path", "String"}}, "Bool", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            return val(vm, std::filesystem::is_directory(pathArg(vm, a[0]), ec));
        });
        m.fn("getsize", {{"path", "String"}}, "Integer", [pathArg](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::error_code ec;
            auto sz = std::filesystem::file_size(pathArg(vm, a[0]), ec);
            if (ec) throw KiritoError("getsize: " + ec.message());
            return val(vm, static_cast<int64_t>(sz));
        });
    }
};

}  // namespace kirito

#endif
