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
    NativeFunction(std::string name, NativeFn fn, std::vector<Handle> captures = {})
        : name_(std::move(name)), fn_(std::move(fn)), captures_(std::move(captures)) {}

    ValueKind kind() const override { return ValueKind::NativeFunction; }
    std::string typeName() const override { return "Function"; }
    bool truthy() const override { return true; }
    std::string str(StringifyCtx&) const override { return "<function " + name_ + ">"; }
    bool equals(const ObjectArena&, const Object& other) const override { return this == &other; }
    Handle call(KiritoVM& vm, std::span<const Handle> args) override { return fn_(vm, args); }
    // Bound methods capture their receiver here so the GC keeps it alive while the method exists.
    void children(std::vector<Handle>& out) const override {
        out.insert(out.end(), captures_.begin(), captures_.end());
    }

private:
    std::string name_;
    NativeFn fn_;
    std::vector<Handle> captures_;
};

// A Kirito-defined function value. It points at its AST definition (owned by the VM, which keeps
// every parsed chunk alive) and captures a handle to its defining scope — that capture is what
// makes closures work and is a GC root via children().
class KiFunction : public Object {
public:
    KiFunction(const ast::FunctionExpr* def, Handle closure) : def_(def), closure_(closure) {}

    const ast::FunctionExpr& def() const { return *def_; }
    Handle closure() const { return closure_; }

    // When this function is a class method, the class it belongs to (so its body may access
    // private members of instances of that class). hasOwner is false for plain functions.
    Handle ownerClass{};
    bool hasOwner = false;

    ValueKind kind() const override { return ValueKind::Function; }
    std::string typeName() const override { return "Function"; }
    bool truthy() const override { return true; }
    std::string str(StringifyCtx&) const override { return "<function>"; }
    bool equals(const ObjectArena&, const Object& other) const override { return this == &other; }
    void children(std::vector<Handle>& out) const override {
        out.push_back(closure_);
        if (hasOwner) out.push_back(ownerClass);
    }

    Handle call(KiritoVM&, std::span<const Handle> args) override;

private:
    const ast::FunctionExpr* def_;
    Handle closure_;
};

}  // namespace kirito

#endif
