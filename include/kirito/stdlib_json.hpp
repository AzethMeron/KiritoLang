#ifndef KIRITO_STDLIB_JSON_HPP
#define KIRITO_STDLIB_JSON_HPP

#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_set>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"

namespace kirito {

// JSON support. parse() turns JSON text into native Kirito values — objects become Dicts (so a
// parsed JSON object *is* a Dict), arrays become Lists, and primitives the obvious scalars.
// stringify() does the reverse.
namespace json {

class Parser {
public:
    Parser(KiritoVM& vm, const std::string& src, RootScope& roots) : vm_(vm), s_(src), roots_(roots) {}

    Handle parse() {
        skipWs();
        Handle v = value();
        skipWs();
        if (pos_ != s_.size()) fail("trailing characters after JSON value");
        return v;
    }

private:
    [[noreturn]] void fail(const std::string& msg) { throw KiritoError("JSON parse error: " + msg); }
    void skipWs() { while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_; }
    char peek() { return pos_ < s_.size() ? s_[pos_] : '\0'; }
    bool match(const char* lit) {
        std::size_t n = std::char_traits<char>::length(lit);
        if (s_.compare(pos_, n, lit) == 0) { pos_ += n; return true; }
        return false;
    }

    Handle value() {
        skipWs();
        char c = peek();
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') return roots_.add(vm_.makeString(string()));
        if (c == 't') { if (match("true")) return vm_.makeBool(true); fail("invalid literal"); }
        if (c == 'f') { if (match("false")) return vm_.makeBool(false); fail("invalid literal"); }
        if (c == 'n') { if (match("null")) return vm_.none(); fail("invalid literal"); }
        if (c == '-' || (c >= '0' && c <= '9')) return number();
        fail("unexpected character");
    }

    Handle object() {
        ++pos_;  // {
        auto dict = std::make_unique<DictVal>();
        Handle h = roots_.add(vm_.alloc(std::move(dict)));
        auto& d = static_cast<DictVal&>(vm_.arena().deref(h));
        skipWs();
        if (peek() == '}') { ++pos_; return h; }
        while (true) {
            skipWs();
            if (peek() != '"') fail("expected string key");
            Handle key = roots_.add(vm_.makeString(string()));
            skipWs();
            if (peek() != ':') fail("expected ':'");
            ++pos_;
            Handle val = value();
            d.set(vm_.arena(), key, val);
            skipWs();
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == '}') { ++pos_; break; }
            fail("expected ',' or '}'");
        }
        return h;
    }

    Handle array() {
        ++pos_;  // [
        auto list = std::make_unique<ListVal>();
        Handle h = roots_.add(vm_.alloc(std::move(list)));
        auto& l = static_cast<ListVal&>(vm_.arena().deref(h));
        skipWs();
        if (peek() == ']') { ++pos_; return h; }
        while (true) {
            l.elems.push_back(value());
            skipWs();
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == ']') { ++pos_; break; }
            fail("expected ',' or ']'");
        }
        return h;
    }

