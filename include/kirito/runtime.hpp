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
#include "stdlib_time.hpp"
#include "stdlib_dump.hpp"
#include "stdlib_zlib.hpp"
#include "stdlib_hash.hpp"
#include "stdlib_kimodules.hpp"
#include "vm.hpp"

// Definitions that need a complete KiritoVM (and the front end): they live here, included last,
// so the per-component headers stay free of upward dependencies.

namespace kirito {

// Upper bound on the size of a single string/collection built by repetition or padding, so a
// hostile or careless count (e.g. "x" * 10**12, "".ljust(10**9)) raises cleanly instead of OOMing
// the host. ~256 MB of characters is far beyond any legitimate scripting use.
inline constexpr uint64_t kMaxRepeat = 256ull * 1024 * 1024;

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
// Two's-complement wraparound for the int64 operators. Signed overflow is UB in C++, so we do the
// arithmetic in uint64_t (where wraparound is defined) and reinterpret — giving consistent,
// well-defined behavior on overflow instead of undefined behavior. (Kirito ints are fixed int64;
// arbitrary-precision integers are a future enrichment.)
inline int64_t wadd(int64_t a, int64_t b) {
    return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}
inline int64_t wsub(int64_t a, int64_t b) {
    return static_cast<int64_t>(static_cast<uint64_t>(a) - static_cast<uint64_t>(b));
}
inline int64_t wmul(int64_t a, int64_t b) {
    return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
}
inline int64_t ifloordiv(int64_t a, int64_t b) {
    if (b == -1) return wsub(0, a);  // INT64_MIN / -1 would overflow; wrap instead of UB
    int64_t q = a / b, r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}
inline int64_t imod(int64_t a, int64_t b) {
    if (b == -1) return 0;           // a % -1 is always 0 (avoids the INT64_MIN/-1 UB)
    int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}
inline int64_t ipow(int64_t base, int64_t exp) {
    int64_t result = 1;
    while (exp > 0) {
        if (exp & 1) result = wmul(result, base);
        exp >>= 1;
        if (exp) base = wmul(base, base);
    }
    return result;
}

// Shared arithmetic dispatch for Integer/Float. Integer⊕Integer stays Integer (except `/`),
// any Float promotes to Float, and `/` always yields Float.
inline Handle numericBinary(KiritoVM& vm, BinOp op, Handle aH, Handle bH) {
    const Object& a = vm.arena().deref(aH);
    const Object& b = vm.arena().deref(bH);
    if (!isNumeric(b)) {
        bool cmp = op == BinOp::Lt || op == BinOp::Le || op == BinOp::Gt || op == BinOp::Ge;
        throw KiritoError("unsupported operand type '" + b.typeName() + "' for " +
                          (cmp ? "comparison" : "arithmetic") + " with '" + a.typeName() + "'");
    }

    if (op == BinOp::Div) {
        double db = asDouble(b);
        if (db == 0.0) throw KiritoError("division by zero");
        return vm.makeFloat(asDouble(a) / db);
    }

    // Integer-vs-Integer is the common hot case: compare/arith directly in int64 (no double round-
    // trip, so large magnitudes compare exactly and it's faster).
    if (a.kind() == ValueKind::Integer && b.kind() == ValueKind::Integer) {
        int64_t x = static_cast<const IntVal&>(a).value();
        int64_t y = static_cast<const IntVal&>(b).value();
        switch (op) {
            case BinOp::Lt: return vm.makeBool(x < y);
            case BinOp::Le: return vm.makeBool(x <= y);
            case BinOp::Gt: return vm.makeBool(x > y);
            case BinOp::Ge: return vm.makeBool(x >= y);
            case BinOp::Add: return vm.makeInt(wadd(x, y));
            case BinOp::Sub: return vm.makeInt(wsub(x, y));
            case BinOp::Mul: return vm.makeInt(wmul(x, y));
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
    }

    switch (op) {
        case BinOp::Lt: return vm.makeBool(asDouble(a) < asDouble(b));
        case BinOp::Le: return vm.makeBool(asDouble(a) <= asDouble(b));
        case BinOp::Gt: return vm.makeBool(asDouble(a) > asDouble(b));
        case BinOp::Ge: return vm.makeBool(asDouble(a) >= asDouble(b));
        default: break;
    }

    {
        // At least one operand is a Float (Integer×Integer was handled above): promote and compute.
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
    if (op == UnOp::Neg) return vm.makeInt(wsub(0, value_));  // wrap (-INT64_MIN would be UB)
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
        if (n <= 0 || value_.empty()) return vm.makeString("");
        // Guard against absurd allocations from a dumb/hostile count (e.g. "x" * 10**12): reject up
        // front rather than OOM the host. The product is computed in unsigned to avoid overflow UB.
        if (static_cast<uint64_t>(n) > kMaxRepeat / value_.size())
            throw KiritoError("repeated String too large");
        std::string out;
        out.reserve(value_.size() * static_cast<std::size_t>(n));
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

// Ordering for sort()/comparisons: numbers numerically, Strings and Lists lexicographically, else a
// type error. List ordering compares element-by-element (recursively) and breaks ties by length,
// matching Python — enabling the common multi-key sort idiom (key returns a [k1, k2, ...] list).
inline bool kiLessThan(KiritoVM& vm, Handle a, Handle b) {
    const Object& x = vm.arena().deref(a);
    const Object& y = vm.arena().deref(b);
    if (isNumeric(x) && isNumeric(y)) return asDouble(x) < asDouble(y);
    if (x.kind() == ValueKind::String && y.kind() == ValueKind::String)
        return static_cast<const StrVal&>(x).value() < static_cast<const StrVal&>(y).value();
    if ((x.kind() == ValueKind::List || x.kind() == ValueKind::Array) &&
        (y.kind() == ValueKind::List || y.kind() == ValueKind::Array)) {
        const auto& xe = static_cast<const ListVal&>(x).elems;
        const auto& ye = static_cast<const ListVal&>(y).elems;
        std::size_t common = std::min(xe.size(), ye.size());
        for (std::size_t i = 0; i < common; ++i) {
            if (kiLessThan(vm, xe[i], ye[i])) return true;
            if (kiLessThan(vm, ye[i], xe[i])) return false;
        }
        return xe.size() < ye.size();  // shared prefix equal: shorter list is "less"
    }
    throw KiritoError("cannot order '" + x.typeName() + "' and '" + y.typeName() + "'");
}

inline Handle ListVal::getItem(KiritoVM& vm, std::span<const Handle> keys) {
    return elems[sequenceIndex(vm, elems.size(), singleKey(*this, keys))];
}
inline void ListVal::setItem(KiritoVM& vm, std::span<const Handle> keys, Handle value) {
    elems[sequenceIndex(vm, elems.size(), singleKey(*this, keys))] = value;
}
inline Handle ListVal::binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) {
    const Object& b = vm.arena().deref(rhs);
    // `+` concatenates two Lists into a new List (Python-style).
    if (op == BinOp::Add) {
        if (b.kind() != ValueKind::List && b.kind() != ValueKind::Array)
            throw KiritoError("can only concatenate List to List, not '" + b.typeName() + "'");
        RootScope rs(vm);
        auto out = std::make_unique<ListVal>();
        out->elems = elems;
        const auto& be = static_cast<const ListVal&>(b).elems;
        out->elems.insert(out->elems.end(), be.begin(), be.end());
        return vm.alloc(std::move(out));
    }
    // Ordering: lexicographic, element-by-element (via kiLessThan), only against another List.
    if (op == BinOp::Lt || op == BinOp::Le || op == BinOp::Gt || op == BinOp::Ge) {
        if (b.kind() != ValueKind::List && b.kind() != ValueKind::Array)
            throw KiritoError("cannot order 'List' and '" + b.typeName() + "'");
        bool lt = kiLessThan(vm, self, rhs);
        bool gt = kiLessThan(vm, rhs, self);
        switch (op) {
            case BinOp::Lt: return vm.makeBool(lt);
            case BinOp::Le: return vm.makeBool(!gt);
            case BinOp::Gt: return vm.makeBool(gt);
            case BinOp::Ge: return vm.makeBool(!lt);
            default: break;
        }
    }
    return Object::binary(vm, op, self, rhs);
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
            [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                auto& list = static_cast<ListVal&>(vm.arena().deref(self));
                if (list.elems.empty()) throw KiritoError("pop from empty List");
                // pop() removes the last element; pop(i) removes (and returns) index i (negative
                // counts from the end, like Python).
                int64_t idx = static_cast<int64_t>(list.elems.size()) - 1;
                if (!a.empty()) {
                    const Object& o = vm.arena().deref(a[0]);
                    if (o.kind() != ValueKind::Integer) throw KiritoError("pop index must be an Integer");
                    idx = static_cast<const IntVal&>(o).value();
                    if (idx < 0) idx += static_cast<int64_t>(list.elems.size());
                    if (idx < 0 || idx >= static_cast<int64_t>(list.elems.size()))
                        throw KiritoError("pop index out of range");
                }
                Handle v = list.elems[static_cast<std::size_t>(idx)];
                list.elems.erase(list.elems.begin() + idx);
                return v;
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
            "sort", [self, self_list](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                // sort([key][, reverse]) — STABLE in-place sort. `key` is an optional callable
                // mapping each element to a sort key; `reverse` (a truthy 2nd arg) descends.
                Handle keyFn{};
                bool hasKey = false, reverse = false;
                if (!a.empty() && vm.arena().deref(a[0]).kind() != ValueKind::None) {
                    keyFn = a[0];
                    hasKey = true;
                }
                if (a.size() > 1) reverse = vm.arena().deref(a[1]).truthy();
                auto& e = self_list(vm, self).elems;
                // Precompute keys once per element (Schwartzian transform): avoids re-invoking the
                // key function O(n log n) times and keeps the comparator allocation-free. The keys
                // are GC-rooted for the duration of the sort.
                RootScope rs(vm);
                std::vector<std::pair<Handle, Handle>> tagged;  // (key, element)
                tagged.reserve(e.size());
                for (Handle el : e) {
                    Handle k = el;
                    if (hasKey) {
                        std::array<Handle, 1> args{el};
                        k = rs.add(vm.arena().deref(keyFn).call(vm, args));
                    }
                    tagged.emplace_back(k, el);
                }
                std::stable_sort(tagged.begin(), tagged.end(),
                                 [&](const std::pair<Handle, Handle>& x, const std::pair<Handle, Handle>& y) {
                                     return reverse ? kiLessThan(vm, y.first, x.first)
                                                    : kiLessThan(vm, x.first, y.first);
                                 });
                for (std::size_t i = 0; i < e.size(); ++i) e[i] = tagged[i].second;
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
    if (name == "clear")
        return vm.alloc(std::make_unique<NativeFunction>(
            "clear", [self, self_list](KiritoVM& vm, std::span<const Handle>) -> Handle {
                self_list(vm, self).elems.clear();
                return vm.none();
            }, std::vector<Handle>{self}));
    if (name == "count")
        return vm.alloc(std::make_unique<NativeFunction>(
            "count", [self, self_list](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                if (a.size() != 1) throw KiritoError("count expected 1 argument");
                auto& e = self_list(vm, self).elems;
                int64_t n = 0;
                for (Handle h : e)
                    if (vm.arena().deref(h).equals(vm.arena(), vm.arena().deref(a[0]))) ++n;
                return vm.makeInt(n);
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
    if (name == "clear")
        return bind("clear", [self, dict](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto& d = dict(vm, self);
            d.buckets.clear();
            d.count = 0;
            return vm.none();
        });
    if (name == "update")
        return bind("update", [self, dict](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            // update(other): merge another Dict (its entries override).
            if (a.size() != 1) throw KiritoError("update expected 1 argument");
            const Object& o = vm.arena().deref(a[0]);
            if (o.kind() != ValueKind::Dict) throw KiritoError("update expects a Dict");
            for (const auto& [k, v] : static_cast<const DictVal&>(o).pairs())
                dict(vm, self).set(vm.arena(), k, v);
            return vm.none();
        });
    if (name == "setdefault")
        return bind("setdefault", [self, dict](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            // setdefault(key[, default]): return existing value, else insert default (or None).
            if (a.empty()) throw KiritoError("setdefault expected at least 1 argument");
            auto& d = dict(vm, self);
            const Handle* v = d.find(vm.arena(), a[0]);
            if (v) return *v;
            Handle dflt = a.size() > 1 ? a[1] : vm.none();
            d.set(vm.arena(), a[0], dflt);
            return dflt;
        });
    if (name == "popitem")
        return bind("popitem", [self, dict](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto& d = dict(vm, self);
            auto ks = d.keys();
            if (ks.empty()) throw KiritoError("popitem: dictionary is empty");
            RootScope rs(vm);
            Handle k = rs.add(ks.back());
            Handle v = rs.add(*d.find(vm.arena(), k));
            d.remove(vm.arena(), k);
            auto pair = std::make_unique<ListVal>();
            pair->elems.push_back(k);
            pair->elems.push_back(v);
            return vm.alloc(std::move(pair));
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
    if (name == "discard")  // remove if present, no error otherwise (cf. remove)
        return bind("discard", [self, set_of](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto& s = set_of(vm, self);
            const Object& v = vm.arena().deref(a[0]);
            if (!v.hashable()) return vm.none();
            auto it = s.buckets.find(v.hash());
            if (it != s.buckets.end())
                for (std::size_t i = 0; i < it->second.size(); ++i)
                    if (vm.arena().deref(it->second[i]).equals(vm.arena(), v)) {
                        it->second.erase(it->second.begin() + i);
                        --s.count;
                        break;
                    }
            return vm.none();
        });
    if (name == "clear")
        return bind("clear", [self, set_of](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto& s = set_of(vm, self);
            s.buckets.clear();
            s.count = 0;
            return vm.none();
        });
    if (name == "pop")  // remove and return an arbitrary element
        return bind("pop", [self, set_of](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto& s = set_of(vm, self);
            for (auto& [h, bucket] : s.buckets)
                if (!bucket.empty()) {
                    Handle v = bucket.back();
                    bucket.pop_back();
                    --s.count;
                    return v;
                }
            throw KiritoError("pop from an empty Set");
        });
    if (name == "union" || name == "intersection" || name == "difference" ||
        name == "symmetric_difference") {
        std::string op(name);
        return bind(op.c_str(), [self, set_of, op](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            RootScope rs(vm);
            auto result = std::make_unique<SetVal>();
            auto& s = set_of(vm, self);
            auto other = vm.arena().deref(a[0]).iterate(vm);
            if (!other) throw KiritoError(op + " expects an iterable");
            if (op == "union") {
                for (Handle e : s.items()) result->add(vm.arena(), e);
                for (Handle e : other.value()) result->add(vm.arena(), e);
            } else if (op == "intersection") {
                SetVal otherSet;
                for (Handle e : other.value()) otherSet.add(vm.arena(), e);
                for (Handle e : s.items()) if (otherSet.contains(vm.arena(), e)) result->add(vm.arena(), e);
            } else if (op == "difference") {
                SetVal otherSet;
                for (Handle e : other.value()) otherSet.add(vm.arena(), e);
                for (Handle e : s.items()) if (!otherSet.contains(vm.arena(), e)) result->add(vm.arena(), e);
            } else {  // symmetric_difference: in one but not both
                SetVal otherSet;
                for (Handle e : other.value()) otherSet.add(vm.arena(), e);
                for (Handle e : s.items()) if (!otherSet.contains(vm.arena(), e)) result->add(vm.arena(), e);
                for (Handle e : otherSet.items()) if (!s.contains(vm.arena(), e)) result->add(vm.arena(), e);
            }
            return vm.alloc(std::move(result));
        });
    }
    if (name == "issubset" || name == "issuperset" || name == "isdisjoint") {
        std::string op(name);
        return bind(op.c_str(), [self, set_of, op](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto& s = set_of(vm, self);
            auto other = vm.arena().deref(a[0]).iterate(vm);
            if (!other) throw KiritoError(op + " expects an iterable");
            SetVal otherSet;
            for (Handle e : other.value()) otherSet.add(vm.arena(), e);
            if (op == "issubset") {
                for (Handle e : s.items()) if (!otherSet.contains(vm.arena(), e)) return vm.makeBool(false);
                return vm.makeBool(true);
            }
            if (op == "issuperset") {
                for (Handle e : otherSet.items()) if (!s.contains(vm.arena(), e)) return vm.makeBool(false);
                return vm.makeBool(true);
            }
            for (Handle e : s.items()) if (otherSet.contains(vm.arena(), e)) return vm.makeBool(false);  // isdisjoint
            return vm.makeBool(true);
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
    if (isAscii()) {  // O(1): code-point index == byte index
        std::size_t i = sequenceIndex(vm, value_.size(), singleKey(*this, keys));
        return vm.makeString(value_.substr(i, 1));
    }
    const auto& starts = codePointStarts();  // cached
    std::size_t i = sequenceIndex(vm, starts.size(), singleKey(*this, keys));
    std::size_t b = starts[i];
    std::size_t e = (i + 1 < starts.size()) ? starts[i + 1] : value_.size();
    return vm.makeString(value_.substr(b, e - b));
}

inline Handle StrVal::slice(KiritoVM& vm, Handle s, Handle e, Handle st) {
    if (isAscii()) {  // byte slice == code-point slice
        int64_t len = static_cast<int64_t>(value_.size());
        std::string out;
        for (int64_t i : pythonSliceIndices(vm, len, s, e, st)) out += value_[static_cast<std::size_t>(i)];
        return vm.makeString(std::move(out));
    }
    const auto& starts = codePointStarts();
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
    std::vector<Handle> out;
    if (isAscii()) {
        out.reserve(value_.size());
        for (char c : value_) out.push_back(rs.add(vm.makeString(std::string(1, c))));
        return out;
    }
    const auto& starts = codePointStarts();
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

    // Code-point-aware case conversion (handles ASCII + Latin-1 + Latin Extended-A, so Polish and
    // most European text map correctly, not just ASCII).
    auto mapCase = [](const std::string& s, unsigned (*fn)(unsigned)) {
        std::string out;
        out.reserve(s.size());
        for (std::size_t st : utf8Starts(s)) utf8Encode(fn(utf8DecodeAt(s, st)), out);
        return out;
    };
    if (name == "upper")
        return bind("upper", [self, recv, mapCase](KiritoVM& vm, std::span<const Handle>) {
            return vm.makeString(mapCase(recv(vm, self), utf8ToUpperCp));
        });
    if (name == "lower")
        return bind("lower", [self, recv, mapCase](KiritoVM& vm, std::span<const Handle>) {
            return vm.makeString(mapCase(recv(vm, self), utf8ToLowerCp));
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
                    std::size_t idx;
                    if (spec.empty()) {
                        idx = auto_i++;
                    } else {
                        for (char ch : spec)
                            if (ch < '0' || ch > '9') throw KiritoError("format field must be an index");
                        try { idx = static_cast<std::size_t>(std::stoull(spec)); }
                        catch (const std::out_of_range&) { throw KiritoError("format index out of range"); }
                    }
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
    // index(sub): like find but raises if not found.
    if (name == "index")
        return bind("index", [self, recv](KiritoVM& vm, std::span<const Handle> a) {
            const std::string& s = recv(vm, self);
            const std::string& sub = asStr(vm, a[0], "index");
            std::size_t bp = s.find(sub);
            if (bp == std::string::npos) throw KiritoError("substring not found");
            int64_t cp = 0;
            for (std::size_t st : utf8Starts(s)) { if (st >= bp) break; ++cp; }
            return vm.makeInt(cp);
        });
    if (name == "rfind" || name == "rindex") {
        bool raise = name == "rindex";
        return bind(std::string(name).c_str(), [self, recv, raise](KiritoVM& vm, std::span<const Handle> a) {
            const std::string& s = recv(vm, self);
            const std::string& sub = asStr(vm, a[0], "rfind");
            std::size_t bp = s.rfind(sub);
            if (bp == std::string::npos) {
                if (raise) throw KiritoError("substring not found");
                return vm.makeInt(-1);
            }
            int64_t cp = 0;
            for (std::size_t st : utf8Starts(s)) { if (st >= bp) break; ++cp; }
            return vm.makeInt(cp);
        });
    }
    // Character-class predicates (non-empty string, all code points satisfy the test).
    auto classify = [&](const char* nm, bool (*test)(unsigned)) {
        return bind(nm, [self, recv, test](KiritoVM& vm, std::span<const Handle>) -> Handle {
            const std::string& s = recv(vm, self);
            if (s.empty()) return vm.makeBool(false);
            for (std::size_t st : utf8Starts(s)) if (!test(utf8DecodeAt(s, st))) return vm.makeBool(false);
            return vm.makeBool(true);
        });
    };
    if (name == "isdigit") return classify("isdigit", [](unsigned c) { return c >= '0' && c <= '9'; });
    if (name == "isalpha") return classify("isalpha", [](unsigned c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0x80; });  // non-ASCII letters count
    if (name == "isalnum") return classify("isalnum", [](unsigned c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c >= 0x80; });
    if (name == "isspace") return classify("isspace", [](unsigned c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; });
    if (name == "islower") return classify("islower", [](unsigned c) {
        return !((c >= 'A' && c <= 'Z')) && (utf8ToUpperCp(c) != c || (c >= 'a' && c <= 'z')); });
    if (name == "isupper") return classify("isupper", [](unsigned c) {
        return !((c >= 'a' && c <= 'z')) && (utf8ToLowerCp(c) != c || (c >= 'A' && c <= 'Z')); });
    // removeprefix / removesuffix (Python 3.9).
    if (name == "removeprefix")
        return bind("removeprefix", [self, recv](KiritoVM& vm, std::span<const Handle> a) {
            const std::string& s = recv(vm, self);
            const std::string& p = asStr(vm, a[0], "removeprefix");
            if (s.size() >= p.size() && s.compare(0, p.size(), p) == 0) return vm.makeString(s.substr(p.size()));
            return vm.makeString(s);
        });
    if (name == "removesuffix")
        return bind("removesuffix", [self, recv](KiritoVM& vm, std::span<const Handle> a) {
            const std::string& s = recv(vm, self);
            const std::string& p = asStr(vm, a[0], "removesuffix");
            if (!p.empty() && s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0)
                return vm.makeString(s.substr(0, s.size() - p.size()));
            return vm.makeString(s);
        });
    // Padding/alignment by code-point width.
    if (name == "ljust" || name == "rjust" || name == "center") {
        std::string op(name);
        return bind(op.c_str(), [self, recv, op](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const std::string& s = recv(vm, self);
            if (a.empty() || vm.arena().deref(a[0]).kind() != ValueKind::Integer)
                throw KiritoError(op + " expects an Integer width");
            int64_t width = static_cast<const IntVal&>(vm.arena().deref(a[0])).value();
            if (static_cast<uint64_t>(width < 0 ? 0 : width) > kMaxRepeat)
                throw KiritoError(op + " width too large");
            std::string fill = a.size() > 1 ? asStr(vm, a[1], op.c_str()) : " ";
            if (utf8Length(fill) != 1) throw KiritoError(op + " fill must be a single character");
            int64_t pad = width - static_cast<int64_t>(utf8Length(s));
            if (pad <= 0) return vm.makeString(s);
            auto rep = [&](int64_t n) { std::string r; for (int64_t i = 0; i < n; ++i) r += fill; return r; };
            if (op == "ljust") return vm.makeString(s + rep(pad));
            if (op == "rjust") return vm.makeString(rep(pad) + s);
            return vm.makeString(rep(pad / 2) + s + rep(pad - pad / 2));  // center
        });
    }
    if (name == "zfill")
        return bind("zfill", [self, recv](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const std::string& s = recv(vm, self);
            if (a.empty() || vm.arena().deref(a[0]).kind() != ValueKind::Integer)
                throw KiritoError("zfill expects an Integer width");
            int64_t width = static_cast<const IntVal&>(vm.arena().deref(a[0])).value();
            if (static_cast<uint64_t>(width < 0 ? 0 : width) > kMaxRepeat)
                throw KiritoError("zfill width too large");
            int64_t pad = width - static_cast<int64_t>(utf8Length(s));
            if (pad <= 0) return vm.makeString(s);
            // Keep a leading sign in front of the zero padding.
            std::string sign, body = s;
            if (!s.empty() && (s[0] == '+' || s[0] == '-')) { sign = s.substr(0, 1); body = s.substr(1); }
            return vm.makeString(sign + std::string(static_cast<std::size_t>(pad), '0') + body);
        });
    if (name == "partition" || name == "rpartition") {
        bool right = name == "rpartition";
        return bind(std::string(name).c_str(), [self, recv, right](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            const std::string& s = recv(vm, self);
            const std::string& sep = asStr(vm, a[0], "partition");
            if (sep.empty()) throw KiritoError("empty separator");
            std::size_t pos = right ? s.rfind(sep) : s.find(sep);
            RootScope rs(vm);
            auto t = std::make_unique<ListVal>();
            if (pos == std::string::npos) {
                t->elems.push_back(rs.add(vm.makeString(right ? "" : s)));
                t->elems.push_back(rs.add(vm.makeString("")));
                t->elems.push_back(rs.add(vm.makeString(right ? s : "")));
            } else {
                t->elems.push_back(rs.add(vm.makeString(s.substr(0, pos))));
                t->elems.push_back(rs.add(vm.makeString(sep)));
                t->elems.push_back(rs.add(vm.makeString(s.substr(pos + sep.size()))));
            }
            return vm.alloc(std::move(t));
        });
    }
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

inline Handle SuperValue::getAttr(KiritoVM& vm, Handle, std::string_view name) {
    // Resolve the named method starting at startClass (the base of the current method's class), then
    // bind it to the ORIGINAL instance so the inherited method runs against the real self.
    const auto& base = static_cast<const ClassValue&>(vm.arena().deref(startClass));
    const Handle* method = base.findMethod(vm.arena(), std::string(name));
    if (!method)
        throw KiritoError("'super' object has no attribute '" + std::string(name) + "'");
    Handle methodH = *method;
    Handle inst = instance;
    return vm.alloc(std::make_unique<NativeFunction>(
        std::string(name),
        [inst, methodH](KiritoVM& vm, std::span<const Handle> args) -> Handle {
            std::vector<Handle> full;
            full.reserve(args.size() + 1);
            full.push_back(inst);
            for (Handle a : args) full.push_back(a);
            return vm.arena().deref(methodH).call(vm, full);
        },
        std::vector<Handle>{inst, methodH}));
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

inline std::optional<std::vector<Handle>> InstanceValue::iterate(KiritoVM& vm) {
    // A class becomes iterable by defining _iter_(self) returning any iterable (commonly a List).
    if (!findMethod(vm.arena(), "_iter_")) return std::nullopt;
    Handle r = invokeOp(vm, *this, "_iter_", {}, "'" + className + "' is not iterable");
    RootScope rs(vm);
    rs.add(r);
    return vm.arena().deref(r).iterate(vm);
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

// The canonical type-name a value matches for annotation checks. Built-ins map to their kind;
// user instances report their class name (and inheritance is handled separately by typeMatches).
inline std::string annotationTypeName(ValueKind k) {
    switch (k) {
        case ValueKind::None: return "None";
        case ValueKind::Bool: return "Bool";
        case ValueKind::Integer: return "Integer";
        case ValueKind::Float: return "Float";
        case ValueKind::String: return "String";
        case ValueKind::List: return "List";
        case ValueKind::Dict: return "Dict";
        case ValueKind::Set: return "Set";
        case ValueKind::Function: case ValueKind::NativeFunction: return "Function";
        case ValueKind::Module: return "Module";
        case ValueKind::Class: return "Class";
        default: return "";
    }
}

// Does `value` satisfy the type annotation `typeName`? Built-in type names match by kind; "Any"
// always matches; otherwise treat it as a class name and check the instance's class chain (so
// subclasses pass) — and also accept a NativeClass whose typeName equals the annotation.
inline bool typeMatches(KiritoVM& vm, Handle value, const std::string& typeName) {
    if (typeName.empty() || typeName == "Any") return true;
    const Object& o = vm.arena().deref(value);
    // built-in kind names
    if (annotationTypeName(o.kind()) == typeName) return true;
    // a user instance: walk its class chain by name (inheritance-aware)
    if (o.kind() == ValueKind::Instance) {
        const auto* inst = dynamic_cast<const InstanceValue*>(&o);
        if (inst) {
            Handle cur = inst->cls;
            while (true) {
                const auto& c = static_cast<const ClassValue&>(vm.arena().deref(cur));
                if (c.name == typeName) return true;
                if (!c.hasBase) break;
                cur = c.base;
            }
        }
        // a C++ NativeClass instance (Matrix, Socket, ...): match its own type name
        if (o.typeName() == typeName) return true;
    }
    return false;
}

inline Handle KiFunction::call(KiritoVM& vm, std::span<const Handle> args) {
    return callFull(vm, args, {});
}

inline Handle KiFunction::callFull(KiritoVM& vm, std::span<const Handle> positional,
                                   std::span<const NamedArg> named) {
    CallGuard depth(vm);  // bound native-stack recursion -> catchable error, not a crash
    const auto& params = def_->params;
    RootScope rs(vm);
    Handle scope = rs.add(vm.newScope(closure_));
    auto& env = static_cast<EnvValue&>(vm.arena().deref(scope));
    env.reserve(params.size());

    // Fast path: the overwhelmingly common call shape — only positional args, exact arity, no
    // type annotations to check. Bind straight into the scope with no temporaries.
    if (!def_->fastBindable.has_value()) {
        bool simple = def_->returnAnnotation.empty();
        for (const auto& p : params)
            if (!p.annotation.empty()) { simple = false; break; }
        def_->fastBindable = simple;  // memoize the per-def annotation check (def_ field is mutable)
    }
    if (named.empty() && positional.size() == params.size() && *def_->fastBindable) {
        for (std::size_t i = 0; i < params.size(); ++i) env.define(params[i].name, positional[i]);
        Evaluator evf(vm, scope);
        if (hasOwner) evf.setCurrentClass(ownerClass);
        return evf.callBody(def_->body);
    }

    std::vector<bool> bound(params.size(), false);
    std::vector<Handle> values(params.size());

    if (positional.size() > params.size())
        throw KiritoError("function takes " + std::to_string(params.size()) + " positional argument(s) but " +
                          std::to_string(positional.size()) + " were given");
    for (std::size_t i = 0; i < positional.size(); ++i) { values[i] = positional[i]; bound[i] = true; }

    // named args: match each to a parameter by name
    for (const auto& na : named) {
        std::size_t idx = params.size();
        for (std::size_t i = 0; i < params.size(); ++i)
            if (params[i].name == na.name) { idx = i; break; }
        if (idx == params.size())
            throw KiritoError("function got an unexpected keyword argument '" + na.name + "'");
        if (bound[idx])
            throw KiritoError("function got multiple values for argument '" + na.name + "'");
        values[idx] = na.value;
        bound[idx] = true;
    }

    // fill defaults / report missing, then enforce annotations
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (!bound[i]) {
            if (params[i].defaultValue) {
                Evaluator dev(vm, scope);  // defaults evaluate in the call scope (closure-visible)
                values[i] = rs.add(dev.eval(*params[i].defaultValue));
            } else {
                throw KiritoError("function missing required argument '" + params[i].name + "'");
            }
        }
        if (!params[i].annotation.empty() && !typeMatches(vm, values[i], params[i].annotation))
            throw KiritoError("argument '" + params[i].name + "' must be " + params[i].annotation +
                              ", got " + vm.arena().deref(values[i]).typeName());
        env.define(params[i].name, values[i]);
    }

    Evaluator ev(vm, scope);
    if (hasOwner) ev.setCurrentClass(ownerClass);  // enables private-member access in the body
    Handle result = ev.callBody(def_->body);

    // enforce the return annotation
    if (!def_->returnAnnotation.empty() && !typeMatches(vm, result, def_->returnAnnotation))
        throw KiritoError("function must return " + def_->returnAnnotation + ", got " +
                          vm.arena().deref(result).typeName());
    return result;
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
        ModuleBuilder builder(vm, h, static_cast<ModuleValue&>(vm.arena().deref(h)));
        mod->setup(builder);
        return h;
    });
}

inline void KiritoVM::registerSourceModule(std::string name, std::string_view source) {
    // A frozen module: the Kirito `source` is compiled and run in a fresh module scope on first
    // import, and its top-level bindings become the module's members. Cached like any module.
    std::string src(source);
    std::string modName = name;
    registerModule(std::move(name), [src, modName](KiritoVM& vm) -> Handle {
        Handle scope = vm.newModuleScope();
        RootScope guard(vm);
        guard.add(scope);
        auto prog = std::make_unique<ast::Program>(Parser(Lexer(src).tokenize()).parseProgram());
        const ast::Program& program = *prog;
        vm.retainChunk(std::move(prog));
        Evaluator ev(vm, scope);
        ev.run(program);
        auto mod = std::make_unique<ModuleValue>(modName);
        for (const auto& [k, v] : static_cast<EnvValue&>(vm.arena().deref(scope)).locals())
            if (!k.empty() && k.front() != '_') mod->members[k] = v;  // hide private top-level names
        return vm.alloc(std::move(mod));
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
        try {
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
        } catch (KiritoError& e) {
            if (e.file.empty()) e.file = path.string();  // attribute the diagnostic to this module
            throw;
        }
    }
    throw KiritoError("no module named '" + name + "'");
}

// --- introspection (`inspect` builtin) -------------------------------------------------------

// Render a function's signature from its AST: name(p1: T1, p2 = default, ...) -> Ret. Annotations
// and defaults are shown when present; `Any`/none are simply omitted.
inline std::string inspectSignature(const std::string& name, const ast::FunctionExpr& def) {
    std::string out = name + "(";
    for (std::size_t i = 0; i < def.params.size(); ++i) {
        if (i) out += ", ";
        out += def.params[i].name;
        if (!def.params[i].annotation.empty()) out += ": " + def.params[i].annotation;
        if (def.params[i].defaultValue) out += " = ...";
    }
    out += ")";
    if (!def.returnAnnotation.empty()) out += " -> " + def.returnAnnotation;
    return out;
}

// Render a native function's declared signature: name(p: T, q = <default>, ...) -> Ret. Defaults are
// shown as their actual value (the VM is on hand to stringify them).
inline std::string inspectNativeSignature(KiritoVM& vm, const std::string& name, const NativeFunction& nf) {
    std::string out = name + "(";
    const auto& ps = nf.params();
    for (std::size_t i = 0; i < ps.size(); ++i) {
        if (i) out += ", ";
        out += ps[i].name;
        if (!ps[i].annotation.empty()) out += ": " + ps[i].annotation;
        if (ps[i].hasDefault) {
            const Object& d = vm.arena().deref(ps[i].defaultValue);
            // Quote String defaults so they read unambiguously (mode = "r", not mode = r).
            out += " = " + (d.kind() == ValueKind::String ? "\"" + vm.stringify(ps[i].defaultValue) + "\""
                                                          : vm.stringify(ps[i].defaultValue));
        }
    }
    out += ")";
    if (!nf.returnType().empty()) out += " -> " + nf.returnType();
    return out;
}

// Human-readable introspection of any value: lists public methods/attributes (with signatures and
// type annotations where declared) for classes, instances, modules, and functions. Returns a String.
inline std::string inspectValue(KiritoVM& vm, Handle h) {
    const Object& o = vm.arena().deref(h);
    auto sortedKeys = [](const std::unordered_map<std::string, Handle>& m) {
        std::vector<std::string> keys;
        for (const auto& [k, v] : m) keys.push_back(k);
        std::sort(keys.begin(), keys.end());
        return keys;
    };
    // Describe one member: a function shows its signature; anything else shows its type.
    auto describe = [&](const std::string& key, Handle mh, const char* indent) -> std::string {
        const Object& m = vm.arena().deref(mh);
        if (m.kind() == ValueKind::Function)
            return std::string(indent) + inspectSignature(key, static_cast<const KiFunction&>(m).def()) + "\n";
        if (m.kind() == ValueKind::NativeFunction) {
            const auto& nf = static_cast<const NativeFunction&>(m);
            if (nf.hasSignature())
                return std::string(indent) + inspectNativeSignature(vm, key, nf) + "  [native]\n";
            return std::string(indent) + key + "(...)  [native]\n";
        }
        return std::string(indent) + key + ": " + m.typeName() + "\n";
    };

    switch (o.kind()) {
        case ValueKind::Class: {
            const auto& c = static_cast<const ClassValue&>(o);
            std::string out = "class " + c.name;
            if (c.hasBase) out += "(" + vm.arena().deref(c.base).typeName() + ")";
            out += ":\n";
            // Walk the class + base chain, collecting the most-derived definition of each method.
            std::vector<std::pair<std::string, Handle>> chain;
            const ClassValue* cur = &c;
            std::unordered_map<std::string, Handle> seen;
            while (true) {
                for (const auto& [k, v] : cur->methods)
                    if (!isPrivateName(k) && seen.find(k) == seen.end()) seen[k] = v;
                if (!cur->hasBase) break;
                cur = &static_cast<const ClassValue&>(vm.arena().deref(cur->base));
            }
            if (seen.empty()) return out + "  (no public methods)";
            std::string methods;
            for (const std::string& k : sortedKeys(seen)) methods += describe(k, seen[k], "  ");
            return out + methods.substr(0, methods.size() - 1);  // drop trailing newline
        }
        case ValueKind::Instance: {
            const auto& inst = static_cast<const InstanceValue&>(o);
            std::string out = inst.className + " instance:\n";
            std::string attrs;
            for (const std::string& k : sortedKeys(inst.attrs))
                if (!isPrivateName(k)) attrs += describe(k, inst.attrs.at(k), "  attr ");
            // methods come from the class
            const auto& c = static_cast<const ClassValue&>(vm.arena().deref(inst.cls));
            std::unordered_map<std::string, Handle> seen;
            const ClassValue* cur = &c;
            while (true) {
                for (const auto& [k, v] : cur->methods)
                    if (!isPrivateName(k) && seen.find(k) == seen.end()) seen[k] = v;
                if (!cur->hasBase) break;
                cur = &static_cast<const ClassValue&>(vm.arena().deref(cur->base));
            }
            std::string methods;
            for (const std::string& k : sortedKeys(seen)) methods += describe(k, seen[k], "  ");
            std::string body = attrs + methods;
            if (body.empty()) return out + "  (no public members)";
            return out + body.substr(0, body.size() - 1);
        }
        case ValueKind::Module: {
            const auto& mod = static_cast<const ModuleValue&>(o);
            std::string out = "module " + mod.name() + ":\n";
            std::string body;
            for (const std::string& k : sortedKeys(mod.members)) body += describe(k, mod.members.at(k), "  ");
            if (body.empty()) return out + "  (empty)";
            return out + body.substr(0, body.size() - 1);
        }
        case ValueKind::Function:
            return inspectSignature("function", static_cast<const KiFunction&>(o).def());
        case ValueKind::NativeFunction: {
            const auto& nf = static_cast<const NativeFunction&>(o);
            return nf.hasSignature() ? inspectNativeSignature(vm, nf.name(), nf)
                                     : nf.name() + "(...)  [native]";
        }
        default:
            return o.typeName() + " value";
    }
}

// Python-style mini format-spec: [[fill]align][sign][#][0][width][,][.precision][type].
// Supports align <^>= , sign +/-/space, zero-pad, width, thousands ',', precision, and types
// b/o/x/X/d/f/e/g/s/% . Returns the formatted String. Raises on a malformed spec.
inline std::string applyFormatSpec(KiritoVM& vm, Handle value, const std::string& spec) {
    const Object& o = vm.arena().deref(value);
    std::size_t i = 0;
    char fill = ' ', align = 0, sign = '-';
    bool zero = false, comma = false;
    std::size_t width = 0;
    int precision = -1;
    char type = 0;
    // optional fill+align (fill only valid when an align char follows)
    auto isAlign = [](char c) { return c == '<' || c == '>' || c == '^' || c == '='; };
    if (i + 1 < spec.size() && isAlign(spec[i + 1])) { fill = spec[i]; align = spec[i + 1]; i += 2; }
    else if (i < spec.size() && isAlign(spec[i])) { align = spec[i]; ++i; }
    if (i < spec.size() && (spec[i] == '+' || spec[i] == '-' || spec[i] == ' ')) { sign = spec[i]; ++i; }
    if (i < spec.size() && spec[i] == '#') ++i;  // alternate form: accepted, no extra prefix added
    if (i < spec.size() && spec[i] == '0') { zero = true; if (!align) { align = '='; fill = '0'; } ++i; }
    while (i < spec.size() && spec[i] >= '0' && spec[i] <= '9') {
        width = width * 10 + static_cast<std::size_t>(spec[i] - '0');
        if (width > kMaxRepeat) throw KiritoError("format width too large");
        ++i;
    }
    if (i < spec.size() && spec[i] == ',') { comma = true; ++i; }
    if (i < spec.size() && spec[i] == '.') {
        ++i; precision = 0;
        while (i < spec.size() && spec[i] >= '0' && spec[i] <= '9') { precision = precision * 10 + (spec[i] - '0'); ++i; }
    }
    if (i < spec.size()) { type = spec[i]; ++i; }
    if (i != spec.size()) throw KiritoError("invalid format spec '" + spec + "'");

    auto groupThousands = [](std::string digits) {
        std::string out;
        int cnt = 0;
        for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
            if (cnt && cnt % 3 == 0) out += ',';
            out += *it; ++cnt;
        }
        std::reverse(out.begin(), out.end());
        return out;
    };

    std::string body, signStr;
    bool numeric = (o.kind() == ValueKind::Integer || o.kind() == ValueKind::Float || o.kind() == ValueKind::Bool);

    if (type == 's' || (type == 0 && !numeric)) {
        body = vm.stringify(value);
        if (precision >= 0 && static_cast<std::size_t>(precision) < utf8Length(body)) {
            auto starts = utf8Starts(body);
            body = body.substr(0, starts[precision]);
        }
        if (!zero && align == 0) align = '<';  // strings default to left-align
    } else if (type == 'b' || type == 'o' || type == 'x' || type == 'X' || type == 'd' || type == 0) {
        if (o.kind() != ValueKind::Integer && o.kind() != ValueKind::Bool)
            throw KiritoError("format type '" + std::string(1, type ? type : 'd') + "' needs an Integer");
        int64_t v = o.kind() == ValueKind::Bool ? (static_cast<const BoolVal&>(o).value() ? 1 : 0)
                                                : static_cast<const IntVal&>(o).value();
        bool neg = v < 0;
        uint64_t u = neg ? 0ull - static_cast<uint64_t>(v) : static_cast<uint64_t>(v);
        int base = type == 'b' ? 2 : type == 'o' ? 8 : (type == 'x' || type == 'X') ? 16 : 10;
        const char* alpha = type == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
        std::string digits;
        if (u == 0) digits = "0";
        while (u) { digits += alpha[u % base]; u /= base; }
        std::reverse(digits.begin(), digits.end());
        if (comma && base == 10) digits = groupThousands(digits);
        signStr = neg ? "-" : (sign == '+' ? "+" : sign == ' ' ? " " : "");
        body = digits;
    } else if (type == 'f' || type == 'e' || type == 'g' || type == '%') {
        double d = o.kind() == ValueKind::Float ? static_cast<const FloatVal&>(o).value()
                 : o.kind() == ValueKind::Integer ? static_cast<double>(static_cast<const IntVal&>(o).value())
                 : (static_cast<const BoolVal&>(o).value() ? 1.0 : 0.0);
        if (type == '%') d *= 100.0;
        bool neg = std::signbit(d);
        char fmtbuf[16];
        char conv = type == '%' ? 'f' : type;
        std::snprintf(fmtbuf, sizeof(fmtbuf), "%%.%d%c", precision < 0 ? 6 : precision, conv);
        char out[64];
        // fmtbuf is built above from a fixed pattern + validated conv char; the dynamic format is intentional.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
        std::snprintf(out, sizeof(out), fmtbuf, std::fabs(d));
#pragma GCC diagnostic pop
        body = out;
        if (type == '%') body += "%";
        if (comma) {
            std::size_t dot = body.find('.');
            std::string intpart = body.substr(0, dot);
            body = groupThousands(intpart) + (dot == std::string::npos ? "" : body.substr(dot));
        }
        signStr = neg ? "-" : (sign == '+' ? "+" : sign == ' ' ? " " : "");
    } else {
        throw KiritoError("unknown format type '" + std::string(1, type) + "'");
    }

    if (width > kMaxRepeat) throw KiritoError("format width too large");
    std::string s = signStr + body;
    if (s.size() >= width) return s;
    std::size_t pad = width - utf8Length(s);
    if (align == '<') return s + std::string(pad, fill);
    if (align == '>') return std::string(pad, fill) + s;
    if (align == '^') return std::string(pad / 2, fill) + s + std::string(pad - pad / 2, fill);
    if (align == '=') return signStr + std::string(pad, fill) + body;  // pad between sign and digits
    return std::string(pad, fill) + s;  // numbers default to right-align
}

// --- built-in globals ------------------------------------------------------------------------

inline void KiritoVM::installBuiltins() {
    auto& g = static_cast<EnvValue&>(arena_.deref(global_));
    auto def = [&](const char* name, NativeFn fn) {
        g.define(name, alloc(std::make_unique<NativeFunction>(name, std::move(fn))));
    };
    // Like def, but with a declared signature so the builtin accepts keyword arguments and defaults
    // and describes itself under `inspect`.
    auto defSig = [&](const char* name, std::vector<NativeParam> sig, std::string ret, NativeFn fn) {
        g.define(name, alloc(std::make_unique<NativeFunction>(name, std::move(sig), std::move(ret), std::move(fn))));
    };

    defSig("len", {{"x"}}, "Integer", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
        if (args.size() != 1) throw KiritoError("len expected 1 argument");
        return vm.makeInt(vm.arena().deref(args[0]).length(vm).value());
    });

    defSig("import", {{"name", "String"}}, "Module", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
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
            case ValueKind::Float: {
                double d = static_cast<const FloatVal&>(o).value();
                // Casting a non-finite or out-of-range double to int64 is UB; reject it cleanly.
                if (std::isnan(d)) throw KiritoError("cannot convert Float NaN to Integer");
                if (std::isinf(d)) throw KiritoError("cannot convert Float infinity to Integer");
                if (d >= 9223372036854775808.0 || d < -9223372036854775808.0)
                    throw KiritoError("Float is out of Integer range");
                return vm.makeInt(static_cast<int64_t>(d));
            }
            case ValueKind::String: {
                const std::string& s = static_cast<const StrVal&>(o).value();
                try {
                    std::size_t pos = 0;
                    int64_t v = static_cast<int64_t>(std::stoll(s, &pos));
                    // Reject trailing garbage (e.g. "42abc", "0x1F", "12.5") — std::stoll would
                    // silently parse only a prefix. Surrounding whitespace is allowed.
                    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
                    if (pos != s.size()) throw std::invalid_argument("trailing");
                    return vm.makeInt(v);
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
                    std::size_t pos = 0;
                    double v = std::stod(s, &pos);
                    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
                    if (pos != s.size()) throw std::invalid_argument("trailing");
                    return vm.makeFloat(v);
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

    // Collection constructors: List()/Set()/Dict() build an empty collection; List(iterable) and
    // Set(iterable) build from any iterable. (Literals [] {} {,} remain the idiomatic shorthand.)
    def("List", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
        if (args.size() > 1) throw KiritoError("List expected at most 1 argument");
        RootScope rs(vm);
        auto list = std::make_unique<ListVal>();
        if (args.size() == 1) {
            auto items = vm.arena().deref(args[0]).iterate(vm);
            if (!items) throw KiritoError("List() argument must be iterable");
            for (Handle h : items.value()) rs.add(h);
            list->elems = std::move(items.value());
        }
        return vm.alloc(std::move(list));
    });
    def("Set", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
        if (args.size() > 1) throw KiritoError("Set expected at most 1 argument");
        RootScope rs(vm);
        Handle sh = rs.add(vm.alloc(std::make_unique<SetVal>()));
        auto& s = static_cast<SetVal&>(vm.arena().deref(sh));
        if (args.size() == 1) {
            auto items = vm.arena().deref(args[0]).iterate(vm);
            if (!items) throw KiritoError("Set() argument must be iterable");
            for (Handle h : items.value()) s.add(vm.arena(), h);
        }
        return sh;
    });
    def("Dict", [](KiritoVM& vm, std::span<const Handle> args) -> Handle {
        if (args.size() > 1) throw KiritoError("Dict expected at most 1 argument");
        RootScope rs(vm);
        Handle dh = rs.add(vm.alloc(std::make_unique<DictVal>()));
        auto& d = static_cast<DictVal&>(vm.arena().deref(dh));
        if (args.size() == 1) {
            // Dict(pairs): each item is an iterable [key, value].
            auto items = vm.arena().deref(args[0]).iterate(vm);
            if (!items) throw KiritoError("Dict() argument must be iterable of pairs");
            for (Handle h : items.value()) {
                auto pair = vm.arena().deref(h).iterate(vm);
                if (!pair || pair.value().size() != 2)
                    throw KiritoError("Dict() items must be [key, value] pairs");
                d.set(vm.arena(), pair.value()[0], pair.value()[1]);
            }
        }
        return dh;
    });

    defSig("abs", {{"x", "Number"}}, "Number", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        const Object& o = vm.arena().deref(a[0]);
        if (o.kind() == ValueKind::Integer) {
            int64_t v = static_cast<const IntVal&>(o).value();
            // std::llabs(INT64_MIN) is UB; negate via unsigned for defined two's-complement
            // wraparound (abs(INT64_MIN) wraps to itself, consistent with Kirito's int64 semantics).
            return vm.makeInt(v < 0 ? wsub(0, v) : v);
        }
        if (o.kind() == ValueKind::Float) return vm.makeFloat(std::fabs(static_cast<const FloatVal&>(o).value()));
        throw KiritoError("abs expects a number");
    });
    defSig("round", {{"x", "Number"}, {"ndigits", "", none()}}, "", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        if (a.empty()) throw KiritoError("round expected at least 1 argument");
        const Object& xo = vm.arena().deref(a[0]);
        if (!isNumeric(xo)) throw KiritoError("round expects a number");
        double x = asDouble(xo);
        // ndigits given (and not the None default) -> round to that many decimals, yielding a Float;
        // otherwise round to the nearest Integer (Python semantics).
        if (a.size() >= 2 && vm.arena().deref(a[1]).kind() != ValueKind::None) {
            const Object& no = vm.arena().deref(a[1]);
            if (no.kind() != ValueKind::Integer) throw KiritoError("round ndigits must be an Integer");
            int64_t nd = static_cast<const IntVal&>(no).value();
            double f = std::pow(10.0, static_cast<double>(nd));
            return vm.makeFloat(std::round(x * f) / f);
        }
        if (std::isnan(x)) throw KiritoError("cannot round NaN to Integer");
        if (std::isinf(x)) throw KiritoError("cannot round infinity to Integer");
        if (x >= 9223372036854775808.0 || x < -9223372036854775808.0)
            throw KiritoError("rounded value out of Integer range");
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
        // range materializes a List (no lazy generators yet), so reject a count that would exhaust
        // memory up front rather than OOM mid-build.
        uint64_t count = 0;
        if (step > 0 && stop > start)
            count = (static_cast<uint64_t>(stop - start) + static_cast<uint64_t>(step) - 1) / static_cast<uint64_t>(step);
        else if (step < 0 && stop < start)
            count = (static_cast<uint64_t>(start - stop) + static_cast<uint64_t>(-step) - 1) / static_cast<uint64_t>(-step);
        if (count > kMaxRepeat) throw KiritoError("range too large");
        RootScope rs(vm);
        auto list = std::make_unique<ListVal>();
        list->elems.reserve(static_cast<std::size_t>(count));
        if (step > 0) for (int64_t i = start; i < stop; i += step) list->elems.push_back(rs.add(vm.makeInt(i)));
        else for (int64_t i = start; i > stop; i += step) list->elems.push_back(rs.add(vm.makeInt(i)));
        return vm.alloc(std::move(list));
    });
    defSig("sum", {{"iterable"}}, "Number", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
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
    defSig("sorted", {{"iterable"}, {"key", "", none()}, {"reverse", "Bool", makeBool(false)}}, "List",
           [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        // sorted(iterable[, key][, reverse]) -> a new STABLE-sorted List.
        RootScope rs(vm);
        Handle keyFn{};
        bool hasKey = false, reverse = false;
        if (a.size() > 1 && vm.arena().deref(a[1]).kind() != ValueKind::None) { keyFn = a[1]; hasKey = true; }
        if (a.size() > 2) reverse = vm.arena().deref(a[2]).truthy();
        auto items = vm.arena().deref(a[0]).iterate(vm);
        std::vector<std::pair<Handle, Handle>> tagged;
        for (Handle h : items.value()) {
            rs.add(h);
            Handle k = h;
            if (hasKey) { std::array<Handle, 1> args{h}; k = rs.add(vm.arena().deref(keyFn).call(vm, args)); }
            tagged.emplace_back(k, h);
        }
        std::stable_sort(tagged.begin(), tagged.end(),
                         [&](const std::pair<Handle, Handle>& x, const std::pair<Handle, Handle>& y) {
                             return reverse ? kiLessThan(vm, y.first, x.first) : kiLessThan(vm, x.first, y.first);
                         });
        auto list = std::make_unique<ListVal>();
        for (auto& p : tagged) list->elems.push_back(p.second);
        return vm.alloc(std::move(list));
    });
    defSig("enumerate", {{"iterable"}}, "List", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
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
    defSig("map", {{"function"}, {"iterable"}}, "List", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
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
    defSig("filter", {{"function"}, {"iterable"}}, "List", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
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
    defSig("type", {{"x"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        return vm.makeString(vm.arena().deref(a[0]).typeName());
    });
    defSig("inspect", {{"x"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        if (a.size() != 1) throw KiritoError("inspect expected 1 argument");
        return vm.makeString(inspectValue(vm, a[0]));
    });
    defSig("format", {{"value"}, {"spec", "String", makeString("")}}, "String",
           [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        // format(value[, spec]) -> String, applying a Python mini-format-spec. No spec == String().
        if (a.size() < 1 || a.size() > 2) throw KiritoError("format expected 1 or 2 arguments");
        std::string spec;
        if (a.size() == 2) {
            const Object& s = vm.arena().deref(a[1]);
            if (s.kind() != ValueKind::String) throw KiritoError("format spec must be a String");
            spec = static_cast<const StrVal&>(s).value();
        }
        return vm.makeString(applyFormatSpec(vm, a[0], spec));
    });

    defSig("all", {{"iterable"}}, "Bool", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        if (a.size() != 1) throw KiritoError("all expected 1 argument");
        auto items = vm.arena().deref(a[0]).iterate(vm);
        if (!items) throw KiritoError("all expects an iterable");
        for (Handle h : items.value())
            if (!vm.arena().deref(h).truthy()) return vm.makeBool(false);
        return vm.makeBool(true);
    });
    defSig("any", {{"iterable"}}, "Bool", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        if (a.size() != 1) throw KiritoError("any expected 1 argument");
        auto items = vm.arena().deref(a[0]).iterate(vm);
        if (!items) throw KiritoError("any expects an iterable");
        for (Handle h : items.value())
            if (vm.arena().deref(h).truthy()) return vm.makeBool(true);
        return vm.makeBool(false);
    });
    defSig("reversed", {{"iterable"}}, "List", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        if (a.size() != 1) throw KiritoError("reversed expected 1 argument");
        auto items = vm.arena().deref(a[0]).iterate(vm);
        if (!items) throw KiritoError("reversed expects an iterable");
        RootScope rs(vm);
        for (Handle h : items.value()) rs.add(h);
        auto out = std::make_unique<ListVal>();
        out->elems.assign(items.value().rbegin(), items.value().rend());
        return vm.alloc(std::move(out));
    });
    defSig("divmod", {{"a"}, {"b"}}, "List", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        if (a.size() != 2) throw KiritoError("divmod expected 2 arguments");
        RootScope rs(vm);
        Handle q = rs.add(numericBinary(vm, BinOp::FloorDiv, a[0], a[1]));
        Handle r = rs.add(numericBinary(vm, BinOp::Mod, a[0], a[1]));
        auto pair = std::make_unique<ListVal>();
        pair->elems.push_back(q);
        pair->elems.push_back(r);
        return vm.alloc(std::move(pair));
    });
    defSig("isinstance", {{"value"}, {"type"}}, "Bool", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        if (a.size() != 2) throw KiritoError("isinstance expected 2 arguments");
        // Second arg is a type: either a class value or a String type-name. Either resolves to a
        // name we match through typeMatches (kind names + inheritance-aware class chain).
        const Object& t = vm.arena().deref(a[1]);
        std::string typeName;
        if (t.kind() == ValueKind::String) typeName = static_cast<const StrVal&>(t).value();
        else if (t.kind() == ValueKind::Class) typeName = static_cast<const ClassValue&>(t).name;
        else throw KiritoError("isinstance second argument must be a class or a type-name String");
        return vm.makeBool(typeMatches(vm, a[0], typeName));
    });
    defSig("ord", {{"char", "String"}}, "Integer", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        if (a.size() != 1) throw KiritoError("ord expected 1 argument");
        const Object& o = vm.arena().deref(a[0]);
        if (o.kind() != ValueKind::String) throw KiritoError("ord expects a String");
        const std::string& s = static_cast<const StrVal&>(o).value();
        if (utf8Length(s) != 1) throw KiritoError("ord expects a single character");
        return vm.makeInt(static_cast<int64_t>(utf8DecodeAt(s, 0)));
    });
    defSig("chr", {{"codepoint", "Integer"}}, "String", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        if (a.size() != 1) throw KiritoError("chr expected 1 argument");
        const Object& o = vm.arena().deref(a[0]);
        if (o.kind() != ValueKind::Integer) throw KiritoError("chr expects an Integer");
        int64_t cp = static_cast<const IntVal&>(o).value();
        if (cp < 0 || cp > 0x10FFFF) throw KiritoError("chr argument out of Unicode range");
        std::string s;
        utf8Encode(static_cast<unsigned>(cp), s);
        return vm.makeString(s);
    });
    auto radix = [](KiritoVM& vm, std::span<const Handle> a, int base, const char* prefix,
                    const char* name) -> Handle {
        if (a.size() != 1) throw KiritoError(std::string(name) + " expected 1 argument");
        const Object& o = vm.arena().deref(a[0]);
        if (o.kind() != ValueKind::Integer) throw KiritoError(std::string(name) + " expects an Integer");
        int64_t v = static_cast<const IntVal&>(o).value();
        bool neg = v < 0;
        uint64_t u = neg ? 0ull - static_cast<uint64_t>(v) : static_cast<uint64_t>(v);
        std::string digits;
        const char* alpha = "0123456789abcdef";
        if (u == 0) digits = "0";
        while (u) { digits += alpha[u % base]; u /= base; }
        std::reverse(digits.begin(), digits.end());
        return vm.makeString((neg ? "-" : "") + std::string(prefix) + digits);
    };
    defSig("bin", {{"n", "Integer"}}, "String", [radix](KiritoVM& vm, std::span<const Handle> a) { return radix(vm, a, 2, "0b", "bin"); });
    defSig("oct", {{"n", "Integer"}}, "String", [radix](KiritoVM& vm, std::span<const Handle> a) { return radix(vm, a, 8, "0o", "oct"); });
    defSig("hex", {{"n", "Integer"}}, "String", [radix](KiritoVM& vm, std::span<const Handle> a) { return radix(vm, a, 16, "0x", "hex"); });
    defSig("pow", {{"base"}, {"exp"}, {"mod", "", none()}}, "", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        // pow(base, exp) -> base**exp; pow(base, exp, mod) -> modular exponentiation. A None mod
        // (the default) means the plain two-argument form.
        if (a.size() < 3 || vm.arena().deref(a[2]).kind() == ValueKind::None)
            return numericBinary(vm, BinOp::Pow, a[0], a[1]);
        {
            // pow(base, exp, mod): modular exponentiation over non-negative Integers.
            auto geti = [&](Handle h, const char* w) {
                const Object& o = vm.arena().deref(h);
                if (o.kind() != ValueKind::Integer) throw KiritoError(std::string("pow ") + w + " must be Integer with 3 args");
                return static_cast<const IntVal&>(o).value();
            };
            int64_t base = geti(a[0], "base"), exp = geti(a[1], "exp"), mod = geti(a[2], "mod");
            if (exp < 0) throw KiritoError("pow exponent must be non-negative with a modulus");
            if (mod == 0) throw KiritoError("pow modulus must be non-zero");
            __extension__ __int128 result = 1 % mod, b = ((base % mod) + mod) % mod;
            while (exp > 0) {
                if (exp & 1) result = (result * b) % mod;
                b = (b * b) % mod;
                exp >>= 1;
            }
            return vm.makeInt(static_cast<int64_t>(result));
        }
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
    install<TimeModule>();
    install<DumpModule>();
    install<ZlibModule>();
    install<HashModule>();

    // Modules authored in Kirito and frozen into the binary (see stdlib_kimodules.hpp).
    registerSourceModule("itertools", kimod::itertools);
    registerSourceModule("functools", kimod::functools);
    registerSourceModule("collections", kimod::collections);
    registerSourceModule("statistics", kimod::statistics);
    registerSourceModule("string", kimod::string_mod);
    registerSourceModule("textwrap", kimod::textwrap);
    registerSourceModule("base64", kimod::base64);
    registerSourceModule("csv", kimod::csv);
    registerSourceModule("heapq", kimod::heapq);
    registerSourceModule("bisect", kimod::bisect);
    registerSourceModule("copy", kimod::copy_mod);
    registerSourceModule("enum", kimod::enum_mod);
}

inline void KiritoVM::retainChunk(std::unique_ptr<ast::Program> chunk) {
    chunks_.push_back(std::move(chunk));
}

inline KiritoVM::~KiritoVM() = default;

inline Handle KiritoVM::evalIn(std::string_view source, Handle scope, std::string_view chunkName) {
    try {
        Lexer lexer(source);
        Parser parser(lexer.tokenize());
        auto prog = std::make_unique<ast::Program>(parser.parseProgram());
        const ast::Program& program = *prog;
        retainChunk(std::move(prog));  // keep the AST alive for the VM's lifetime (closures)
        Evaluator ev(*this, scope);
        try {
            return ev.run(program);
        } catch (const KiritoThrow& t) {
            throw KiritoError("uncaught exception: " + stringify(t.value), t.span);
        }
    } catch (KiritoError& e) {
        if (e.file.empty() && !chunkName.empty()) e.file = std::string(chunkName);
        throw;
    }
}

inline Handle KiritoVM::runSource(std::string_view source, std::string_view chunkName) {
    return evalIn(source, newModuleScope(), chunkName);
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
