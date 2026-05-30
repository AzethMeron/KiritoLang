#ifndef KIRITO_RUNTIME_HPP
#define KIRITO_RUNTIME_HPP

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

#include "ast.hpp"
#include "builtins.hpp"
#include "class_value.hpp"
#include "collections.hpp"
#include "evaluator.hpp"
#include "function.hpp"
#include "lexer.hpp"
#include "module.hpp"
#include "native.hpp"
#include "object.hpp"
#include "parser.hpp"
#include "stdlib_io.hpp"
#include "stdlib_math.hpp"
#include "vm.hpp"

// Definitions that need a complete KiritoVM (and the front end): they live here, included last,
// so the per-component headers stay free of upward dependencies.

namespace kirito {

// --- numeric helpers (Python semantics) -----------------------------------------------------

inline bool isNumeric(const Object& o) {
    return o.kind() == ValueKind::Integer || o.kind() == ValueKind::Float;
}
inline double asDouble(const Object& o) {
    return o.kind() == ValueKind::Integer
               ? static_cast<double>(static_cast<const IntVal&>(o).value())
               : static_cast<const FloatVal&>(o).value();
}
inline bool floatEqual(double l, double r) {
    constexpr double absEps = 1e-9, relEps = 1e-9;
    double diff = std::fabs(l - r);
    if (diff <= absEps) return true;
    return diff <= relEps * std::max(std::fabs(l), std::fabs(r));
}
inline int64_t ifloordiv(int64_t a, int64_t b) {
    int64_t q = a / b, r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}
inline int64_t imod(int64_t a, int64_t b) {
    int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}
inline int64_t ipow(int64_t base, int64_t exp) {
    int64_t result = 1;
    while (exp > 0) {
        if (exp & 1) result *= base;
        exp >>= 1;
        if (exp) base *= base;
    }
    return result;
}

// Shared arithmetic dispatch for Integer/Float. Integer⊕Integer stays Integer (except `/`),
// any Float promotes to Float, and `/` always yields Float.
inline Handle numericBinary(KiritoVM& vm, BinOp op, Handle aH, Handle bH) {
    const Object& a = vm.arena().deref(aH);
    const Object& b = vm.arena().deref(bH);
    if (!isNumeric(b))
        throw KiritoError("unsupported operand type '" + b.typeName() +
                          "' for arithmetic with '" + a.typeName() + "'");

    if (op == BinOp::Div) {
        double db = asDouble(b);
        if (db == 0.0) throw KiritoError("division by zero");
        return vm.makeFloat(asDouble(a) / db);
    }

    switch (op) {
        case BinOp::Lt: return vm.makeBool(asDouble(a) < asDouble(b));
        case BinOp::Le: return vm.makeBool(asDouble(a) <= asDouble(b));
        case BinOp::Gt: return vm.makeBool(asDouble(a) > asDouble(b));
        case BinOp::Ge: return vm.makeBool(asDouble(a) >= asDouble(b));
        default: break;
    }

    if (a.kind() == ValueKind::Integer && b.kind() == ValueKind::Integer) {
        int64_t x = static_cast<const IntVal&>(a).value();
        int64_t y = static_cast<const IntVal&>(b).value();
        switch (op) {
            case BinOp::Add: return vm.makeInt(x + y);
            case BinOp::Sub: return vm.makeInt(x - y);
            case BinOp::Mul: return vm.makeInt(x * y);
            case BinOp::FloorDiv:
                if (y == 0) throw KiritoError("integer division by zero");
                return vm.makeInt(ifloordiv(x, y));
            case BinOp::Mod:
                if (y == 0) throw KiritoError("integer modulo by zero");
                return vm.makeInt(imod(x, y));
            case BinOp::Pow:
                if (y < 0) return vm.makeFloat(std::pow(static_cast<double>(x), static_cast<double>(y)));
                return vm.makeInt(ipow(x, y));
            default: break;
        }
    } else {
        double x = asDouble(a), y = asDouble(b);
        switch (op) {
            case BinOp::Add: return vm.makeFloat(x + y);
            case BinOp::Sub: return vm.makeFloat(x - y);
            case BinOp::Mul: return vm.makeFloat(x * y);
            case BinOp::FloorDiv:
                if (y == 0.0) throw KiritoError("float division by zero");
                return vm.makeFloat(std::floor(x / y));
            case BinOp::Mod:
                if (y == 0.0) throw KiritoError("float modulo by zero");
                return vm.makeFloat(x - std::floor(x / y) * y);
            case BinOp::Pow: return vm.makeFloat(std::pow(x, y));
            default: break;
        }
    }
    throw KiritoError("unsupported numeric operator");
}

// --- IntVal / FloatVal out-of-line members ---------------------------------------------------

inline bool IntVal::equals(const ObjectArena&, const Object& other) const {
    if (other.kind() == ValueKind::Integer) return value_ == static_cast<const IntVal&>(other).value();
    if (other.kind() == ValueKind::Float)
        return floatEqual(static_cast<double>(value_), static_cast<const FloatVal&>(other).value());
    return false;
}
inline Handle IntVal::binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) {
    return numericBinary(vm, op, self, rhs);
}
inline Handle IntVal::unary(KiritoVM& vm, UnOp op, Handle) {
    if (op == UnOp::Neg) return vm.makeInt(-value_);
    throw KiritoError("Integer does not support this unary operator");
}

