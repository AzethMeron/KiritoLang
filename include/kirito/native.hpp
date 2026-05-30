#ifndef KIRITO_NATIVE_HPP
#define KIRITO_NATIVE_HPP

#include <memory>
#include <string>

#include "function.hpp"
#include "module.hpp"
#include "object.hpp"
#include "vm.hpp"

namespace kirito {

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
    ModuleBuilder(KiritoVM& vm, ModuleValue& mod) : vm_(vm), mod_(mod) {}

    ModuleBuilder& fn(std::string name, NativeFn impl) {
        mod_.members[name] = vm_.alloc(std::make_unique<NativeFunction>(name, std::move(impl)));
        return *this;
    }
    ModuleBuilder& value(const std::string& name, Handle h) {
        mod_.members[name] = h;
        return *this;
    }

private:
    KiritoVM& vm_;
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
