#ifndef KIRITO_BUILTINS_HPP
#define KIRITO_BUILTINS_HPP
#include <span>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

#include "arena.hpp"
#include "object.hpp"

namespace kirito {

// Byte offset of each UTF-8 code point in s (so strings index/slice by character, not byte).
inline std::vector<std::size_t> utf8Starts(const std::string& s) {
    std::vector<std::size_t> starts;
    for (std::size_t i = 0; i < s.size();) {
        starts.push_back(i);
        unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3
                      : (c >> 3) == 0x1E ? 4 : 1;
        i += n;
    }
    return starts;
}
inline std::size_t utf8Length(const std::string& s) { return utf8Starts(s).size(); }

// Decode the single code point starting at byte `i` (caller guarantees a valid start). Returns the
// code point; advances nothing (the byte width is recoverable from the lead byte).
inline unsigned utf8DecodeAt(const std::string& s, std::size_t i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) return c;
    if ((c >> 5) == 0x6 && i + 1 < s.size())
        return ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
    if ((c >> 4) == 0xE && i + 2 < s.size())
        return ((c & 0x0F) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
               (static_cast<unsigned char>(s[i + 2]) & 0x3F);
    if ((c >> 3) == 0x1E && i + 3 < s.size())
        return ((c & 0x07) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
               ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
               (static_cast<unsigned char>(s[i + 3]) & 0x3F);
    return c;
}

// Append code point `cp` to `out` as UTF-8.
inline void utf8Encode(unsigned cp, std::string& out) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

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

// Boolean. Interned per VM (True/False each share one slot).
class BoolVal : public Object {
public:
    explicit BoolVal(bool v) : value_(v) {}
    bool value() const { return value_; }

    ValueKind kind() const override { return ValueKind::Bool; }
    std::string typeName() const override { return "Bool"; }
    bool truthy() const override { return value_; }
    std::string str(StringifyCtx&) const override { return value_ ? "True" : "False"; }
    bool equals(const ObjectArena&, const Object& other) const override {
        return other.kind() == ValueKind::Bool &&
               static_cast<const BoolVal&>(other).value_ == value_;
    }
    bool hashable() const override { return true; }
    std::size_t hash() const override { return value_ ? 1 : 0; }

private:
    bool value_;
};

// Immutable text. Hash is computed once and cached.
class StrVal : public Object {
public:
    explicit StrVal(std::string v) : value_(std::move(v)), hash_(std::hash<std::string>{}(value_)) {}
    const std::string& value() const { return value_; }

    ValueKind kind() const override { return ValueKind::String; }
    std::string typeName() const override { return "String"; }
    bool truthy() const override { return !value_.empty(); }
    std::string str(StringifyCtx&) const override { return value_; }
    bool equals(const ObjectArena&, const Object& other) const override {
        return other.kind() == ValueKind::String &&
               static_cast<const StrVal&>(other).value_ == value_;
    }
    bool hashable() const override { return true; }
    std::size_t hash() const override { return hash_; }
    std::optional<int64_t> length(KiritoVM&) override { return static_cast<int64_t>(utf8Length(value_)); }

    Handle binary(KiritoVM&, BinOp, Handle self, Handle rhs) override;
    Handle getItem(KiritoVM&, std::span<const Handle> keys) override;
    Handle slice(KiritoVM&, Handle start, Handle stop, Handle step) override;
    std::optional<std::vector<Handle>> iterate(KiritoVM&) override;
    bool contains(KiritoVM&, Handle value) override;
    Handle getAttr(KiritoVM&, Handle self, std::string_view name) override;

private:
    std::string value_;
    std::size_t hash_;
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
