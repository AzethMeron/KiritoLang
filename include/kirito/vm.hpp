#ifndef KIRITO_VM_HPP
#define KIRITO_VM_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "arena.hpp"
#include "builtins.hpp"
#include "environment.hpp"
#include "handle.hpp"
#include "object.hpp"

namespace kirito {

namespace ast { struct Program; }

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
    }

    ObjectArena& arena() { return arena_; }
    const ObjectArena& arena() const { return arena_; }

    // Interned singletons.
    Handle none() const { return none_; }
    Handle makeBool(bool v) const { return v ? true_ : false_; }

    // Value-construction helpers — also the embedding surface for C++ callers.
    Handle makeInt(int64_t v) { return arena_.alloc(std::make_unique<IntVal>(v)); }
    Handle makeFloat(double v) { return arena_.alloc(std::make_unique<FloatVal>(v)); }
    Handle makeString(std::string v) { return arena_.alloc(std::make_unique<StrVal>(std::move(v))); }

    Handle global() const { return global_; }
    // A fresh scope whose parent is the given one (defaults to a module scope under global).
    Handle newScope(Handle parent) { return arena_.alloc(std::make_unique<EnvValue>(parent)); }
    Handle newModuleScope() { return newScope(global_); }

    std::string stringify(Handle h) const {
        StringifyCtx ctx{arena_, {}};
        return arena_.deref(h).str(ctx);
    }

    // Lex, parse, and evaluate a chunk of Kirito source in a fresh module scope; returns the
    // handle of the last expression's value (or None). Defined in runtime.hpp.
    Handle runSource(std::string_view source, std::string_view chunkName = "<main>");

    // Keep a parsed chunk alive for the VM's lifetime so function bodies (referenced by closures
    // that may outlive the call) never dangle. Defined in runtime.hpp (needs a complete Program).
    void retainChunk(std::unique_ptr<ast::Program> chunk);

    ~KiritoVM();  // out-of-line so unique_ptr<ast::Program> sees a complete type

private:
    ObjectArena arena_;
    Handle none_;
    Handle true_;
    Handle false_;
    Handle global_;
    std::vector<std::unique_ptr<ast::Program>> chunks_;
};

}  // namespace kirito

#endif
