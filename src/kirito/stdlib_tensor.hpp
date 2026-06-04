#ifndef KIRITO_STDLIB_TENSOR_HPP
#define KIRITO_STDLIB_TENSOR_HPP

#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"
#include "stdlib_complex.hpp"  // ComplexVal, cdouble, complexToString, cpx::asComplex
#include "tensor.hpp"

namespace kirito {

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#endif

// An N-dimensional tensor exposed to Kirito. The element type (dtype) is chosen at runtime: Float
// (double, the default) or Complex (std::complex<double>). The numeric work is the shared
// `kirito::tensor` engine; this class is the Kirito-facing wrapper + dispatch. (The engine is
// generic in T, so a third party can instantiate it for other element types in C++.)
class TensorVal : public NativeClass<TensorVal> {
public:
    static constexpr const char* kTypeName = "Tensor";
    using FT = tensor::Tensor<double>;
    using CT = tensor::Tensor<cdouble>;
    std::variant<FT, CT> store;  // index 0 = Float, 1 = Complex

    TensorVal() = default;
    explicit TensorVal(FT t) : store(std::move(t)) {}
    explicit TensorVal(CT t) : store(std::move(t)) {}

    bool isComplex() const { return store.index() == 1; }
    const char* dtypeName() const { return isComplex() ? "Complex" : "Float"; }
    const tensor::Shape& shape() const { return isComplex() ? std::get<CT>(store).shape : std::get<FT>(store).shape; }
    std::size_t ndim() const { return shape().size(); }
    std::size_t size() const { return isComplex() ? std::get<CT>(store).size() : std::get<FT>(store).size(); }
    // every element promoted to a complex value (for cross-dtype comparison / promotion)
    cdouble elemAsComplex(std::size_t i) const {
        return isComplex() ? std::get<CT>(store).data[i] : cdouble(std::get<FT>(store).data[i], 0.0);
    }

    bool truthy() const override { return size() != 0; }

    std::string str(StringifyCtx&) const override {
        if (isComplex()) return format(std::get<CT>(store), [](cdouble z) { return complexToString(z); });
        return format(std::get<FT>(store), [](double x) { return floatToString(x); });
    }
    bool equals(const ObjectArena&, const Object& other) const override {
        const auto* o = dynamic_cast<const TensorVal*>(&other);
        if (!o || o->shape() != shape()) return false;
        for (std::size_t i = 0; i < size(); ++i) {
            cdouble a = elemAsComplex(i), b = o->elemAsComplex(i);
            if (std::abs(a - b) > 1e-9) return false;
        }
        return true;
    }
    std::vector<std::string> inspectMembers() const override {
        return {"shape() -> List", "ndim() -> Integer", "size() -> Integer", "dtype() -> String",
                "reshape(shape) -> Tensor", "transpose() -> Tensor", "permute(axes) -> Tensor",
                "flatten() -> Tensor", "apply(fn) -> Tensor", "astype(dtype) -> Tensor",
                "matmul(other) -> Tensor", "dot(other) -> Number",
                "sum(axis = None) -> Tensor", "mean(axis = None) -> Tensor",
                "prod(axis = None) -> Tensor", "min() -> Float", "max() -> Float"};
    }

    Handle binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) override;
    Handle unary(KiritoVM& vm, UnOp op, Handle self) override;
    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override;
    Handle getItem(KiritoVM& vm, std::span<const Handle> keys) override;
    void setItem(KiritoVM& vm, std::span<const Handle> keys, Handle value) override;

    // N-D nested-bracket formatting, e.g. [[1.0, 2.0], [3.0, 4.0]].
    template <class T, class Fmt>
    static std::string format(const tensor::Tensor<T>& t, Fmt fmt) {
        if (t.ndim() == 0) return fmt(t.data.empty() ? T{} : t.data[0]);
        tensor::Shape st = t.strides();
        std::function<std::string(std::size_t, std::size_t)> rec = [&](std::size_t dim, std::size_t off) {
            std::string s = "[";
            for (std::size_t i = 0; i < t.shape[dim]; ++i) {
                if (i) s += ", ";
                if (dim + 1 == t.ndim()) s += fmt(t.data[off + i * st[dim]]);
                else s += rec(dim + 1, off + i * st[dim]);
            }
            return s + "]";
        };
        return rec(0, 0);
    }
};