inline bool FloatVal::equals(const ObjectArena&, const Object& other) const {
    if (other.kind() == ValueKind::Float) return floatEqual(value_, static_cast<const FloatVal&>(other).value());
    if (other.kind() == ValueKind::Integer)
        return floatEqual(value_, static_cast<double>(static_cast<const IntVal&>(other).value()));
    return false;
}
inline std::size_t FloatVal::hash() const {
    double rounded = std::round(value_);
    if (rounded == value_ && std::fabs(value_) < 9.2e18)
        return std::hash<int64_t>{}(static_cast<int64_t>(rounded));
    return std::hash<double>{}(value_);
}
inline Handle FloatVal::binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) {
    return numericBinary(vm, op, self, rhs);
}
inline Handle FloatVal::unary(KiritoVM& vm, UnOp op, Handle) {
    if (op == UnOp::Neg) return vm.makeFloat(-value_);
    throw KiritoError("Float does not support this unary operator");
}

// --- StrVal out-of-line members --------------------------------------------------------------

inline Handle StrVal::binary(KiritoVM& vm, BinOp op, Handle, Handle rhs) {
    const Object& b = vm.arena().deref(rhs);
    if (op == BinOp::Add) {
        if (b.kind() != ValueKind::String)
            throw KiritoError("can only concatenate String to String, not '" + b.typeName() + "'");
        return vm.makeString(value_ + static_cast<const StrVal&>(b).value());
    }
    if (op == BinOp::Mul) {
        if (b.kind() != ValueKind::Integer)
            throw KiritoError("can only repeat String by an Integer");
        int64_t n = static_cast<const IntVal&>(b).value();
        std::string out;
        for (int64_t i = 0; i < n; ++i) out += value_;
        return vm.makeString(std::move(out));
    }
    if (b.kind() == ValueKind::String) {
        const std::string& r = static_cast<const StrVal&>(b).value();
        switch (op) {
            case BinOp::Lt: return vm.makeBool(value_ < r);
            case BinOp::Le: return vm.makeBool(value_ <= r);
            case BinOp::Gt: return vm.makeBool(value_ > r);
            case BinOp::Ge: return vm.makeBool(value_ >= r);
            default: break;
        }
    }
    throw KiritoError("type 'String' does not support this operator");
}

// --- Collection out-of-line members ----------------------------------------------------------

// Resolve an Integer key against a sequence length, supporting Python negative indices.
inline std::size_t sequenceIndex(KiritoVM& vm, std::size_t size, Handle key) {
    const Object& k = vm.arena().deref(key);
    if (k.kind() != ValueKind::Integer)
        throw KiritoError("index must be Integer, not '" + k.typeName() + "'");
    int64_t i = static_cast<const IntVal&>(k).value();
    int64_t n = static_cast<int64_t>(size);
    if (i < 0) i += n;
    if (i < 0 || i >= n) throw KiritoError("index out of range");
    return static_cast<std::size_t>(i);
}

