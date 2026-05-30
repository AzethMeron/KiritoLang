#ifndef KIRITO_VM_HPP
#define KIRITO_VM_HPP

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "arena.hpp"
#include "builtins.hpp"
#include "environment.hpp"
#include "handle.hpp"
#include "object.hpp"

namespace kirito {

namespace ast { struct Program; }
class NativeModule;

// One KiritoVM == one fully-encapsulated Kirito process. It owns the whole process state by
// composing its sub-objects: the value arena, the global (built-ins) environment, and interned
// singletons. No global/static mutable state, so independent VMs never interfere.
class KiritoVM {
public:
    KiritoVM() {
        none_ = arena_.alloc(std::make_unique<NoneVal>());
        true_ = arena_.alloc(std::make_unique<BoolVal>(true));
        false_ = arena_.alloc(std::make_unique<BoolVal>(false));
        global_ = arena_.alloc(std::make_unique<EnvValue>());
        smallInts_.reserve(kSmallIntHi - kSmallIntLo + 1);
        for (int64_t v = kSmallIntLo; v <= kSmallIntHi; ++v)
            smallInts_.push_back(arena_.alloc(std::make_unique<IntVal>(v)));
        installBuiltins();
        installStandardLibrary();
    }

    ObjectArena& arena() { return arena_; }
    const ObjectArena& arena() const { return arena_; }

    // The GC-aware allocator: every Kirito value flows through here, so a collection can be
    // triggered (before the new object exists) when allocation pressure crosses the threshold.
    Handle alloc(std::unique_ptr<Object> obj) {
        if (gcEnabled_ && ++allocsSinceGc_ >= gcThreshold_) collectGarbage();
        return arena_.alloc(std::move(obj));
    }

    // Interned singletons.
    Handle none() const { return none_; }
    Handle makeBool(bool v) const { return v ? true_ : false_; }

    // Value-construction helpers — also the embedding surface for C++ callers.
    Handle makeInt(int64_t v) {
        if (v >= kSmallIntLo && v <= kSmallIntHi) return smallInts_[v - kSmallIntLo];  // interned
        return alloc(std::make_unique<IntVal>(v));
    }
    Handle makeFloat(double v) { return alloc(std::make_unique<FloatVal>(v)); }
    Handle makeString(std::string v) { return alloc(std::make_unique<StrVal>(std::move(v))); }

    Handle global() const { return global_; }
    // A fresh scope whose parent is the given one (defaults to a module scope under global).
    Handle newScope(Handle parent) { return alloc(std::make_unique<EnvValue>(parent)); }
    Handle newModuleScope() { return newScope(global_); }

    // --- garbage collection ---
    // Temporary roots: handles held in C++ locals across an allocation must be protected here
    // (see RootScope below) so a mid-expression collection cannot reclaim them.
    void pushTemp(Handle h) { tempRoots_.push_back(h); }
    std::size_t tempMark() const { return tempRoots_.size(); }
    void popTempTo(std::size_t mark) { tempRoots_.resize(mark); }

    // Mark from every root (singletons, globals, module cache, REPL scope, temp roots), then sweep.
    void collectGarbage() {
        arena_.clearMarks();
        std::vector<Handle> work;
        auto enqueue = [&](Handle h) { if (arena_.markIfUnmarked(h)) work.push_back(h); };
        enqueue(none_); enqueue(true_); enqueue(false_); enqueue(global_);
        for (Handle h : smallInts_) enqueue(h);
        if (replScopeReady_) enqueue(replScope_);
        for (const auto& [name, h] : moduleCache_) enqueue(h);
        for (Handle h : tempRoots_) enqueue(h);
        std::vector<Handle> childbuf;
        while (!work.empty()) {
            Handle h = work.back();
            work.pop_back();
            childbuf.clear();
            arena_.deref(h).children(childbuf);
            for (Handle c : childbuf) enqueue(c);
        }
        arena_.sweep();
        allocsSinceGc_ = 0;
    }

    void setGcThreshold(std::size_t n) { gcThreshold_ = n; }
    void setGcEnabled(bool on) { gcEnabled_ = on; }
    std::size_t liveCount() const { return arena_.liveCount(); }

    // Expose a C++ callable (or any value) as a Kirito global — the simplest extension point.
    void registerGlobal(const std::string& name, Handle value) {
        static_cast<EnvValue&>(arena_.deref(global_)).define(name, value);
    }

    // --- extension / module API (defined in runtime.hpp) ---
    using ModuleFactory = std::function<Handle(KiritoVM&)>;
    void registerModule(std::string name, ModuleFactory factory);
    template <class T> void install();              // install a NativeModule subclass (one-liner)
    void installStandardLibrary();                  // register the bundled stdlib modules
    Handle importModule(const std::string& name);   // native module, then <name>.ki on the lib path

    // Directories searched (in order) when importing a `.ki` module file.
    void addLibPath(std::string dir) { libPaths_.push_back(std::move(dir)); }
    const std::vector<std::string>& libPaths() const { return libPaths_; }

    std::string stringify(Handle h) const {
        StringifyCtx ctx{arena_, {}};
        return arena_.deref(h).str(ctx);
    }

    // Lex, parse, and evaluate a chunk of Kirito source in a fresh module scope; returns the
    // handle of the last expression's value (or None). Defined in runtime.hpp.
    Handle runSource(std::string_view source, std::string_view chunkName = "<main>");

    // Like runSource but reuses one persistent module scope across calls, so bindings survive
    // between lines — for the REPL. Defined in runtime.hpp.
    Handle runRepl(std::string_view source);

    // Keep a parsed chunk alive for the VM's lifetime so function bodies (referenced by closures
    // that may outlive the call) never dangle. Defined in runtime.hpp (needs a complete Program).
    void retainChunk(std::unique_ptr<ast::Program> chunk);

    // Register built-in globals (len, ...). Defined in runtime.hpp; called from the constructor.
    void installBuiltins();

    // Shared lex->parse->retain->evaluate against a given scope. Defined in runtime.hpp.
    Handle evalIn(std::string_view source, Handle scope);

    ~KiritoVM();  // out-of-line so unique_ptr<ast::Program> sees a complete type

private:
    ObjectArena arena_;
    Handle none_;
    Handle true_;
    Handle false_;
    Handle global_;
    std::vector<std::unique_ptr<ast::Program>> chunks_;
    std::vector<std::unique_ptr<NativeModule>> nativeModules_;
    std::unordered_map<std::string, ModuleFactory> moduleFactories_;
    std::unordered_map<std::string, Handle> moduleCache_;
    std::vector<std::string> libPaths_;
    Handle replScope_{};
    bool replScopeReady_ = false;
    std::vector<Handle> tempRoots_;
    std::vector<Handle> smallInts_;
    static constexpr int64_t kSmallIntLo = -256;
    static constexpr int64_t kSmallIntHi = 256;
    std::size_t allocsSinceGc_ = 0;
    std::size_t gcThreshold_ = 100000;
    bool gcEnabled_ = true;
};

// RAII protector: handles added here are GC roots until the scope ends. Use wherever the
// evaluator holds a handle in a C++ local while doing more work that may allocate.
struct RootScope {
    KiritoVM& vm;
    std::size_t mark;
    explicit RootScope(KiritoVM& vm) : vm(vm), mark(vm.tempMark()) {}
    ~RootScope() { vm.popTempTo(mark); }
    RootScope(const RootScope&) = delete;
    RootScope& operator=(const RootScope&) = delete;
    Handle add(Handle h) {
        vm.pushTemp(h);
        return h;
    }
};

}  // namespace kirito

#endif