namespace tns {

inline constexpr std::size_t kMaxElems = 64ull * 1024 * 1024;
inline void checkSize(const tensor::Shape& s) {
    std::size_t n = 1;
    for (std::size_t d : s) {
        if (d != 0 && n > kMaxElems / d) throw KiritoError("Tensor too large");
        n *= d;
    }
}

// Promote a Float tensor to Complex (reals on the real axis).
inline TensorVal::CT toComplex(const TensorVal::FT& t) {
    TensorVal::CT out(t.shape);
    for (std::size_t i = 0; i < t.data.size(); ++i) out.data[i] = cdouble(t.data[i], 0.0);
    return out;
}

inline Handle make(KiritoVM& vm, TensorVal::FT t) { checkSize(t.shape); return vm.alloc(std::make_unique<TensorVal>(std::move(t))); }
inline Handle make(KiritoVM& vm, TensorVal::CT t) { checkSize(t.shape); return vm.alloc(std::make_unique<TensorVal>(std::move(t))); }

// Wrap an engine call so its TensorError surfaces as a KiritoError.
template <class F>
auto wrap(F&& f) -> decltype(f()) {
    try { return f(); } catch (const tensor::TensorError& e) { throw KiritoError(e.what()); }
}

// Read a shape argument (a List of non-negative Integers).
inline tensor::Shape readShape(Value v) {
    tensor::Shape s;
    for (Value e : v.items()) {
        int64_t d = e.asInt("shape dimension");
        if (d < 0) throw KiritoError("Tensor shape dimensions must be non-negative");
        s.push_back(static_cast<std::size_t>(d));
    }
    return s;
}

// dtype string -> wants-complex flag (default Float).
inline bool wantsComplex(KiritoVM& vm, Handle h) {
    const Object& o = vm.arena().deref(h);
    if (o.kind() == ValueKind::None) return false;
    std::string d = Value(vm, h).asString("dtype");
    if (d == "Float") return false;
    if (d == "Complex") return true;
    throw KiritoError("Tensor dtype must be \"Float\" or \"Complex\"");
}

}  // namespace tns

inline Handle TensorVal::binary(KiritoVM& vm, BinOp op, Handle, Handle rhs) {
    char c = op == BinOp::Add ? '+' : op == BinOp::Sub ? '-' : op == BinOp::Mul ? '*' : op == BinOp::Div ? '/' : 0;
    if (!c) throw KiritoError("Tensor does not support this operator (use .matmul for matrix products)");
    const Object& b = vm.arena().deref(rhs);
    const auto* ot = dynamic_cast<const TensorVal*>(&b);
    return tns::wrap([&]() -> Handle {
        auto applyF = [&](const TensorVal::FT& x, const TensorVal::FT& y) {
            switch (c) {
                case '+': return tensor::add(x, y);
                case '-': return tensor::sub(x, y);
                case '*': return tensor::mul(x, y);
                default: return tensor::div(x, y);
            }
        };
        auto applyC = [&](const TensorVal::CT& x, const TensorVal::CT& y) {
            switch (c) {
                case '+': return tensor::add(x, y);
                case '-': return tensor::sub(x, y);
                case '*': return tensor::mul(x, y);
                default: return tensor::div(x, y);
            }
        };
        if (ot) {  // tensor OP tensor (promote to complex if either side is complex)
            if (!isComplex() && !ot->isComplex()) return tns::make(vm, applyF(std::get<FT>(store), std::get<FT>(ot->store)));
            CT a = isComplex() ? std::get<CT>(store) : tns::toComplex(std::get<FT>(store));
            CT d = ot->isComplex() ? std::get<CT>(ot->store) : tns::toComplex(std::get<FT>(ot->store));
            return tns::make(vm, applyC(a, d));
        }
        // tensor OP scalar (Integer / Float / Complex)
        bool scalarComplex = dynamic_cast<const ComplexVal*>(&b) != nullptr;
        if (!isComplex() && !scalarComplex) {
            return tns::make(vm, tensor::scalarOp(std::get<FT>(store), Value(vm, rhs).asFloat("scalar"), c));
        }
        CT a = isComplex() ? std::get<CT>(store) : tns::toComplex(std::get<FT>(store));
        return tns::make(vm, tensor::scalarOp(a, cpx::asComplex(vm, rhs, "scalar"), c));
    });
}