inline Handle ListVal::getItem(KiritoVM& vm, Handle key) {
    return elems[sequenceIndex(vm, elems.size(), key)];
}
inline void ListVal::setItem(KiritoVM& vm, Handle key, Handle value) {
    elems[sequenceIndex(vm, elems.size(), key)] = value;
}
inline Handle ListVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    if (name == "append")
        return vm.alloc(std::make_unique<NativeFunction>(
            "append",
            [self](KiritoVM& vm, std::span<const Handle> args) -> Handle {
                if (args.size() != 1) throw KiritoError("append expected 1 argument");
                static_cast<ListVal&>(vm.arena().deref(self)).elems.push_back(args[0]);
                return vm.none();
            },
            std::vector<Handle>{self}));
    if (name == "pop")
        return vm.alloc(std::make_unique<NativeFunction>(
            "pop",
            [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
                auto& list = static_cast<ListVal&>(vm.arena().deref(self));
                if (list.elems.empty()) throw KiritoError("pop from empty List");
                Handle last = list.elems.back();
                list.elems.pop_back();
                return last;
            },
            std::vector<Handle>{self}));
    return Object::getAttr(vm, self, name);
}

inline Handle DictVal::getItem(KiritoVM& vm, Handle key) {
    const Handle* v = find(vm.arena(), key);
    if (!v) throw KiritoError("key not found: " + vm.stringify(key));
    return *v;
}
inline void DictVal::setItem(KiritoVM& vm, Handle key, Handle value) {
    set(vm.arena(), key, value);
}

inline Handle SetVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    if (name == "add")
        return vm.alloc(std::make_unique<NativeFunction>(
            "add",
            [self](KiritoVM& vm, std::span<const Handle> args) -> Handle {
                if (args.size() != 1) throw KiritoError("add expected 1 argument");
                static_cast<SetVal&>(vm.arena().deref(self)).add(vm.arena(), args[0]);
                return vm.none();
            },
            std::vector<Handle>{self}));
    if (name == "contains")
        return vm.alloc(std::make_unique<NativeFunction>(
            "contains",
            [self](KiritoVM& vm, std::span<const Handle> args) -> Handle {
                if (args.size() != 1) throw KiritoError("contains expected 1 argument");
                return vm.makeBool(static_cast<SetVal&>(vm.arena().deref(self)).contains(vm.arena(), args[0]));
            },
            std::vector<Handle>{self}));
    return Object::getAttr(vm, self, name);
}

// --- slicing helper --------------------------------------------------------------------------

// Concrete indices for a Python slice over [0,len). start/stop/step are Integer handles or None.
// Mirrors CPython's PySlice_AdjustIndices (negative indices, clamping, negative step).
inline std::vector<int64_t> pythonSliceIndices(KiritoVM& vm, int64_t len,
                                               Handle sH, Handle eH, Handle stH) {
    auto opt = [&](Handle h) -> std::optional<int64_t> {
        const Object& o = vm.arena().deref(h);
        if (o.kind() == ValueKind::None) return std::nullopt;
        if (o.kind() != ValueKind::Integer) throw KiritoError("slice indices must be Integer or None");
        return static_cast<const IntVal&>(o).value();
    };
    std::optional<int64_t> so = opt(sH), eo = opt(eH), sto = opt(stH);
    int64_t step = sto.value_or(1);
    if (step == 0) throw KiritoError("slice step cannot be zero");
    int64_t lower = step < 0 ? -1 : 0;
    int64_t upper = step < 0 ? len - 1 : len;
    int64_t start, stop;
    if (!so) start = step < 0 ? upper : lower;
    else { start = *so; if (start < 0) { start += len; if (start < lower) start = lower; }
           else if (start > upper) start = upper; }
    if (!eo) stop = step < 0 ? lower : upper;
    else { stop = *eo; if (stop < 0) { stop += len; if (stop < lower) stop = lower; }
           else if (stop > upper) stop = upper; }
    std::vector<int64_t> idx;
    if (step > 0) for (int64_t i = start; i < stop; i += step) idx.push_back(i);
    else for (int64_t i = start; i > stop; i += step) idx.push_back(i);
    return idx;
}

// --- String indexing / slicing / iteration (UTF-8 aware) -------------------------------------

