#ifndef KIRITO_RUNTIME_HPP
#define KIRITO_RUNTIME_HPP

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
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
#include "stdlib_random.hpp"
#include "stdlib_matrix.hpp"
#include "stdlib_json.hpp"
#include "stdlib_net.hpp"
#include "stdlib_serialize.hpp"
#include "stdlib_sys.hpp"
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

// Ordering for sort(): numeric or lexicographic, else a type error.
inline bool kiLessThan(KiritoVM& vm, Handle a, Handle b) {
    const Object& x = vm.arena().deref(a);
    const Object& y = vm.arena().deref(b);
    if (isNumeric(x) && isNumeric(y)) return asDouble(x) < asDouble(y);
    if (x.kind() == ValueKind::String && y.kind() == ValueKind::String)
        return static_cast<const StrVal&>(x).value() < static_cast<const StrVal&>(y).value();
    throw KiritoError("cannot order '" + x.typeName() + "' and '" + y.typeName() + "'");
}

inline Handle ListVal::getItem(KiritoVM& vm, std::span<const Handle> keys) {
    return elems[sequenceIndex(vm, elems.size(), singleKey(*this, keys))];
}
inline void ListVal::setItem(KiritoVM& vm, std::span<const Handle> keys, Handle value) {
    elems[sequenceIndex(vm, elems.size(), singleKey(*this, keys))] = value;
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
    auto self_list = [](KiritoVM& vm, Handle self) -> ListVal& {
        return static_cast<ListVal&>(vm.arena().deref(self));
    };
    if (name == "reverse")
        return vm.alloc(std::make_unique<NativeFunction>(
            "reverse", [self, self_list](KiritoVM& vm, std::span<const Handle>) -> Handle {
                auto& e = self_list(vm, self).elems;
                std::reverse(e.begin(), e.end());
                return vm.none();
            }, std::vector<Handle>{self}));
    if (name == "sort")
        return vm.alloc(std::make_unique<NativeFunction>(
            "sort", [self, self_list](KiritoVM& vm, std::span<const Handle>) -> Handle {
                auto& e = self_list(vm, self).elems;
                std::sort(e.begin(), e.end(), [&](Handle a, Handle b) { return kiLessThan(vm, a, b); });
                return vm.none();
            }, std::vector<Handle>{self}));
    if (name == "insert")
        return vm.alloc(std::make_unique<NativeFunction>(
            "insert", [self, self_list](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto& e = self_list(vm, self).elems;
                int64_t i = static_cast<const IntVal&>(vm.arena().deref(a[0])).value();
                if (i < 0) i += static_cast<int64_t>(e.size());
                if (i < 0) i = 0;
                if (i > static_cast<int64_t>(e.size())) i = static_cast<int64_t>(e.size());
                e.insert(e.begin() + i, a[1]);
                return vm.none();
            }, std::vector<Handle>{self}));
    if (name == "remove")
        return vm.alloc(std::make_unique<NativeFunction>(
            "remove", [self, self_list](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto& e = self_list(vm, self).elems;
                const Object& v = vm.arena().deref(a[0]);
                for (std::size_t i = 0; i < e.size(); ++i)
                    if (vm.arena().deref(e[i]).equals(vm.arena(), v)) { e.erase(e.begin() + i); return vm.none(); }
                throw KiritoError("remove: value not in List");
            }, std::vector<Handle>{self}));
    if (name == "index")
        return vm.alloc(std::make_unique<NativeFunction>(
            "index", [self, self_list](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto& e = self_list(vm, self).elems;
                const Object& v = vm.arena().deref(a[0]);
                for (std::size_t i = 0; i < e.size(); ++i)
                    if (vm.arena().deref(e[i]).equals(vm.arena(), v)) return vm.makeInt(static_cast<int64_t>(i));
                throw KiritoError("index: value not in List");
            }, std::vector<Handle>{self}));
    if (name == "extend")
        return vm.alloc(std::make_unique<NativeFunction>(
            "extend", [self, self_list](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto other = vm.arena().deref(a[0]).iterate(vm);
                auto& e = self_list(vm, self).elems;
                for (Handle h : other.value()) e.push_back(h);
                return vm.none();
            }, std::vector<Handle>{self}));
    if (name == "copy")
        return vm.alloc(std::make_unique<NativeFunction>(
            "copy", [self, self_list](KiritoVM& vm, std::span<const Handle>) -> Handle {
                // Shallow copy: a new List sharing the same element handles (aliasing preserved).
                auto c = std::make_unique<ListVal>();
                c->elems = self_list(vm, self).elems;
                return vm.alloc(std::move(c));
            }, std::vector<Handle>{self}));
    return Object::getAttr(vm, self, name);
}