inline Handle TensorVal::unary(KiritoVM& vm, UnOp op, Handle) {
    if (op != UnOp::Neg) throw KiritoError("Tensor does not support this unary operator");
    if (isComplex()) return tns::make(vm, tensor::negate(std::get<CT>(store)));
    return tns::make(vm, tensor::negate(std::get<FT>(store)));
}

inline Handle TensorVal::getItem(KiritoVM& vm, std::span<const Handle> keys) {
    tensor::Shape idx;
    for (Handle k : keys) {
        const Object& o = vm.arena().deref(k);
        if (o.kind() != ValueKind::Integer) throw KiritoError("Tensor index must be Integer");
        int64_t v = static_cast<const IntVal&>(o).value();
        if (v < 0) throw KiritoError("Tensor index out of range");
        idx.push_back(static_cast<std::size_t>(v));
    }
    if (idx.size() > ndim()) throw KiritoError("too many indices for tensor");
    const tensor::Shape& sh = shape();
    tensor::Shape st = tensor::rowMajorStrides(sh);
    std::size_t off = 0;
    for (std::size_t i = 0; i < idx.size(); ++i) {
        if (idx[i] >= sh[i]) throw KiritoError("Tensor index out of range");
        off += idx[i] * st[i];
    }
    if (idx.size() == ndim()) {  // a single element -> a scalar value
        if (isComplex()) return cpx::make(vm, std::get<CT>(store).data[off]);
        return vm.makeFloat(std::get<FT>(store).data[off]);
    }
    // a partial index -> a sub-tensor (the contiguous block of the remaining axes)
    tensor::Shape sub(sh.begin() + static_cast<std::ptrdiff_t>(idx.size()), sh.end());
    std::size_t subN = tensor::numel(sub);
    if (isComplex()) {
        const auto& d = std::get<CT>(store).data;
        return tns::make(vm, TensorVal::CT(sub, std::vector<cdouble>(d.begin() + static_cast<std::ptrdiff_t>(off), d.begin() + static_cast<std::ptrdiff_t>(off + subN))));
    }
    const auto& d = std::get<FT>(store).data;
    return tns::make(vm, TensorVal::FT(sub, std::vector<double>(d.begin() + static_cast<std::ptrdiff_t>(off), d.begin() + static_cast<std::ptrdiff_t>(off + subN))));
}

inline void TensorVal::setItem(KiritoVM& vm, std::span<const Handle> keys, Handle value) {
    if (keys.size() != ndim()) throw KiritoError("Tensor element assignment needs a full index (one per dimension)");
    tensor::Shape idx;
    for (Handle k : keys) {
        const Object& o = vm.arena().deref(k);
        if (o.kind() != ValueKind::Integer) throw KiritoError("Tensor index must be Integer");
        int64_t v = static_cast<const IntVal&>(o).value();
        if (v < 0) throw KiritoError("Tensor index out of range");
        idx.push_back(static_cast<std::size_t>(v));
    }
    if (isComplex()) std::get<CT>(store).at(idx) = cpx::asComplex(vm, value, "Tensor element");
    else std::get<FT>(store).at(idx) = Value(vm, value).asFloat("Tensor element");
}