inline Handle StrVal::getItem(KiritoVM& vm, Handle key) {
    auto starts = utf8Starts(value_);
    std::size_t i = sequenceIndex(vm, starts.size(), key);
    std::size_t b = starts[i];
    std::size_t e = (i + 1 < starts.size()) ? starts[i + 1] : value_.size();
    return vm.makeString(value_.substr(b, e - b));
}

inline Handle StrVal::slice(KiritoVM& vm, Handle s, Handle e, Handle st) {
    auto starts = utf8Starts(value_);
    int64_t len = static_cast<int64_t>(starts.size());
    std::string out;
    for (int64_t i : pythonSliceIndices(vm, len, s, e, st)) {
        std::size_t b = starts[i];
        std::size_t en = (i + 1 < len) ? starts[i + 1] : value_.size();
        out.append(value_, b, en - b);
    }
    return vm.makeString(std::move(out));
}

inline std::optional<std::vector<Handle>> StrVal::iterate(KiritoVM& vm) {
    RootScope rs(vm);  // each character is a fresh String; protect them while building
    auto starts = utf8Starts(value_);
    std::vector<Handle> out;
    out.reserve(starts.size());
    for (std::size_t i = 0; i < starts.size(); ++i) {
        std::size_t b = starts[i];
        std::size_t e = (i + 1 < starts.size()) ? starts[i + 1] : value_.size();
        out.push_back(rs.add(vm.makeString(value_.substr(b, e - b))));
    }
    return out;
}

inline bool StrVal::contains(KiritoVM& vm, Handle value) {
    const Object& o = vm.arena().deref(value);
    if (o.kind() != ValueKind::String)
        throw KiritoError("'in <String>' requires a String, not '" + o.typeName() + "'");
    return value_.find(static_cast<const StrVal&>(o).value()) != std::string::npos;
}

// Read a String argument or raise.
inline const std::string& asStr(KiritoVM& vm, Handle h, const char* what) {
    const Object& o = vm.arena().deref(h);
    if (o.kind() != ValueKind::String) throw KiritoError(std::string(what) + " requires a String");
    return static_cast<const StrVal&>(o).value();
}