inline Handle DictVal::getItem(KiritoVM& vm, std::span<const Handle> keys) {
    Handle key = singleKey(*this, keys);
    const Handle* v = find(vm.arena(), key);
    if (!v) throw KiritoError("key not found: " + vm.stringify(key));
    return *v;
}
inline void DictVal::setItem(KiritoVM& vm, std::span<const Handle> keys, Handle value) {
    Handle key = singleKey(*this, keys);
    set(vm.arena(), key, value);
}
inline Handle DictVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    auto bind = [&](const char* nm, NativeFn fn) {
        return vm.alloc(std::make_unique<NativeFunction>(nm, std::move(fn), std::vector<Handle>{self}));
    };
    auto dict = [](KiritoVM& vm, Handle self) -> DictVal& {
        return static_cast<DictVal&>(vm.arena().deref(self));
    };
    if (name == "keys")
        return bind("keys", [self, dict](KiritoVM& vm, std::span<const Handle>) -> Handle {
            RootScope rs(vm);
            auto list = std::make_unique<ListVal>();
            for (Handle k : dict(vm, self).keys()) list->elems.push_back(rs.add(k));
            return vm.alloc(std::move(list));
        });
    if (name == "values")
        return bind("values", [self, dict](KiritoVM& vm, std::span<const Handle>) -> Handle {
            RootScope rs(vm);
            auto list = std::make_unique<ListVal>();
            for (Handle k : dict(vm, self).keys()) list->elems.push_back(rs.add(*dict(vm, self).find(vm.arena(), k)));
            return vm.alloc(std::move(list));
        });
    if (name == "items")
        return bind("items", [self, dict](KiritoVM& vm, std::span<const Handle>) -> Handle {
            RootScope rs(vm);
            auto list = std::make_unique<ListVal>();
            for (Handle k : dict(vm, self).keys()) {
                auto pair = std::make_unique<ListVal>();
                pair->elems.push_back(k);
                pair->elems.push_back(*dict(vm, self).find(vm.arena(), k));
                list->elems.push_back(rs.add(vm.alloc(std::move(pair))));
            }
            return vm.alloc(std::move(list));
        });
    if (name == "get")
        return bind("get", [self, dict](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const Handle* v = dict(vm, self).find(vm.arena(), a[0]);
            if (v) return *v;
            return a.size() > 1 ? a[1] : vm.none();
        });
    if (name == "pop")
        return bind("pop", [self, dict](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto& d = dict(vm, self);
            const Handle* v = d.find(vm.arena(), a[0]);
            if (!v) {
                if (a.size() > 1) return a[1];
                throw KiritoError("pop: key not found");
            }
            Handle result = *v;
            d.remove(vm.arena(), a[0]);
            return result;
        });
    if (name == "remove")
        return bind("remove", [self, dict](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            dict(vm, self).remove(vm.arena(), a[0]);
            return vm.none();
        });
    if (name == "copy")
        return bind("copy", [self, dict](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto c = std::make_unique<DictVal>();
            c->buckets = dict(vm, self).buckets;
            c->count = dict(vm, self).count;
            return vm.alloc(std::move(c));
        });
    return Object::getAttr(vm, self, name);
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
    auto set_of = [](KiritoVM& vm, Handle h) -> SetVal& { return static_cast<SetVal&>(vm.arena().deref(h)); };
    auto bind = [&](const char* nm, NativeFn fn) {
        return vm.alloc(std::make_unique<NativeFunction>(nm, std::move(fn), std::vector<Handle>{self}));
    };
    if (name == "remove")
        return bind("remove", [self, set_of](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto& s = set_of(vm, self);
            const Object& v = vm.arena().deref(a[0]);
            if (!v.hashable()) throw KiritoError("unhashable type");
            auto it = s.buckets.find(v.hash());
            if (it != s.buckets.end())
                for (std::size_t i = 0; i < it->second.size(); ++i)
                    if (vm.arena().deref(it->second[i]).equals(vm.arena(), v)) {
                        it->second.erase(it->second.begin() + i);
                        --s.count;
                        return vm.none();
                    }
            throw KiritoError("remove: value not in Set");
        });
    if (name == "copy")
        return bind("copy", [self, set_of](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto c = std::make_unique<SetVal>();
            c->buckets = set_of(vm, self).buckets;
            c->count = set_of(vm, self).count;
            return vm.alloc(std::move(c));
        });
    if (name == "union" || name == "intersection" || name == "difference") {
        std::string op(name);
        return bind(op.c_str(), [self, set_of, op](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            RootScope rs(vm);
            auto result = std::make_unique<SetVal>();
            auto& s = set_of(vm, self);
            auto other = vm.arena().deref(a[0]).iterate(vm);
            if (op == "union") {
                for (Handle e : s.items()) result->add(vm.arena(), e);
                for (Handle e : other.value()) result->add(vm.arena(), e);
            } else if (op == "intersection") {
                SetVal otherSet;
                for (Handle e : other.value()) otherSet.add(vm.arena(), e);
                for (Handle e : s.items()) if (otherSet.contains(vm.arena(), e)) result->add(vm.arena(), e);
            } else {  // difference
                SetVal otherSet;
                for (Handle e : other.value()) otherSet.add(vm.arena(), e);
                for (Handle e : s.items()) if (!otherSet.contains(vm.arena(), e)) result->add(vm.arena(), e);
            }
            return vm.alloc(std::move(result));
        });
    }
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