inline Handle TensorVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    auto self_t = [](KiritoVM& vm, Handle self) -> TensorVal& { return static_cast<TensorVal&>(vm.arena().deref(self)); };
    auto bind = [&](const char* nm, std::vector<std::string> params, NativeFn fn) {
        return makeMethod(vm, nm, std::move(params), std::move(fn), std::vector<Handle>{self});
    };
    auto axisOf = [](KiritoVM& vm, std::span<const Handle> a) -> int64_t {  // -1 = whole tensor
        if (a.empty() || vm.arena().deref(a[0]).kind() == ValueKind::None) return -1;
        return Value(vm, a[0]).asInt("axis");
    };
    if (name == "shape") return bind("shape", {}, [self, self_t](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto list = std::make_unique<ListVal>();
        for (std::size_t d : self_t(vm, self).shape()) list->elems.push_back(vm.makeInt(static_cast<int64_t>(d)));
        return vm.alloc(std::move(list));
    });
    if (name == "ndim") return bind("ndim", {}, [self, self_t](KiritoVM& vm, std::span<const Handle>) { return vm.makeInt(static_cast<int64_t>(self_t(vm, self).ndim())); });
    if (name == "size") return bind("size", {}, [self, self_t](KiritoVM& vm, std::span<const Handle>) { return vm.makeInt(static_cast<int64_t>(self_t(vm, self).size())); });
    if (name == "dtype") return bind("dtype", {}, [self, self_t](KiritoVM& vm, std::span<const Handle>) { return vm.makeString(self_t(vm, self).dtypeName()); });
    if (name == "reshape") return bind("reshape", {"shape"}, [self, self_t](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& t = self_t(vm, self);
        tensor::Shape ns = tns::readShape(Value(vm, a[0]));
        return tns::wrap([&]() -> Handle {
            if (t.isComplex()) return tns::make(vm, tensor::reshape(std::get<CT>(t.store), ns));
            return tns::make(vm, tensor::reshape(std::get<FT>(t.store), ns));
        });
    });
    if (name == "transpose") return bind("transpose", {}, [self, self_t](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto& t = self_t(vm, self);
        if (t.isComplex()) return tns::make(vm, tensor::transpose(std::get<CT>(t.store)));
        return tns::make(vm, tensor::transpose(std::get<FT>(t.store)));
    });
    if (name == "permute") return bind("permute", {"axes"}, [self, self_t](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& t = self_t(vm, self);
        tensor::Shape ax = tns::readShape(Value(vm, a[0]));
        return tns::wrap([&]() -> Handle {
            if (t.isComplex()) return tns::make(vm, tensor::permute(std::get<CT>(t.store), ax));
            return tns::make(vm, tensor::permute(std::get<FT>(t.store), ax));
        });
    });
    if (name == "flatten") return bind("flatten", {}, [self, self_t](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto& t = self_t(vm, self);
        if (t.isComplex()) return tns::make(vm, tensor::flatten(std::get<CT>(t.store)));
        return tns::make(vm, tensor::flatten(std::get<FT>(t.store)));
    });
    if (name == "apply") return bind("apply", {"fn"}, [self, self_t](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        Handle fn = a[0];
        auto& t = self_t(vm, self);
        if (t.isComplex()) {
            CT out = std::get<CT>(t.store);
            for (std::size_t i = 0; i < out.data.size(); ++i) {
                RootScope rs(vm);
                std::array<Handle, 1> args{rs.add(cpx::make(vm, out.data[i]))};
                out.data[i] = cpx::asComplex(vm, vm.arena().deref(fn).call(vm, args), "apply result");
            }
            return tns::make(vm, std::move(out));
        }
        FT out = std::get<FT>(t.store);
        for (std::size_t i = 0; i < out.data.size(); ++i) {
            std::array<Handle, 1> args{vm.makeFloat(out.data[i])};
            out.data[i] = Value(vm, vm.arena().deref(fn).call(vm, args)).asFloat("apply result");
        }
        return tns::make(vm, std::move(out));
    });
    if (name == "astype") return bind("astype", {"dtype"}, [self, self_t](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& t = self_t(vm, self);
        bool toComplex = tns::wantsComplex(vm, a[0]);
        if (toComplex) {
            if (t.isComplex()) return tns::make(vm, std::get<CT>(t.store));
            return tns::make(vm, tns::toComplex(std::get<FT>(t.store)));
        }
        if (!t.isComplex()) return tns::make(vm, std::get<FT>(t.store));
        const CT& c = std::get<CT>(t.store);  // Complex -> Float keeps the real part
        FT out(c.shape);
        for (std::size_t i = 0; i < c.data.size(); ++i) out.data[i] = c.data[i].real();
        return tns::make(vm, std::move(out));
    });
    if (name == "matmul") return bind("matmul", {"other"}, [self, self_t](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& t = self_t(vm, self);
        const auto* o = dynamic_cast<const TensorVal*>(&vm.arena().deref(a[0]));
        if (!o) throw KiritoError("matmul expects a Tensor");
        return tns::wrap([&]() -> Handle {
            if (!t.isComplex() && !o->isComplex()) return tns::make(vm, tensor::matmul(std::get<FT>(t.store), std::get<FT>(o->store)));
            CT x = t.isComplex() ? std::get<CT>(t.store) : tns::toComplex(std::get<FT>(t.store));
            CT y = o->isComplex() ? std::get<CT>(o->store) : tns::toComplex(std::get<FT>(o->store));
            return tns::make(vm, tensor::matmul(x, y));
        });
    });
    if (name == "dot") return bind("dot", {"other"}, [self, self_t](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& t = self_t(vm, self);
        const auto* o = dynamic_cast<const TensorVal*>(&vm.arena().deref(a[0]));
        if (!o) throw KiritoError("dot expects a Tensor");
        return tns::wrap([&]() -> Handle {
            if (!t.isComplex() && !o->isComplex()) return vm.makeFloat(tensor::dot(std::get<FT>(t.store), std::get<FT>(o->store)));
            CT x = t.isComplex() ? std::get<CT>(t.store) : tns::toComplex(std::get<FT>(t.store));
            CT y = o->isComplex() ? std::get<CT>(o->store) : tns::toComplex(std::get<FT>(o->store));
            return cpx::make(vm, tensor::dot(x, y));
        });
    });
    // reductions: sum/mean/prod take an optional axis; min/max are whole-tensor and Float-only.
    auto reduce = [&](const char* nm, char kind) {
        return bind(nm, {"axis"}, [self, self_t, axisOf, kind](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            auto& t = self_t(vm, self);
            int64_t axis = axisOf(vm, a);
            return tns::wrap([&]() -> Handle {
                auto combF = [kind](double x, double y) { return kind == '+' ? x + y : x * y; };
                auto combC = [kind](cdouble x, cdouble y) { return kind == '+' ? x + y : x * y; };
                if (axis < 0) {  // whole-tensor reduction -> a scalar
                    if (t.isComplex()) {
                        const CT& c = std::get<CT>(t.store);
                        return cpx::make(vm, kind == '+' ? tensor::sumAll(c) : tensor::prodAll(c));
                    }
                    const FT& f = std::get<FT>(t.store);
                    double r = kind == '+' ? tensor::sumAll(f) : tensor::prodAll(f);
                    return vm.makeFloat(r);
                }
                std::size_t ax = static_cast<std::size_t>(axis);
                if (t.isComplex()) return tns::make(vm, tensor::reduceAxis(std::get<CT>(t.store), ax, combC));
                return tns::make(vm, tensor::reduceAxis(std::get<FT>(t.store), ax, combF));
            });
        });
    };
    if (name == "sum") return reduce("sum", '+');
    if (name == "prod") return reduce("prod", '*');
    if (name == "mean") return bind("mean", {"axis"}, [self, self_t, axisOf](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& t = self_t(vm, self);
        int64_t axis = axisOf(vm, a);
        return tns::wrap([&]() -> Handle {
            if (axis < 0) {
                double cnt = static_cast<double>(t.size());
                if (cnt == 0) throw KiritoError("mean of an empty tensor");
                if (t.isComplex()) return cpx::make(vm, tensor::sumAll(std::get<CT>(t.store)) / cdouble(cnt, 0.0));
                return vm.makeFloat(tensor::sumAll(std::get<FT>(t.store)) / cnt);
            }
            std::size_t ax = static_cast<std::size_t>(axis);
            double cnt = static_cast<double>(t.shape()[ax]);
            if (t.isComplex()) {
                CT s = tensor::reduceAxis(std::get<CT>(t.store), ax, [](cdouble x, cdouble y) { return x + y; });
                for (auto& z : s.data) z /= cdouble(cnt, 0.0);
                return tns::make(vm, std::move(s));
            }
            FT s = tensor::reduceAxis(std::get<FT>(t.store), ax, [](double x, double y) { return x + y; });
            for (auto& x : s.data) x /= cnt;
            return tns::make(vm, std::move(s));
        });
    });
    if (name == "min" || name == "max") {
        bool wantMax = name == "max";
        return bind(wantMax ? "max" : "min", {}, [self, self_t, wantMax](KiritoVM& vm, std::span<const Handle>) -> Handle {
            auto& t = self_t(vm, self);
            if (t.isComplex()) throw KiritoError("min/max is not defined for a Complex tensor (complex values are unordered)");
            return tns::wrap([&]() -> Handle {
                const FT& f = std::get<FT>(t.store);
                return vm.makeFloat(wantMax ? tensor::maxAll(f) : tensor::minAll(f));
            });
        });
    }
    return Object::getAttr(vm, self, name);
}