inline Handle StrVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    auto bind = [&](const char* nm, NativeFn fn) {
        return vm.alloc(std::make_unique<NativeFunction>(nm, std::move(fn), std::vector<Handle>{self}));
    };
    auto recv = [](KiritoVM& vm, Handle self) -> const std::string& {
        return static_cast<StrVal&>(vm.arena().deref(self)).value();
    };

    if (name == "upper")
        return bind("upper", [self, recv](KiritoVM& vm, std::span<const Handle>) {
            std::string s = recv(vm, self);
            for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return vm.makeString(std::move(s));
        });
    if (name == "lower")
        return bind("lower", [self, recv](KiritoVM& vm, std::span<const Handle>) {
            std::string s = recv(vm, self);
            for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return vm.makeString(std::move(s));
        });
    if (name == "strip" || name == "lstrip" || name == "rstrip") {
        bool left = name != "rstrip", right = name != "lstrip";
        return bind(std::string(name).c_str(), [self, recv, left, right](KiritoVM& vm, std::span<const Handle>) {
            const std::string& s = recv(vm, self);
            std::size_t b = 0, e = s.size();
            if (left) while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
            if (right) while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
            return vm.makeString(s.substr(b, e - b));
        });
    }
    if (name == "startswith")
        return bind("startswith", [self, recv](KiritoVM& vm, std::span<const Handle> a) {
            const std::string& s = recv(vm, self);
            const std::string& p = asStr(vm, a[0], "startswith");
            return vm.makeBool(s.size() >= p.size() && s.compare(0, p.size(), p) == 0);
        });
    if (name == "endswith")
        return bind("endswith", [self, recv](KiritoVM& vm, std::span<const Handle> a) {
            const std::string& s = recv(vm, self);
            const std::string& p = asStr(vm, a[0], "endswith");
            return vm.makeBool(s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0);
        });
    if (name == "replace")
        return bind("replace", [self, recv](KiritoVM& vm, std::span<const Handle> a) {
            std::string s = recv(vm, self);
            const std::string& from = asStr(vm, a[0], "replace");
            const std::string& to = asStr(vm, a[1], "replace");
            if (!from.empty()) {
                std::string out;
                std::size_t pos = 0, prev = 0;
                while ((pos = s.find(from, prev)) != std::string::npos) {
                    out.append(s, prev, pos - prev);
                    out += to;
                    prev = pos + from.size();
                }
                out.append(s, prev, std::string::npos);
                s = std::move(out);
            }
            return vm.makeString(std::move(s));
        });
    if (name == "count")
        return bind("count", [self, recv](KiritoVM& vm, std::span<const Handle> a) {
            const std::string& s = recv(vm, self);
            const std::string& sub = asStr(vm, a[0], "count");
            int64_t n = 0;
            if (!sub.empty())
                for (std::size_t pos = s.find(sub); pos != std::string::npos; pos = s.find(sub, pos + sub.size()))
                    ++n;
            return vm.makeInt(n);
        });
    if (name == "find")
        return bind("find", [self, recv](KiritoVM& vm, std::span<const Handle> a) {
            const std::string& s = recv(vm, self);
            const std::string& sub = asStr(vm, a[0], "find");
            std::size_t bp = s.find(sub);
            if (bp == std::string::npos) return vm.makeInt(-1);
            int64_t cp = 0;  // byte offset -> code point index
            for (std::size_t st : utf8Starts(s)) { if (st >= bp) break; ++cp; }
            return vm.makeInt(cp);
        });
    if (name == "split")
        return bind("split", [self, recv](KiritoVM& vm, std::span<const Handle> a) {
            const std::string& s = recv(vm, self);
            RootScope rs(vm);
            auto list = std::make_unique<ListVal>();
            if (a.empty()) {
                std::size_t i = 0, n = s.size();
                while (i < n) {
                    while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
                    std::size_t start = i;
                    while (i < n && !std::isspace(static_cast<unsigned char>(s[i]))) ++i;
                    if (i > start) list->elems.push_back(rs.add(vm.makeString(s.substr(start, i - start))));
                }
            } else {
                const std::string& sep = asStr(vm, a[0], "split");
                if (sep.empty()) throw KiritoError("empty separator");
                std::size_t pos, prev = 0;
                while ((pos = s.find(sep, prev)) != std::string::npos) {
                    list->elems.push_back(rs.add(vm.makeString(s.substr(prev, pos - prev))));
                    prev = pos + sep.size();
                }
                list->elems.push_back(rs.add(vm.makeString(s.substr(prev))));
            }
            return vm.alloc(std::move(list));
        });
    if (name == "join")
        return bind("join", [self, recv](KiritoVM& vm, std::span<const Handle> a) {
            const std::string& sep = recv(vm, self);
            auto items = vm.arena().deref(a[0]).iterate(vm);
            std::string out;
            bool first = true;
            for (Handle h : items.value()) {
                if (!first) out += sep;
                first = false;
                out += asStr(vm, h, "join");
            }
            return vm.makeString(std::move(out));
        });
    if (name == "format")
        return bind("format", [self, recv](KiritoVM& vm, std::span<const Handle> a) {
            const std::string& tmpl = recv(vm, self);
            std::string out;
            std::size_t auto_i = 0;
            for (std::size_t i = 0; i < tmpl.size(); ++i) {
                if (tmpl[i] == '{') {
                    if (i + 1 < tmpl.size() && tmpl[i + 1] == '{') { out += '{'; ++i; continue; }
                    std::size_t close = tmpl.find('}', i);
                    if (close == std::string::npos) throw KiritoError("unmatched '{' in format string");
                    std::string spec = tmpl.substr(i + 1, close - i - 1);
                    std::size_t idx = spec.empty() ? auto_i++ : static_cast<std::size_t>(std::stoll(spec));
                    if (idx >= a.size()) throw KiritoError("format index out of range");
                    out += vm.stringify(a[idx]);
                    i = close;
                } else if (tmpl[i] == '}' && i + 1 < tmpl.size() && tmpl[i + 1] == '}') {
                    out += '}';
                    ++i;
                } else {
                    out += tmpl[i];
                }
            }
            return vm.makeString(std::move(out));
        });
    return Object::getAttr(vm, self, name);
}

// --- List slice / contains, Dict / Set contains ----------------------------------------------