inline Handle StrVal::getItem(KiritoVM& vm, std::span<const Handle> keys) {
    auto starts = utf8Starts(value_);
    std::size_t i = sequenceIndex(vm, starts.size(), singleKey(*this, keys));
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
    static_cast<InstanceValue&>(vm.arena().deref(instH)).selfHandle = instH;
    if (const Handle* init = findMethod(vm.arena(), "_init_")) {
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

// Invoke a class method named `method` with [self, args...]; throws `notFound` if the class chain
// lacks it. Used by the operator-protocol slots below.
inline Handle invokeOp(KiritoVM& vm, InstanceValue& inst, const char* method,
                       std::span<const Handle> args, const std::string& notFound) {
    const Handle* m = inst.findMethod(vm.arena(), method);
    if (!m) throw KiritoError(notFound);
    RootScope rs(vm);
    Handle mh = rs.add(*m);
    std::vector<Handle> full;
    full.reserve(args.size() + 1);
    full.push_back(inst.selfHandle);
    for (Handle a : args) full.push_back(rs.add(a));
    return vm.arena().deref(mh).call(vm, full);
}

inline Handle InstanceValue::binary(KiritoVM& vm, BinOp op, Handle, Handle rhs) {
    std::array<Handle, 1> a{rhs};
    return invokeOp(vm, *this, binOpMethod(op), a,
                    "'" + className + "' has no operator '" + binOpMethod(op) + "'");
}
inline Handle InstanceValue::unary(KiritoVM& vm, UnOp op, Handle) {
    return invokeOp(vm, *this, unOpMethod(op), {},
                    "'" + className + "' has no operator '" + unOpMethod(op) + "'");
}
inline Handle InstanceValue::call(KiritoVM& vm, std::span<const Handle> args) {
    return invokeOp(vm, *this, "_call_", args, "'" + className + "' object is not callable");
}
inline Handle InstanceValue::getItem(KiritoVM& vm, std::span<const Handle> keys) {
    return invokeOp(vm, *this, "_getitem_", keys, "'" + className + "' object is not indexable");
}
inline void InstanceValue::setItem(KiritoVM& vm, std::span<const Handle> keys, Handle value) {
    std::vector<Handle> args(keys.begin(), keys.end());
    args.push_back(value);
    invokeOp(vm, *this, "_setitem_", args, "'" + className + "' does not support item assignment");
}
inline std::optional<int64_t> InstanceValue::length(KiritoVM& vm) {
    Handle r = invokeOp(vm, *this, "_len_", {}, "'" + className + "' has no length");
    const Object& o = vm.arena().deref(r);
    if (o.kind() != ValueKind::Integer) throw KiritoError("_len_ must return an Integer");
    return static_cast<const IntVal&>(o).value();
}
inline bool InstanceValue::contains(KiritoVM& vm, Handle value) {
    std::array<Handle, 1> a{value};
    Handle r = invokeOp(vm, *this, "_contains_", a, "'" + className + "' does not support 'in'");
    return vm.arena().deref(r).truthy();
}
inline std::string InstanceValue::str(StringifyCtx& ctx) const {
    if (ctx.vm) {
        const Handle* m = findMethod(ctx.arena, "_str_");
        if (m) {
            if (ctx.active.count(this)) return "<" + className + " object>";  // cycle guard
            RootScope rs(*ctx.vm);
            std::array<Handle, 1> full{selfHandle};
            Handle r = rs.add(ctx.vm->arena().deref(*m).call(*ctx.vm, full));
            const Object& o = ctx.vm->arena().deref(r);
            if (o.kind() == ValueKind::String) return static_cast<const StrVal&>(o).value();
            ctx.active.insert(this);
            std::string s = o.str(ctx);
            ctx.active.erase(this);
            return s;
        }
    }
    return "<" + className + " object>";
}

// --- KiFunction call -------------------------------------------------------------------------

inline Handle KiFunction::call(KiritoVM& vm, std::span<const Handle> args) {
    CallGuard depth(vm);  // bound native-stack recursion -> catchable error, not a crash
    const auto& params = def_->params;
    if (args.size() != params.size())
        throw KiritoError("function expected " + std::to_string(params.size()) +
                          " argument(s), got " + std::to_string(args.size()));
    Handle scope = vm.newScope(closure_);
    auto& env = static_cast<EnvValue&>(vm.arena().deref(scope));
    for (std::size_t i = 0; i < params.size(); ++i) env.define(params[i], args[i]);
    Evaluator ev(vm, scope);
    if (hasOwner) ev.setCurrentClass(ownerClass);  // enables private-member access in the body
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
        RootScope rs(vm);  // keep the module alive while setup() allocates members (which may GC)
        rs.add(h);
        ModuleBuilder builder(vm, static_cast<ModuleValue&>(vm.arena().deref(h)));
        mod->setup(builder);
        return h;
    });
}