// ----------------------------------------------------------------------------------- the module
class TensorModule : public NativeModule {
public:
    std::string name() const override { return "tensor"; }
    void setup(ModuleBuilder& m) override {
        // Tensor(nested[, dtype]) — build from a (rectangular) nested list; dtype defaults to Float.
        m.fn("Tensor", {{"data"}, {"dtype", "", m.vm().none()}}, "Tensor", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "Tensor");
            bool wantC = a.size() > 1 && tns::wantsComplex(vm, a[1]);
            Value arg = args[0];
            // infer shape by descending the first elements
            tensor::Shape shape;
            Value cur = arg;
            while (cur.isList()) {
                auto it = cur.items();
                shape.push_back(it.size());
                if (it.empty()) break;
                cur = it[0];
            }
            tns::checkSize(shape);
            // collect the leaves in row-major order, validating rectangularity
            std::vector<Handle> flat;
            std::function<void(Value, std::size_t)> rec = [&](Value v, std::size_t d) {
                if (d == shape.size()) { flat.push_back(v.handle()); return; }
                if (!v.isList()) throw KiritoError("Tensor: nested list is ragged or irregular");
                auto it = v.items();
                if (it.size() != shape[d]) throw KiritoError("Tensor: nested list is ragged (rows differ in length)");
                for (Value e : it) rec(e, d + 1);
            };
            rec(arg, 0);
            if (wantC) {
                std::vector<cdouble> data;
                for (Handle h : flat) data.push_back(cpx::asComplex(vm, h, "Tensor element"));
                return tns::make(vm, TensorVal::CT(shape, std::move(data)));
            }
            std::vector<double> data;
            for (Handle h : flat) data.push_back(Value(vm, h).asFloat("Tensor element"));
            return tns::make(vm, TensorVal::FT(shape, std::move(data)));
        });
        m.alias("tensor", "Tensor");

        auto filled = [](const char* nm, double fillReal) {
            return [nm, fillReal](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                Args args(vm, a, nm);
                tensor::Shape shape = tns::readShape(args[0]);
                tns::checkSize(shape);
                bool wantC = a.size() > 1 && tns::wantsComplex(vm, a[1]);
                if (wantC) return tns::make(vm, TensorVal::CT(shape, cdouble(fillReal, 0.0)));
                return tns::make(vm, TensorVal::FT(shape, fillReal));
            };
        };
        m.fn("zeros", {{"shape", "List"}, {"dtype", "", m.vm().none()}}, "Tensor", filled("zeros", 0.0));
        m.fn("ones", {{"shape", "List"}, {"dtype", "", m.vm().none()}}, "Tensor", filled("ones", 1.0));
        m.fn("full", {{"shape", "List"}, {"value", "Number"}, {"dtype", "", m.vm().none()}}, "Tensor", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "full");
            tensor::Shape shape = tns::readShape(args[0]);
            tns::checkSize(shape);
            bool wantC = a.size() > 2 && tns::wantsComplex(vm, a[2]);
            if (wantC) return tns::make(vm, TensorVal::CT(shape, cpx::asComplex(vm, a[1], "full value")));
            return tns::make(vm, TensorVal::FT(shape, args[1].asFloat("full value")));
        });
        m.fn("eye", {{"n", "Integer"}, {"dtype", "", m.vm().none()}}, "Tensor", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::size_t n = static_cast<std::size_t>(Args(vm, a, "eye")[0].asInt("n"));
            bool wantC = a.size() > 1 && tns::wantsComplex(vm, a[1]);
            if (wantC) {
                TensorVal::CT t(tensor::Shape{n, n});
                for (std::size_t i = 0; i < n; ++i) t.data[i * n + i] = cdouble(1.0, 0.0);
                return tns::make(vm, std::move(t));
            }
            TensorVal::FT t(tensor::Shape{n, n});
            for (std::size_t i = 0; i < n; ++i) t.data[i * n + i] = 1.0;
            return tns::make(vm, std::move(t));
        });
        // arange(stop) or arange(start, stop[, step]) -> a 1-D Float tensor.
        m.fn("arange", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "arange");
            double start = 0, stop, step = 1;
            if (args.size() == 1) stop = args[0].asFloat("stop");
            else { start = args[0].asFloat("start"); stop = args[1].asFloat("stop"); if (args.size() > 2) step = args[2].asFloat("step"); }
            if (step == 0) throw KiritoError("arange step must be non-zero");
            std::vector<double> data;
            if (step > 0) for (double x = start; x < stop; x += step) { data.push_back(x); if (data.size() > tns::kMaxElems) throw KiritoError("Tensor too large"); }
            else for (double x = start; x > stop; x += step) { data.push_back(x); if (data.size() > tns::kMaxElems) throw KiritoError("Tensor too large"); }
            std::size_t n = data.size();
            return tns::make(vm, TensorVal::FT(tensor::Shape{n}, std::move(data)));
        });
    }
};

}  // namespace kirito

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
#endif
