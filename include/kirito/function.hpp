#ifndef KIRITO_FUNCTION_HPP
#define KIRITO_FUNCTION_HPP

#include <functional>
#include <span>
#include <string>
#include <vector>

#include "ast.hpp"
#include "handle.hpp"
#include "object.hpp"

namespace kirito {

// A C++ callable exposed to Kirito: receives its args as handles plus the VM (to allocate
// results / dereference operands) and returns a handle. This is how built-in functions, bound
// methods, and embedder-registered functions all appear to the evaluator — identical to a
// Kirito function.
using NativeFn = std::function<Handle(KiritoVM&, std::span<const Handle>)>;

class NativeFunction : public Object {
public:
    NativeFunction(std::string name, NativeFn fn) : name_(std::move(name)), fn_(std::move(fn)) {}

    ValueKind kind() const override { return ValueKind::NativeFunction; }
    std::string typeName() const override { return "Function"; }
    bool truthy() const override { return true; }
    std::string str(StringifyCtx&) const override { return "<function " + name_ + ">"; }
    bool equals(const ObjectArena&, const Object& other) const override { return this == &other; }
    Handle call(KiritoVM& vm, std::span<const Handle> args) override { return fn_(vm, args); }

private:
    std::string name_;
    NativeFn fn_;
};

// A Kirito-defined function value. It points at its AST definition (owned by the VM, which keeps
// every parsed chunk alive) and captures a handle to its defining scope — that capture is what
// makes closures work and is a GC root via children().
class KiFunction : public Object {
public:
    KiFunction(const ast::FunctionExpr* def, Handle closure) : def_(def), closure_(closure) {}

    const ast::FunctionExpr& def() const { return *def_; }
    Handle closure() const { return closure_; }

    ValueKind kind() const override { return ValueKind::Function; }
    std::string typeName() const override { return "Function"; }
    bool truthy() const override { return true; }
    std::string str(StringifyCtx&) const override { return "<function>"; }
    bool equals(const ObjectArena&, const Object& other) const override { return this == &other; }
    void children(std::vector<Handle>& out) const override { out.push_back(closure_); }

    Handle call(KiritoVM&, std::span<const Handle> args) override;

private:
    const ast::FunctionExpr* def_;
    Handle closure_;
};

}  // namespace kirito

#endif