inline Handle KiritoVM::importModule(const std::string& name) {
    auto cached = moduleCache_.find(name);
    if (cached != moduleCache_.end()) return cached->second;  // modules are per-VM singletons
    auto factory = moduleFactories_.find(name);
    if (factory != moduleFactories_.end()) {
        Handle h = factory->second(*this);
        moduleCache_[name] = h;
        return h;
    }
    // Otherwise search the library paths for <name>.ki and load it as a module: the file's
    // top-level bindings become the module's members. The file is lexed+parsed+evaluated at most
    // once per VM — deduplicated by resolved absolute path, so the same file reached via different
    // module names (or repeated imports) reuses the one already-built module.
    for (const auto& dir : libPaths_) {
        std::filesystem::path path = std::filesystem::path(dir) / (name + ".ki");
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) continue;
        std::filesystem::path canon = std::filesystem::weakly_canonical(path, ec);
        std::string key = ec ? path.string() : canon.string();
        if (auto it = pathCache_.find(key); it != pathCache_.end()) {
            moduleCache_[name] = it->second;  // same file already loaded under another name
            return it->second;
        }
        std::ifstream in(path);
        std::stringstream buf;
        buf << in.rdbuf();
        Handle scope = newModuleScope();
        {
            RootScope guard(*this);
            guard.add(scope);
            auto prog = std::make_unique<ast::Program>(
                Parser(Lexer(buf.str()).tokenize()).parseProgram());
            const ast::Program& program = *prog;
            retainChunk(std::move(prog));
            Evaluator ev(*this, scope);
            ev.run(program);
            auto mod = std::make_unique<ModuleValue>(name);
            for (const auto& [k, v] : static_cast<EnvValue&>(arena_.deref(scope)).locals())
                mod->members[k] = v;
            Handle h = alloc(std::move(mod));
            moduleCache_[name] = h;
            pathCache_[key] = h;
            return h;
        }
    }
    throw KiritoError("no module named '" + name + "'");
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

    def("abs", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        const Object& o = vm.arena().deref(a[0]);
        if (o.kind() == ValueKind::Integer) return vm.makeInt(std::llabs(static_cast<const IntVal&>(o).value()));
        if (o.kind() == ValueKind::Float) return vm.makeFloat(std::fabs(static_cast<const FloatVal&>(o).value()));
        throw KiritoError("abs expects a number");
    });
    def("round", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        double x = asDouble(vm.arena().deref(a[0]));
        if (a.size() >= 2) {
            int64_t nd = static_cast<const IntVal&>(vm.arena().deref(a[1])).value();
            double f = std::pow(10.0, static_cast<double>(nd));
            return vm.makeFloat(std::round(x * f) / f);
        }
        return vm.makeInt(static_cast<int64_t>(std::llround(x)));  // Python: round(x) -> Integer
    });
    def("range", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto iv = [&](Handle h) {
            const Object& o = vm.arena().deref(h);
            if (o.kind() != ValueKind::Integer) throw KiritoError("range expects Integers");
            return static_cast<const IntVal&>(o).value();
        };
        int64_t start = 0, stop = 0, step = 1;
        if (a.size() == 1) stop = iv(a[0]);
        else if (a.size() == 2) { start = iv(a[0]); stop = iv(a[1]); }
        else if (a.size() == 3) { start = iv(a[0]); stop = iv(a[1]); step = iv(a[2]); }
        else throw KiritoError("range expects 1 to 3 arguments");
        if (step == 0) throw KiritoError("range step cannot be zero");
        RootScope rs(vm);
        auto list = std::make_unique<ListVal>();
        if (step > 0) for (int64_t i = start; i < stop; i += step) list->elems.push_back(rs.add(vm.makeInt(i)));
        else for (int64_t i = start; i > stop; i += step) list->elems.push_back(rs.add(vm.makeInt(i)));
        return vm.alloc(std::move(list));
    });
    def("sum", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto items = vm.arena().deref(a[0]).iterate(vm);
        bool isFloat = false;
        double f = 0;
        int64_t n = 0;
        for (Handle h : items.value()) {
            const Object& o = vm.arena().deref(h);
            if (o.kind() == ValueKind::Float) { isFloat = true; f += static_cast<const FloatVal&>(o).value(); }
            else if (o.kind() == ValueKind::Integer) { int64_t v = static_cast<const IntVal&>(o).value(); n += v; f += static_cast<double>(v); }
            else throw KiritoError("sum expects numbers");
        }
        return isFloat ? vm.makeFloat(f) : vm.makeInt(n);
    });
    auto extremum = [](KiritoVM& vm, std::span<const Handle> a, bool wantMax) -> Handle {
        std::vector<Handle> items;
        if (a.size() == 1) items = vm.arena().deref(a[0]).iterate(vm).value();
        else items.assign(a.begin(), a.end());
        if (items.empty()) throw KiritoError("min/max of empty sequence");
        Handle best = items[0];
        for (std::size_t i = 1; i < items.size(); ++i) {
            bool better = wantMax ? kiLessThan(vm, best, items[i]) : kiLessThan(vm, items[i], best);
            if (better) best = items[i];
        }
        return best;
    };
    def("min", [extremum](KiritoVM& vm, std::span<const Handle> a) { return extremum(vm, a, false); });
    def("max", [extremum](KiritoVM& vm, std::span<const Handle> a) { return extremum(vm, a, true); });
    def("sorted", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        RootScope rs(vm);
        auto list = std::make_unique<ListVal>();
        auto items = vm.arena().deref(a[0]).iterate(vm);
        for (Handle h : items.value()) list->elems.push_back(rs.add(h));
        std::sort(list->elems.begin(), list->elems.end(), [&](Handle x, Handle y) { return kiLessThan(vm, x, y); });
        return vm.alloc(std::move(list));
    });
    def("enumerate", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        RootScope rs(vm);
        auto out = std::make_unique<ListVal>();
        auto items = vm.arena().deref(a[0]).iterate(vm);
        int64_t i = 0;
        for (Handle h : items.value()) {
            rs.add(h);
            auto pair = std::make_unique<ListVal>();
            pair->elems.push_back(vm.makeInt(i++));
            pair->elems.push_back(h);
            out->elems.push_back(rs.add(vm.alloc(std::move(pair))));
        }
        return vm.alloc(std::move(out));
    });
    def("zip", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        RootScope rs(vm);
        std::vector<std::vector<Handle>> cols;
        std::size_t minLen = SIZE_MAX;
        for (Handle h : a) { cols.push_back(vm.arena().deref(h).iterate(vm).value()); minLen = std::min(minLen, cols.back().size()); }
        if (cols.empty()) minLen = 0;
        auto out = std::make_unique<ListVal>();
        for (std::size_t i = 0; i < minLen; ++i) {
            auto tup = std::make_unique<ListVal>();
            for (auto& c : cols) tup->elems.push_back(c[i]);
            out->elems.push_back(rs.add(vm.alloc(std::move(tup))));
        }
        return vm.alloc(std::move(out));
    });
    def("map", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        RootScope rs(vm);
        Handle f = a[0];
        auto out = std::make_unique<ListVal>();
        auto items = vm.arena().deref(a[1]).iterate(vm);
        for (Handle x : items.value()) {
            rs.add(x);
            std::array<Handle, 1> args{x};
            out->elems.push_back(rs.add(vm.arena().deref(f).call(vm, args)));
        }
        return vm.alloc(std::move(out));
    });
    def("filter", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        RootScope rs(vm);
        Handle f = a[0];
        auto out = std::make_unique<ListVal>();
        auto items = vm.arena().deref(a[1]).iterate(vm);
        for (Handle x : items.value()) {
            rs.add(x);
            std::array<Handle, 1> args{x};
            if (vm.arena().deref(vm.arena().deref(f).call(vm, args)).truthy()) out->elems.push_back(x);
        }
        return vm.alloc(std::move(out));
    });
    def("type", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        return vm.makeString(vm.arena().deref(a[0]).typeName());
    });
}

// Register the bundled standard-library modules. Each line is a one-liner; a third party adds
// their own module exactly the same way: #include the module's header, then vm.install<T>().
inline void KiritoVM::installStandardLibrary() {
    install<IoModule>();
    install<MathModule>();
    install<RandomModule>();
    install<MatrixModule>();
    install<JsonModule>();
    install<NetModule>();
    install<SerializeModule>();
    install<SysModule>();
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