inline Handle ListVal::slice(KiritoVM& vm, Handle s, Handle e, Handle st) {
    auto result = std::make_unique<ListVal>();
    for (int64_t i : pythonSliceIndices(vm, static_cast<int64_t>(elems.size()), s, e, st))
        result->elems.push_back(elems[i]);  // existing handles, reachable from this list
    return vm.alloc(std::move(result));
}
inline bool ListVal::contains(KiritoVM& vm, Handle value) {
    const Object& v = vm.arena().deref(value);
    for (Handle e : elems)
        if (vm.arena().deref(e).equals(vm.arena(), v)) return true;
    return false;
}
inline bool DictVal::contains(KiritoVM& vm, Handle key) {
    return find(vm.arena(), key) != nullptr;
}
inline bool SetVal::contains(KiritoVM& vm, Handle value) {
    return contains(vm.arena(), value);
}

// --- Classes & instances ---------------------------------------------------------------------

inline Handle ClassValue::call(KiritoVM& vm, std::span<const Handle> args) {
    auto inst = std::make_unique<InstanceValue>();
    inst->cls = selfHandle;
    inst->className = name;
    Handle instH = vm.alloc(std::move(inst));
    if (const Handle* init = findMethod(vm.arena(), "init")) {
        RootScope rs(vm);
        rs.add(instH);
        Handle initH = rs.add(*init);
        std::vector<Handle> full;
        full.reserve(args.size() + 1);
        full.push_back(instH);
        for (Handle a : args) full.push_back(a);
        vm.arena().deref(initH).call(vm, full);
    }
    return instH;
}

inline Handle InstanceValue::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    auto it = attrs.find(std::string(name));
    if (it != attrs.end()) return it->second;
    const auto& klass = static_cast<const ClassValue&>(vm.arena().deref(cls));
    const Handle* method = klass.findMethod(vm.arena(), std::string(name));
    if (!method)
        throw KiritoError("'" + className + "' object has no attribute '" + std::string(name) + "'");
    // Return a bound method: a callable that prepends the receiver before invoking the function.
    Handle methodH = *method;
    return vm.alloc(std::make_unique<NativeFunction>(
        std::string(name),
        [self, methodH](KiritoVM& vm, std::span<const Handle> args) -> Handle {
            std::vector<Handle> full;
            full.reserve(args.size() + 1);
            full.push_back(self);
            for (Handle a : args) full.push_back(a);
            return vm.arena().deref(methodH).call(vm, full);
        },
        std::vector<Handle>{self, methodH}));
}

// --- KiFunction call -------------------------------------------------------------------------

inline Handle KiFunction::call(KiritoVM& vm, std::span<const Handle> args) {
    const auto& params = def_->params;
    if (args.size() != params.size())
        throw KiritoError("function expected " + std::to_string(params.size()) +
                          " argument(s), got " + std::to_string(args.size()));
    Handle scope = vm.newScope(closure_);
    auto& env = static_cast<EnvValue&>(vm.arena().deref(scope));
    for (std::size_t i = 0; i < params.size(); ++i) env.define(params[i], args[i]);
    Evaluator ev(vm, scope);
    return ev.callBody(def_->body);
}

// --- VM entry point & lifetime ---------------------------------------------------------------

// --- module / extension API ------------------------------------------------------------------

inline void KiritoVM::registerModule(std::string name, ModuleFactory factory) {
    moduleFactories_[std::move(name)] = std::move(factory);
}

template <class T>
void KiritoVM::install() {
    nativeModules_.push_back(std::make_unique<T>());
    NativeModule* mod = nativeModules_.back().get();
    registerModule(mod->name(), [mod](KiritoVM& vm) -> Handle {
        Handle h = vm.alloc(std::make_unique<ModuleValue>(mod->name()));
        ModuleBuilder builder(vm, static_cast<ModuleValue&>(vm.arena().deref(h)));
        mod->setup(builder);
        return h;
    });
}

inline Handle KiritoVM::importModule(const std::string& name) {
    auto cached = moduleCache_.find(name);
    if (cached != moduleCache_.end()) return cached->second;  // modules are per-VM singletons
    auto factory = moduleFactories_.find(name);
    if (factory == moduleFactories_.end()) throw KiritoError("no module named '" + name + "'");
    Handle h = factory->second(*this);
    moduleCache_[name] = h;
    return h;
}

