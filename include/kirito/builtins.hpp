#ifndef KIRITO_BUILTINS_HPP
#define KIRITO_BUILTINS_HPP

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

#include "arena.hpp"
#include "object.hpp"

namespace kirito {

// Render a double the Python way: shortest sensible form, always with a decimal point
// (so 2.0 prints "2.0", not "2"), while non-finite values pass through.
inline std::string floatToString(double d) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.15g", d);
    std::string s(buf);
    if (s.find_first_of(".eEni") == std::string::npos) s += ".0";
    return s;
}

// The unit value. Interned once per VM, so every `None` shares one arena slot.
class NoneVal : public Object {
public:
    ValueKind kind() const override { return ValueKind::None; }
    std::string typeName() const override { return "None"; }
    bool truthy() const override { return false; }
    std::string str(StringifyCtx&) const override { return "None"; }
    bool equals(const ObjectArena&, const Object& other) const override {
        return other.kind() == ValueKind::None;
    }
    bool hashable() const override { return true; }
    std::size_t hash() const override { return 0; }
};

// 64-bit signed integer.
class IntVal : public Object {
public:
    explicit IntVal(int64_t v) : value_(v) {}
    int64_t value() const { return value_; }

    ValueKind kind() const override { return ValueKind::Integer; }
    std::string typeName() const override { return "Integer"; }
    bool truthy() const override { return value_ != 0; }
    std::string str(StringifyCtx&) const override { return std::to_string(value_); }
    bool equals(const ObjectArena&, const Object& other) const override;
    bool hashable() const override { return true; }
    std::size_t hash() const override { return std::hash<int64_t>{}(value_); }

    Handle binary(KiritoVM&, BinOp, Handle self, Handle rhs) override;
    Handle unary(KiritoVM&, UnOp, Handle self) override;

private:
    int64_t value_;
};

// Double-precision floating point. Equality is epsilon-based (see runtime.hpp).
class FloatVal : public Object {
public:
    explicit FloatVal(double v) : value_(v) {}
    double value() const { return value_; }

    ValueKind kind() const override { return ValueKind::Float; }
    std::string typeName() const override { return "Float"; }
    bool truthy() const override { return value_ != 0.0; }
    std::string str(StringifyCtx&) const override { return floatToString(value_); }
    bool equals(const ObjectArena&, const Object& other) const override;
    bool hashable() const override { return true; }
    std::size_t hash() const override;

    Handle binary(KiritoVM&, BinOp, Handle self, Handle rhs) override;
    Handle unary(KiritoVM&, UnOp, Handle self) override;

private:
    double value_;
};

}  // namespace kirito

#endif