    std::string string() {
        ++pos_;  // opening quote
        std::string out;
        while (pos_ < s_.size() && s_[pos_] != '"') {
            char c = s_[pos_++];
            if (c == '\\') {
                if (pos_ >= s_.size()) fail("bad escape");
                char e = s_[pos_++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        if (pos_ + 4 > s_.size()) fail("bad \\u escape");
                        unsigned cp = 0;
                        for (int k = 0; k < 4; ++k) {
                            char d = s_[pos_ + k];
                            int v = (d >= '0' && d <= '9') ? d - '0'
                                  : (d >= 'a' && d <= 'f') ? d - 'a' + 10
                                  : (d >= 'A' && d <= 'F') ? d - 'A' + 10 : -1;
                            if (v < 0) fail("invalid \\u escape (expected hex digits)");
                            cp = cp * 16 + static_cast<unsigned>(v);
                        }
                        pos_ += 4;
                        encodeUtf8(cp, out);
                        break;
                    }
                    default: fail("bad escape");
                }
            } else {
                out += c;
            }
        }
        if (pos_ >= s_.size()) fail("unterminated string");
        ++pos_;  // closing quote
        return out;
    }

    static void encodeUtf8(unsigned cp, std::string& out) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    }

    Handle number() {
        std::size_t start = pos_;
        bool isFloat = false;
        if (peek() == '-') ++pos_;
        while (pos_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[pos_])))) ++pos_;
        if (peek() == '.') { isFloat = true; ++pos_; while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_; }
        if (peek() == 'e' || peek() == 'E') { isFloat = true; ++pos_; if (peek() == '+' || peek() == '-') ++pos_; while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_; }
        std::string tok = s_.substr(start, pos_ - start);
        if (tok.empty() || tok == "-") fail("invalid number");
        try {
            if (isFloat) return roots_.add(vm_.makeFloat(std::stod(tok)));
            return vm_.makeInt(static_cast<int64_t>(std::stoll(tok)));
        } catch (const std::out_of_range&) {
            // An integer too large for int64 -> widen to Float (mirroring dynamic languages). If
            // even the double overflows, represent it as infinity rather than throwing.
            try { return roots_.add(vm_.makeFloat(std::stod(tok))); }
            catch (const std::out_of_range&) {
                return roots_.add(vm_.makeFloat(tok[0] == '-' ? -HUGE_VAL : HUGE_VAL));
            }
        } catch (const std::invalid_argument&) {
            fail("invalid number");
        }
    }

    KiritoVM& vm_;
    const std::string& s_;
    RootScope& roots_;
    std::size_t pos_ = 0;
};

inline void escapeString(const std::string& s, std::string& out) {
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default: out += c;
        }
    }
    out += '"';
}

inline void write(KiritoVM& vm, Handle h, std::string& out, std::unordered_set<const Object*>& active) {
    const Object& o = vm.arena().deref(h);
    switch (o.kind()) {
        case ValueKind::None: out += "null"; return;
        case ValueKind::Bool: out += static_cast<const BoolVal&>(o).value() ? "true" : "false"; return;
        case ValueKind::Integer: out += std::to_string(static_cast<const IntVal&>(o).value()); return;
        case ValueKind::Float: out += floatToString(static_cast<const FloatVal&>(o).value()); return;
        case ValueKind::String: escapeString(static_cast<const StrVal&>(o).value(), out); return;
        default: break;
    }
    if (active.count(&o)) throw KiritoError("cannot serialize a cyclic structure to JSON");
    active.insert(&o);
    if (o.kind() == ValueKind::List || o.kind() == ValueKind::Array) {
        out += '[';
        const auto& l = static_cast<const ListVal&>(o);
        for (std::size_t i = 0; i < l.elems.size(); ++i) { if (i) out += ", "; write(vm, l.elems[i], out, active); }
        out += ']';
    } else if (o.kind() == ValueKind::Dict) {
        out += '{';
        const auto& d = static_cast<const DictVal&>(o);
        bool first = true;
        for (Handle k : d.keys()) {
            if (!first) out += ", ";
            first = false;
            const Object& ko = vm.arena().deref(k);
            if (ko.kind() == ValueKind::String) escapeString(static_cast<const StrVal&>(ko).value(), out);
            else escapeString(vm.stringify(k), out);  // non-string keys become strings, JSON-style
            out += ": ";
            write(vm, *d.find(vm.arena(), k), out, active);
        }
        out += '}';
    } else {
        active.erase(&o);
        throw KiritoError("cannot serialize '" + o.typeName() + "' to JSON");
    }
    active.erase(&o);
}

}  // namespace json

class JsonModule : public NativeModule {
public:
    std::string name() const override { return "json"; }
    void setup(ModuleBuilder& m) override {
        m.fn("parse", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const Object& o = vm.arena().deref(a[0]);
            if (o.kind() != ValueKind::String) throw KiritoError("json.parse expects a String");
            RootScope roots(vm);
            json::Parser p(vm, static_cast<const StrVal&>(o).value(), roots);
            return p.parse();
        });
        m.fn("stringify", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::string out;
            std::unordered_set<const Object*> active;
            json::write(vm, a[0], out, active);
            return vm.makeString(std::move(out));
        });
    }
};

}  // namespace kirito

#endif