// --- built-in globals ------------------------------------------------------------------------

inline void KiritoVM::installBuiltins() {
    auto& g = static_cast<EnvValue&>(arena_.deref(global_));
    auto def = [&](const char* name, NativeFn fn) {
        g.define(name, alloc(std::make_unique<NativeFunction>(name, std::move(fn))));
    };

    def("len", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
        if (args.size() != 1) throw KiritoError("len expected 1 argument");
        return vm.makeInt(vm.arena().deref(args[0]).length(vm).value());
    });

    def("import", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
        if (args.size() != 1) throw KiritoError("import expected 1 argument");
        const Object& a = vm.arena().deref(args[0]);
        if (a.kind() != ValueKind::String) throw KiritoError("import expects a String module name");
        return vm.importModule(static_cast<const StrVal&>(a).value());
    });

    // Type constructors double as converters (Python style): Integer("42"), String(n), ...
    def("Integer", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
        if (args.size() != 1) throw KiritoError("Integer expected 1 argument");
        const Object& o = vm.arena().deref(args[0]);
        switch (o.kind()) {
            case ValueKind::Integer: return args[0];
            case ValueKind::Bool: return vm.makeInt(static_cast<const BoolVal&>(o).value() ? 1 : 0);
            case ValueKind::Float:
                return vm.makeInt(static_cast<int64_t>(static_cast<const FloatVal&>(o).value()));
            case ValueKind::String: {
                const std::string& s = static_cast<const StrVal&>(o).value();
                try {
                    return vm.makeInt(static_cast<int64_t>(std::stoll(s)));
                } catch (...) {
                    throw KiritoError("cannot convert String to Integer: '" + s + "'");
                }
            }
            default: throw KiritoError("cannot convert '" + o.typeName() + "' to Integer");
        }
    });

    def("Float", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
        if (args.size() != 1) throw KiritoError("Float expected 1 argument");
        const Object& o = vm.arena().deref(args[0]);
        switch (o.kind()) {
            case ValueKind::Float: return args[0];
            case ValueKind::Integer:
                return vm.makeFloat(static_cast<double>(static_cast<const IntVal&>(o).value()));
            case ValueKind::Bool: return vm.makeFloat(static_cast<const BoolVal&>(o).value() ? 1.0 : 0.0);
            case ValueKind::String: {
                const std::string& s = static_cast<const StrVal&>(o).value();
                try {
                    return vm.makeFloat(std::stod(s));
                } catch (...) {
                    throw KiritoError("cannot convert String to Float: '" + s + "'");
                }
            }
            default: throw KiritoError("cannot convert '" + o.typeName() + "' to Float");
        }
    });

    def("String", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
        if (args.size() != 1) throw KiritoError("String expected 1 argument");
        return vm.makeString(vm.stringify(args[0]));
    });

    def("Bool", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
        if (args.size() != 1) throw KiritoError("Bool expected 1 argument");
        return vm.makeBool(vm.arena().deref(args[0]).truthy());
    });

    install<IoModule>();
    install<MathModule>();
}

inline void KiritoVM::retainChunk(std::unique_ptr<ast::Program> chunk) {
    chunks_.push_back(std::move(chunk));
}

inline KiritoVM::~KiritoVM() = default;

inline Handle KiritoVM::evalIn(std::string_view source, Handle scope) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    auto prog = std::make_unique<ast::Program>(parser.parseProgram());
    const ast::Program& program = *prog;
    retainChunk(std::move(prog));  // keep the AST alive for the VM's lifetime (closures)
    Evaluator ev(*this, scope);
    try {
        return ev.run(program);
    } catch (const KiritoThrow& t) {
        throw KiritoError("uncaught exception: " + stringify(t.value));
    }
}

inline Handle KiritoVM::runSource(std::string_view source, std::string_view) {
    return evalIn(source, newModuleScope());
}

inline Handle KiritoVM::runRepl(std::string_view source) {
    if (!replScopeReady_) {
        replScope_ = newModuleScope();
        replScopeReady_ = true;
    }
    return evalIn(source, replScope_);
}

}  // namespace kirito

#endif
