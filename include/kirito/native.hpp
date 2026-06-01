#ifndef KIRITO_NATIVE_HPP
#define KIRITO_NATIVE_HPP

#include <memory>
#include <string>

#include "builtins.hpp"
#include "function.hpp"
#include "module.hpp"
#include "object.hpp"
#include "value.hpp"
#include "vm.hpp"

namespace kirito {

// Shared argument-extraction helpers for native functions/modules — one place to dereference a
// Handle and type-check it, so modules don't each reimplement asStr/asInt (DRY). `who` names the
// caller for the error message.
inline const std::string& argString(KiritoVM& vm, Handle h, const char* who) {
    const Object& o = vm.arena().deref(h);
    if (o.kind() != ValueKind::String) throw KiritoError(std::string(who) + " expects a String");
    return static_cast<const StrVal&>(o).value();
}
inline int64_t argInt(KiritoVM& vm, Handle h, const char* who) {
    const Object& o = vm.arena().deref(h);
    if (o.kind() != ValueKind::Integer) throw KiritoError(std::string(who) + " expects an Integer");
    return static_cast<const IntVal&>(o).value();
}

// ============================================================================================
// Extension API — how C++ code adds new functions, modules, and types to Kirito.
//
//  * A one-off function:   vm.registerGlobal("now", myFn);
//  * A whole module:       struct MyMod : NativeModule { ... };  vm.install<MyMod>();
//  * A new object type:    struct MyType : NativeClass<MyType> { ... };  expose a constructor
//                          with vm.registerGlobal("MyType", factoryFn);
//
// Everything flows through the same uniform Object protocol, so an extension is indistinguishable
// from a built-in to the evaluator.
// ============================================================================================

// Fluent helper passed to NativeModule::setup() to register a module's members with no boilerplate.
class ModuleBuilder {
public:
    ModuleBuilder(KiritoVM& vm, Handle module, ModuleValue& mod) : vm_(vm), module_(module), mod_(mod) {}

    // The module's own value handle, so members can capture it to reach *sibling* members at call
    // time (e.g. io.print resolving the current io.stdout, which the user may have reassigned).
    Handle moduleHandle() const { return module_; }

    ModuleBuilder& fn(std::string name, NativeFn impl) {
        mod_.members[name] = vm_.alloc(std::make_unique<NativeFunction>(name, std::move(impl)));
        return *this;
    }
    // With a declared signature: the function then accepts keyword arguments and defaults, and
    // `inspect` shows its parameters/types and return type. Params: {"x"} / {"x","Int"} /
    // {"x","Int", vm().makeInt(0)} (with default).
    ModuleBuilder& fn(std::string name, std::vector<NativeParam> sig, std::string returnType, NativeFn impl) {
        mod_.members[name] = vm_.alloc(std::make_unique<NativeFunction>(
            name, std::move(sig), std::move(returnType), std::move(impl)));
        return *this;
    }
    ModuleBuilder& value(const std::string& name, Handle h) {
        mod_.members[name] = h;
        return *this;
    }
    // Bind `name` to the same member as an already-registered `existing` (a second public name).
    ModuleBuilder& alias(const std::string& name, const std::string& existing) {
        auto it = mod_.members.find(existing);
        if (it == mod_.members.end()) throw KiritoError("alias target '" + existing + "' not registered");
        mod_.members[name] = it->second;
        return *this;
    }
    KiritoVM& vm() { return vm_; }

private:
    KiritoVM& vm_;
    Handle module_;
    ModuleValue& mod_;
};

// Inherit and implement to define a module in C++: give it a name and register its members.
class NativeModule {
public:
    virtual ~NativeModule() = default;
    virtual std::string name() const = 0;
    virtual void setup(ModuleBuilder&) = 0;
};

// CRTP convenience base for a C++-authored object type. Fills in sane protocol defaults (identity
// equality, a name) so a subclass overrides only the slots it actually supports (binary, getAttr,
// call, ...). Derived must define `static constexpr const char* kTypeName`.
template <class Derived>
class NativeClass : public Object {
public:
    ValueKind kind() const override { return ValueKind::Instance; }
    std::string typeName() const override { return Derived::kTypeName; }
    bool truthy() const override { return true; }
    std::string str(StringifyCtx&) const override {
        return std::string("<") + Derived::kTypeName + ">";
    }
    bool equals(const ObjectArena&, const Object& other) const override { return this == &other; }
};

}  // namespace kirito

#endif
