#ifndef KIRITO_STDLIB_TENSOR_HPP
#define KIRITO_STDLIB_TENSOR_HPP

#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "builtins.hpp"
#include "collections.hpp"
#include "module.hpp"
#include "native.hpp"
#include "stdlib_complex.hpp"  // ComplexVal, cdouble, complexToString, cpx::asComplex
#include "tensor.hpp"

namespace kirito {

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#endif

// ---- autograd graph node ----------------------------------------------------------------------
// Reverse-mode automatic differentiation, Float-only. A tensor produced by a differentiable op
// records how it was made: its grad-requiring inputs (`parents`, kept alive for the GC) and a
// `backward` rule that, given dL/d(this), returns dL/d(parent_i) for each parent — already reduced
// to that parent's shape. Leaves (user tensors marked `requiresgrad`) have no node. The graph
// records *operations*, never where the data lives, so a future GPU backend reuses it unchanged.
struct TensorGradNode {
    std::vector<Handle> parents;
    std::function<std::vector<tensor::Tensor<double>>(const tensor::Tensor<double>&)> backward;
};

// An N-dimensional tensor exposed to Kirito. The element type (dtype) is chosen at runtime: Float
// (double, the default) or Complex (std::complex<double>). The numeric work is the shared
// `kirito::tensor` engine; this class is the Kirito-facing wrapper + dispatch + autograd. (The
// engine is generic in T, so a third party can instantiate it for other element types in C++.)
class TensorVal : public NativeClass<TensorVal> {
public:
    static constexpr const char* kTypeName = "Tensor";
    using FT = tensor::Tensor<double>;
    using CT = tensor::Tensor<cdouble>;
    std::variant<FT, CT> store;  // index 0 = Float, 1 = Complex

    // autograd state (Float-only). `requiresGrad` marks a tensor as part of the differentiable
    // graph; `grad` accumulates dL/dself after backward(); `node` records the producing op (null
    // for a leaf). Complex tensors never carry grad.
    bool requiresGrad = false;
    std::optional<FT> grad;
    std::shared_ptr<TensorGradNode> node;

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

    // The autograd graph keeps its inputs reachable for the GC.
    void children(std::vector<Handle>& out) const override {
        if (node) for (Handle p : node->parents) out.push_back(p);
    }

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
                "prod(axis = None) -> Tensor", "min() -> Float", "max() -> Float",
                // autograd
                "requiresgrad(flag = None) -> Bool", "grad: Tensor", "backward(seed = None) -> None",
                "zerograd() -> None", "detach() -> Tensor",
                // differentiable element-wise math
                "exp() -> Tensor", "log() -> Tensor", "log10() -> Tensor", "log2() -> Tensor",
                "sqrt() -> Tensor", "cbrt() -> Tensor", "square() -> Tensor", "pow(p) -> Tensor",
                "reciprocal() -> Tensor", "abs() -> Tensor", "sign() -> Tensor",
                "sin() -> Tensor", "cos() -> Tensor", "tan() -> Tensor",
                "asin() -> Tensor", "acos() -> Tensor", "atan() -> Tensor",
                "sinh() -> Tensor", "cosh() -> Tensor", "tanh() -> Tensor",
                "asinh() -> Tensor", "acosh() -> Tensor", "atanh() -> Tensor",
                "relu() -> Tensor", "sigmoid() -> Tensor", "softplus() -> Tensor", "erf() -> Tensor"};
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

// A per-VM grad-tracking flag, stored as a hidden member of the `tensor` module (so it is VM-scoped,
// not a global). `with tensor.nograd():` flips it off; the differentiable ops consult it.
class TensorGradFlag : public NativeClass<TensorGradFlag> {
public:
    static constexpr const char* kTypeName = "TensorGradFlag";
    bool enabled = true;
    std::string str(StringifyCtx&) const override { return "<tensor grad-mode>"; }
};

// The differentiable element-wise math ops. Each is a unary map with a known local derivative, so it
// participates in autograd.
enum class MathOp {
    Exp, Log, Log10, Log2, Sqrt, Cbrt, Square, Reciprocal, Abs, Sign,
    Sin, Cos, Tan, Asin, Acos, Atan,
    Sinh, Cosh, Tanh, Asinh, Acosh, Atanh,
    Relu, Sigmoid, Softplus, Erf
};

namespace tns {

using FT = TensorVal::FT;
using CT = TensorVal::CT;

inline TensorVal& asT(KiritoVM& vm, Handle h) { return static_cast<TensorVal&>(vm.arena().deref(h)); }

inline constexpr std::size_t kMaxElems = 64ull * 1024 * 1024;
inline void checkSize(const tensor::Shape& s) {
    std::size_t n = 1;
    for (std::size_t d : s) {
        if (d != 0 && n > kMaxElems / d) throw KiritoError("Tensor too large");
        n *= d;
    }
}

// Promote a Float tensor to Complex (reals on the real axis).
inline CT toComplex(const FT& t) {
    CT out(t.shape);
    for (std::size_t i = 0; i < t.data.size(); ++i) out.data[i] = cdouble(t.data[i], 0.0);
    return out;
}

inline Handle make(KiritoVM& vm, FT t) { checkSize(t.shape); return vm.alloc(std::make_unique<TensorVal>(std::move(t))); }
inline Handle make(KiritoVM& vm, CT t) { checkSize(t.shape); return vm.alloc(std::make_unique<TensorVal>(std::move(t))); }

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

// ---- grad-mode flag (VM-scoped, lives on the tensor module) ----------------------------------
inline TensorGradFlag& gradMode(KiritoVM& vm) {
    Handle m = vm.importModule("tensor");
    ModuleValue& mod = static_cast<ModuleValue&>(vm.arena().deref(m));
    auto it = mod.members.find("_grad");
    if (it == mod.members.end()) throw KiritoError("tensor: grad-mode flag missing");
    return static_cast<TensorGradFlag&>(vm.arena().deref(it->second));
}
inline bool gradEnabled(KiritoVM& vm) { return gradMode(vm).enabled; }

// Tracking is on, and at least one Float input requires grad.
inline bool wantsGrad(KiritoVM& vm, std::initializer_list<const TensorVal*> ins) {
    if (!gradEnabled(vm)) return false;
    for (const TensorVal* t : ins) if (t && !t->isComplex() && t->requiresGrad) return true;
    return false;
}

// ---- gradient shape helpers ------------------------------------------------------------------
// Sum a gradient back to a (broadcast-from) shape: undo NumPy broadcasting by summing the expanded
// axes (PyTorch's "sum_to"). Leading extra axes are summed away; size-1 axes are summed with keepdim.
inline FT sumTo(FT g, const tensor::Shape& target) {
    auto plus = [](double x, double y) { return x + y; };
    while (g.shape.size() > target.size()) g = tensor::reduceAxis(g, 0, plus);
    for (std::size_t i = 0; i < target.size(); ++i) {
        if (target[i] == 1 && g.shape[i] != 1) {
            FT r = tensor::reduceAxis(g, i, plus);
            tensor::Shape ns = g.shape; ns[i] = 1;
            g = tensor::reshape(r, ns);
        }
    }
    if (g.shape != target) g = tensor::reshape(g, target);
    return g;
}

// Expand a tensor to a (broadcast-compatible) target shape.
inline FT broadcastTo(const FT& t, const tensor::Shape& target) { return tensor::add(FT(target, 0.0), t); }

// Swap the last two axes (the batched matrix transpose).
inline FT transposeLast2(const FT& t) {
    if (t.ndim() < 2) throw tensor::TensorError("transpose of last two axes needs rank >= 2");
    tensor::Shape ax(t.ndim());
    for (std::size_t i = 0; i < t.ndim(); ++i) ax[i] = i;
    std::swap(ax[t.ndim() - 1], ax[t.ndim() - 2]);
    return tensor::permute(t, ax);
}

// Allocate a Float tensor and wire its grad node (only the caller's grad-requiring inputs become
// parents; `bw` must return one gradient per parent, in order).
inline Handle makeAutogradFloat(KiritoVM& vm, FT out, std::vector<Handle> parents,
                                std::function<std::vector<FT>(const FT&)> bw) {
    RootScope rs(vm);
    for (Handle p : parents) rs.add(p);
    Handle h = make(vm, std::move(out));
    TensorVal& tv = asT(vm, h);
    tv.requiresGrad = true;
    tv.node = std::make_shared<TensorGradNode>();
    tv.node->parents = std::move(parents);
    tv.node->backward = std::move(bw);
    return h;
}

// ---- differentiable primitive ops (Float, grad-aware; Complex routes through with no grad) -----

inline Handle g_binop(KiritoVM& vm, char op, Handle ah, Handle bh) {
    TensorVal& A = asT(vm, ah);
    TensorVal& B = asT(vm, bh);
    const FT& a = std::get<FT>(A.store);
    const FT& b = std::get<FT>(B.store);
    FT out = op == '+' ? tensor::add(a, b) : op == '-' ? tensor::sub(a, b)
           : op == '*' ? tensor::mul(a, b) : tensor::div(a, b);
    if (!wantsGrad(vm, {&A, &B})) return make(vm, std::move(out));
    bool ag = A.requiresGrad, bg = B.requiresGrad;
    tensor::Shape ashape = a.shape, bshape = b.shape;
    FT acopy, bcopy;
    if (op == '*' || op == '/') { acopy = a; bcopy = b; }
    std::vector<Handle> parents;
    if (ag) parents.push_back(ah);
    if (bg) parents.push_back(bh);
    auto bw = [op, ag, bg, ashape, bshape, acopy, bcopy](const FT& g) -> std::vector<FT> {
        std::vector<FT> r;
        if (op == '+') {
            if (ag) r.push_back(sumTo(g, ashape));
            if (bg) r.push_back(sumTo(g, bshape));
        } else if (op == '-') {
            if (ag) r.push_back(sumTo(g, ashape));
            if (bg) r.push_back(sumTo(tensor::negate(g), bshape));
        } else if (op == '*') {
            if (ag) r.push_back(sumTo(tensor::mul(g, bcopy), ashape));
            if (bg) r.push_back(sumTo(tensor::mul(g, acopy), bshape));
        } else {  // '/'  d(a/b): da = g/b ; db = -g*a/b^2
            if (ag) r.push_back(sumTo(tensor::div(g, bcopy), ashape));
            if (bg) r.push_back(sumTo(tensor::negate(tensor::div(tensor::mul(g, acopy), tensor::mul(bcopy, bcopy))), bshape));
        }
        return r;
    };
    return makeAutogradFloat(vm, std::move(out), std::move(parents), std::move(bw));
}

inline Handle g_scalar(KiritoVM& vm, char op, Handle ah, double s) {
    TensorVal& A = asT(vm, ah);
    const FT& a = std::get<FT>(A.store);
    FT out = tensor::scalarOp(a, s, op);
    if (!wantsGrad(vm, {&A})) return make(vm, std::move(out));
    auto bw = [op, s](const FT& g) -> std::vector<FT> {
        FT da = (op == '+' || op == '-') ? g : (op == '*') ? tensor::scalarOp(g, s, '*') : tensor::scalarOp(g, s, '/');
        return {da};
    };
    return makeAutogradFloat(vm, std::move(out), {ah}, std::move(bw));
}

inline Handle g_neg(KiritoVM& vm, Handle ah) {
    TensorVal& A = asT(vm, ah);
    const FT& a = std::get<FT>(A.store);
    FT out = tensor::negate(a);
    if (!wantsGrad(vm, {&A})) return make(vm, std::move(out));
    auto bw = [](const FT& g) -> std::vector<FT> { return {tensor::negate(g)}; };
    return makeAutogradFloat(vm, std::move(out), {ah}, std::move(bw));
}

inline Handle g_matmul(KiritoVM& vm, Handle ah, Handle bh) {
    TensorVal& A = asT(vm, ah);
    TensorVal& B = asT(vm, bh);
    if (A.isComplex() || B.isComplex()) {  // complex matmul: no grad
        CT x = A.isComplex() ? std::get<CT>(A.store) : toComplex(std::get<FT>(A.store));
        CT y = B.isComplex() ? std::get<CT>(B.store) : toComplex(std::get<FT>(B.store));
        return make(vm, tensor::matmul(x, y));
    }
    const FT& a = std::get<FT>(A.store);
    const FT& b = std::get<FT>(B.store);
    FT out = tensor::matmul(a, b);
    if (!wantsGrad(vm, {&A, &B})) return make(vm, std::move(out));
    bool ag = A.requiresGrad, bg = B.requiresGrad;
    tensor::Shape ashape = a.shape, bshape = b.shape;
    FT acopy = a, bcopy = b;
    std::vector<Handle> parents;
    if (ag) parents.push_back(ah);
    if (bg) parents.push_back(bh);
    auto bw = [ag, bg, ashape, bshape, acopy, bcopy](const FT& g) -> std::vector<FT> {
        std::vector<FT> r;
        if (ag) r.push_back(sumTo(tensor::matmul(g, transposeLast2(bcopy)), ashape));
        if (bg) r.push_back(sumTo(tensor::matmul(transposeLast2(acopy), g), bshape));
        return r;
    };
    return makeAutogradFloat(vm, std::move(out), std::move(parents), std::move(bw));
}

inline Handle g_transpose(KiritoVM& vm, Handle ah) {
    TensorVal& A = asT(vm, ah);
    if (A.isComplex()) return make(vm, tensor::transpose(std::get<CT>(A.store)));
    const FT& a = std::get<FT>(A.store);
    FT out = tensor::transpose(a);
    if (!wantsGrad(vm, {&A})) return make(vm, std::move(out));
    auto bw = [](const FT& g) -> std::vector<FT> { return {tensor::transpose(g)}; };
    return makeAutogradFloat(vm, std::move(out), {ah}, std::move(bw));
}

inline Handle g_permute(KiritoVM& vm, Handle ah, tensor::Shape axes) {
    TensorVal& A = asT(vm, ah);
    if (A.isComplex()) return make(vm, tensor::permute(std::get<CT>(A.store), axes));
    const FT& a = std::get<FT>(A.store);
    FT out = tensor::permute(a, axes);
    if (!wantsGrad(vm, {&A})) return make(vm, std::move(out));
    tensor::Shape inv(axes.size());
    for (std::size_t i = 0; i < axes.size(); ++i) inv[axes[i]] = i;
    auto bw = [inv](const FT& g) -> std::vector<FT> { return {tensor::permute(g, inv)}; };
    return makeAutogradFloat(vm, std::move(out), {ah}, std::move(bw));
}

inline Handle g_reshape(KiritoVM& vm, Handle ah, tensor::Shape ns) {
    TensorVal& A = asT(vm, ah);
    if (A.isComplex()) return make(vm, tensor::reshape(std::get<CT>(A.store), ns));
    const FT& a = std::get<FT>(A.store);
    tensor::Shape ashape = a.shape;
    FT out = tensor::reshape(a, ns);
    if (!wantsGrad(vm, {&A})) return make(vm, std::move(out));
    auto bw = [ashape](const FT& g) -> std::vector<FT> { return {tensor::reshape(g, ashape)}; };
    return makeAutogradFloat(vm, std::move(out), {ah}, std::move(bw));
}

inline Handle g_flatten(KiritoVM& vm, Handle ah) { return g_reshape(vm, ah, tensor::Shape{asT(vm, ah).size()}); }

inline Handle g_sum(KiritoVM& vm, Handle ah, int64_t axis) {
    TensorVal& A = asT(vm, ah);
    if (A.isComplex()) {
        const CT& c = std::get<CT>(A.store);
        if (axis < 0) return cpx::make(vm, tensor::sumAll(c));
        return make(vm, tensor::reduceAxis(c, static_cast<std::size_t>(axis), [](cdouble x, cdouble y) { return x + y; }));
    }
    const FT& a = std::get<FT>(A.store);
    tensor::Shape ashape = a.shape;
    if (axis < 0) {  // whole-tensor sum
        double total = tensor::sumAll(a);
        if (!wantsGrad(vm, {&A})) return vm.makeFloat(total);  // non-grad: a scalar Float (as before)
        FT out(tensor::Shape{}, std::vector<double>{total});   // grad: a 0-D tensor, keeps the graph
        auto bw = [ashape](const FT& g) -> std::vector<FT> {
            return {FT(ashape, g.data.empty() ? 0.0 : g.data[0])};
        };
        return makeAutogradFloat(vm, std::move(out), {ah}, std::move(bw));
    }
    std::size_t ax = static_cast<std::size_t>(axis);
    FT out = tensor::reduceAxis(a, ax, [](double x, double y) { return x + y; });
    if (!wantsGrad(vm, {&A})) return make(vm, std::move(out));
    auto bw = [ashape, ax](const FT& g) -> std::vector<FT> {
        tensor::Shape keep = ashape; keep[ax] = 1;
        return {broadcastTo(tensor::reshape(g, keep), ashape)};
    };
    return makeAutogradFloat(vm, std::move(out), {ah}, std::move(bw));
}

inline Handle g_mean(KiritoVM& vm, Handle ah, int64_t axis) {
    TensorVal& A = asT(vm, ah);
    if (A.isComplex()) {
        const CT& c = std::get<CT>(A.store);
        if (axis < 0) {
            double n = static_cast<double>(c.data.size());
            if (n == 0) throw KiritoError("mean of an empty tensor");
            return cpx::make(vm, tensor::sumAll(c) / cdouble(n, 0.0));
        }
        std::size_t ax = static_cast<std::size_t>(axis);
        double cnt = static_cast<double>(c.shape[ax]);
        CT s = tensor::reduceAxis(c, ax, [](cdouble x, cdouble y) { return x + y; });
        for (auto& z : s.data) z /= cdouble(cnt, 0.0);
        return make(vm, std::move(s));
    }
    const FT& a = std::get<FT>(A.store);
    tensor::Shape ashape = a.shape;
    if (axis < 0) {
        double n = static_cast<double>(a.size());
        if (n == 0) throw KiritoError("mean of an empty tensor");
        double mean = tensor::sumAll(a) / n;
        if (!wantsGrad(vm, {&A})) return vm.makeFloat(mean);
        FT out(tensor::Shape{}, std::vector<double>{mean});
        auto bw = [ashape, n](const FT& g) -> std::vector<FT> {
            return {FT(ashape, (g.data.empty() ? 0.0 : g.data[0]) / n)};
        };
        return makeAutogradFloat(vm, std::move(out), {ah}, std::move(bw));
    }
    std::size_t ax = static_cast<std::size_t>(axis);
    double cnt = static_cast<double>(a.shape[ax]);
    FT s = tensor::reduceAxis(a, ax, [](double x, double y) { return x + y; });
    for (auto& v : s.data) v /= cnt;
    if (!wantsGrad(vm, {&A})) return make(vm, std::move(s));
    auto bw = [ashape, ax, cnt](const FT& g) -> std::vector<FT> {
        tensor::Shape keep = ashape; keep[ax] = 1;
        FT b = broadcastTo(tensor::reshape(g, keep), ashape);
        for (auto& v : b.data) v /= cnt;
        return {b};
    };
    return makeAutogradFloat(vm, std::move(s), {ah}, std::move(bw));
}

// Forward value of an element-wise math op.
inline double mathForward(MathOp k, double x) {
    switch (k) {
        case MathOp::Exp: return std::exp(x);
        case MathOp::Log: return std::log(x);
        case MathOp::Log10: return std::log10(x);
        case MathOp::Log2: return std::log2(x);
        case MathOp::Sqrt: return std::sqrt(x);
        case MathOp::Cbrt: return std::cbrt(x);
        case MathOp::Square: return x * x;
        case MathOp::Reciprocal: return 1.0 / x;
        case MathOp::Abs: return std::fabs(x);
        case MathOp::Sign: return (x > 0) - (x < 0);
        case MathOp::Sin: return std::sin(x);
        case MathOp::Cos: return std::cos(x);
        case MathOp::Tan: return std::tan(x);
        case MathOp::Asin: return std::asin(x);
        case MathOp::Acos: return std::acos(x);
        case MathOp::Atan: return std::atan(x);
        case MathOp::Sinh: return std::sinh(x);
        case MathOp::Cosh: return std::cosh(x);
        case MathOp::Tanh: return std::tanh(x);
        case MathOp::Asinh: return std::asinh(x);
        case MathOp::Acosh: return std::acosh(x);
        case MathOp::Atanh: return std::atanh(x);
        case MathOp::Relu: return x > 0 ? x : 0.0;
        case MathOp::Sigmoid: return 1.0 / (1.0 + std::exp(-x));
        case MathOp::Softplus: return std::log1p(std::exp(x));
        case MathOp::Erf: return std::erf(x);
    }
    return x;
}

// Local derivative dout/dx, given x and o = mathForward(k, x).
inline double mathDeriv(MathOp k, double x, double o) {
    static const double kInvSqrtPi2 = 1.1283791670955126;  // 2/sqrt(pi)
    static const double kLn10 = 2.302585092994046, kLn2 = 0.6931471805599453;
    switch (k) {
        case MathOp::Exp: return o;
        case MathOp::Log: return 1.0 / x;
        case MathOp::Log10: return 1.0 / (x * kLn10);
        case MathOp::Log2: return 1.0 / (x * kLn2);
        case MathOp::Sqrt: return 0.5 / o;
        case MathOp::Cbrt: return 1.0 / (3.0 * o * o);
        case MathOp::Square: return 2.0 * x;
        case MathOp::Reciprocal: return -o * o;
        case MathOp::Abs: return (x > 0) - (x < 0);
        case MathOp::Sign: return 0.0;
        case MathOp::Sin: return std::cos(x);
        case MathOp::Cos: return -std::sin(x);
        case MathOp::Tan: return 1.0 + o * o;
        case MathOp::Asin: return 1.0 / std::sqrt(1.0 - x * x);
        case MathOp::Acos: return -1.0 / std::sqrt(1.0 - x * x);
        case MathOp::Atan: return 1.0 / (1.0 + x * x);
        case MathOp::Sinh: return std::cosh(x);
        case MathOp::Cosh: return std::sinh(x);
        case MathOp::Tanh: return 1.0 - o * o;
        case MathOp::Asinh: return 1.0 / std::sqrt(x * x + 1.0);
        case MathOp::Acosh: return 1.0 / std::sqrt(x * x - 1.0);
        case MathOp::Atanh: return 1.0 / (1.0 - x * x);
        case MathOp::Relu: return x > 0 ? 1.0 : 0.0;
        case MathOp::Sigmoid: return o * (1.0 - o);
        case MathOp::Softplus: return 1.0 / (1.0 + std::exp(-x));
        case MathOp::Erf: return kInvSqrtPi2 * std::exp(-x * x);
    }
    return 1.0;
}

inline Handle g_math(KiritoVM& vm, Handle ah, MathOp k) {
    TensorVal& A = asT(vm, ah);
    if (A.isComplex())
        throw KiritoError("this math op is Float-only on tensors (use the complex module for complex math)");
    const FT& a = std::get<FT>(A.store);
    FT out = tensor::mapUnary(a, [k](double x) { return mathForward(k, x); });
    if (!wantsGrad(vm, {&A})) return make(vm, std::move(out));
    FT acopy = a, outcopy = out;
    auto bw = [k, acopy, outcopy](const FT& g) -> std::vector<FT> {
        FT d(acopy.shape);
        for (std::size_t i = 0; i < d.data.size(); ++i)
            d.data[i] = g.data[i] * mathDeriv(k, acopy.data[i], outcopy.data[i]);
        return {d};
    };
    return makeAutogradFloat(vm, std::move(out), {ah}, std::move(bw));
}

inline Handle g_pow(KiritoVM& vm, Handle ah, double p) {
    TensorVal& A = asT(vm, ah);
    if (A.isComplex()) throw KiritoError("pow is Float-only on tensors");
    const FT& a = std::get<FT>(A.store);
    FT out = tensor::mapUnary(a, [p](double x) { return std::pow(x, p); });
    if (!wantsGrad(vm, {&A})) return make(vm, std::move(out));
    FT acopy = a;
    auto bw = [acopy, p](const FT& g) -> std::vector<FT> {
        FT d(acopy.shape);
        for (std::size_t i = 0; i < d.data.size(); ++i)
            d.data[i] = g.data[i] * p * std::pow(acopy.data[i], p - 1.0);
        return {d};
    };
    return makeAutogradFloat(vm, std::move(out), {ah}, std::move(bw));
}

// ---- tensordot / contract (built from the differentiable primitives, so it is differentiable) --
inline Handle tensordot(KiritoVM& vm, Handle ah, Handle bh,
                        const std::vector<int64_t>& aaxes, const std::vector<int64_t>& baxes) {
    TensorVal& A = asT(vm, ah);
    TensorVal& B = asT(vm, bh);
    std::size_t an = A.ndim(), bn = B.ndim();
    if (aaxes.size() != baxes.size()) throw KiritoError("tensordot: the two axis lists must have equal length");
    auto norm = [](int64_t ax, std::size_t nd) -> std::size_t {
        if (ax < 0) ax += static_cast<int64_t>(nd);
        if (ax < 0 || static_cast<std::size_t>(ax) >= nd) throw KiritoError("tensordot: axis out of range");
        return static_cast<std::size_t>(ax);
    };
    std::vector<std::size_t> ac, bc;
    for (std::size_t i = 0; i < aaxes.size(); ++i) { ac.push_back(norm(aaxes[i], an)); bc.push_back(norm(baxes[i], bn)); }
    const tensor::Shape& ashape = A.shape();
    const tensor::Shape& bshape = B.shape();
    for (std::size_t i = 0; i < ac.size(); ++i)
        if (ashape[ac[i]] != bshape[bc[i]]) throw KiritoError("tensordot: contracted dimensions do not match");
    std::vector<bool> aIsC(an, false), bIsC(bn, false);
    for (std::size_t x : ac) aIsC[x] = true;
    for (std::size_t x : bc) bIsC[x] = true;
    // a -> [free_a..., contracted...]; b -> [contracted..., free_b...]
    tensor::Shape aperm, bperm;
    std::vector<std::size_t> afree, bfree;
    for (std::size_t i = 0; i < an; ++i) if (!aIsC[i]) { aperm.push_back(i); afree.push_back(i); }
    for (std::size_t x : ac) aperm.push_back(x);
    for (std::size_t x : bc) bperm.push_back(x);
    for (std::size_t i = 0; i < bn; ++i) if (!bIsC[i]) { bperm.push_back(i); bfree.push_back(i); }
    std::size_t M = 1, K = 1, N = 1;
    for (std::size_t i : afree) M *= ashape[i];
    for (std::size_t i : ac) K *= ashape[i];
    for (std::size_t i : bfree) N *= bshape[i];
    RootScope rs(vm);
    Handle ap = rs.add(g_permute(vm, ah, aperm));
    Handle ar = rs.add(g_reshape(vm, ap, tensor::Shape{M, K}));
    Handle bp = rs.add(g_permute(vm, bh, bperm));
    Handle br = rs.add(g_reshape(vm, bp, tensor::Shape{K, N}));
    Handle mm = rs.add(g_matmul(vm, ar, br));  // (M, N)
    tensor::Shape outshape;
    for (std::size_t i : afree) outshape.push_back(ashape[i]);
    for (std::size_t i : bfree) outshape.push_back(bshape[i]);
    return g_reshape(vm, mm, outshape);  // outshape may be empty -> a 0-D scalar tensor
}

// ---- reverse-mode backward pass --------------------------------------------------------------
inline void accumulateGrad(TensorVal& t, const FT& g) {
    if (!t.grad) t.grad = g;
    else t.grad = tensor::add(*t.grad, g);
}

inline void runBackward(KiritoVM& vm, Handle root, std::optional<FT> seed) {
    TensorVal& R = asT(vm, root);
    if (R.isComplex()) throw KiritoError("backward: gradients are Float-only");
    if (!R.requiresGrad) throw KiritoError("backward: this tensor does not require grad");
    FT s;
    if (seed) {
        s = *seed;
        if (s.shape != R.shape()) throw KiritoError("backward: the seed gradient shape must match the tensor");
    } else {
        if (R.size() != 1) throw KiritoError("backward: a seed gradient is required for a non-scalar tensor");
        s = FT(R.shape(), 1.0);
    }
    // reverse-topological order via DFS over the graph (parents first, node last)
    std::vector<Handle> order;
    std::unordered_set<Handle> seen;
    std::function<void(Handle)> dfs = [&](Handle h) {
        if (!seen.insert(h).second) return;
        TensorVal& tv = asT(vm, h);
        if (tv.node) for (Handle p : tv.node->parents) dfs(p);
        order.push_back(h);
    };
    dfs(root);
    accumulateGrad(R, s);
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        TensorVal& tv = asT(vm, *it);
        if (!tv.node || !tv.grad) continue;
        std::vector<FT> contrib = tv.node->backward(*tv.grad);
        for (std::size_t i = 0; i < tv.node->parents.size() && i < contrib.size(); ++i)
            accumulateGrad(asT(vm, tv.node->parents[i]), contrib[i]);
    }
}

// Parse an axis argument that may be a single Integer or a List of Integers.
inline std::vector<int64_t> readAxisList(Value v) {
    std::vector<int64_t> out;
    if (v.isInt()) { out.push_back(v.asInt("axis")); return out; }
    if (v.isList()) { for (Value e : v.items()) out.push_back(e.asInt("axis")); return out; }
    throw KiritoError("tensordot: axes must be an Integer or a List of Integers");
}

}  // namespace tns

inline Handle TensorVal::binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) {
    char c = op == BinOp::Add ? '+' : op == BinOp::Sub ? '-' : op == BinOp::Mul ? '*' : op == BinOp::Div ? '/' : 0;
    if (!c) throw KiritoError("Tensor does not support this operator (use .matmul for matrix products)");
    const Object& b = vm.arena().deref(rhs);
    const auto* ot = dynamic_cast<const TensorVal*>(&b);
    bool scalarComplex = dynamic_cast<const ComplexVal*>(&b) != nullptr;
    return tns::wrap([&]() -> Handle {
        if (ot) {  // tensor OP tensor
            if (!isComplex() && !ot->isComplex()) return tns::g_binop(vm, c, self, rhs);
            CT a = isComplex() ? std::get<CT>(store) : tns::toComplex(std::get<FT>(store));
            CT d = ot->isComplex() ? std::get<CT>(ot->store) : tns::toComplex(std::get<FT>(ot->store));
            switch (c) {
                case '+': return tns::make(vm, tensor::add(a, d));
                case '-': return tns::make(vm, tensor::sub(a, d));
                case '*': return tns::make(vm, tensor::mul(a, d));
                default: return tns::make(vm, tensor::div(a, d));
            }
        }
        // tensor OP scalar
        if (!isComplex() && !scalarComplex) return tns::g_scalar(vm, c, self, Value(vm, rhs).asFloat("scalar"));
        CT a = isComplex() ? std::get<CT>(store) : tns::toComplex(std::get<FT>(store));
        return tns::make(vm, tensor::scalarOp(a, cpx::asComplex(vm, rhs, "scalar"), c));
    });
}

inline Handle TensorVal::unary(KiritoVM& vm, UnOp op, Handle self) {
    if (op != UnOp::Neg) throw KiritoError("Tensor does not support this unary operator");
    if (isComplex()) return tns::make(vm, tensor::negate(std::get<CT>(store)));
    return tns::g_neg(vm, self);
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
    if (name == "reshape") return bind("reshape", {"shape"}, [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        tensor::Shape ns = tns::readShape(Value(vm, a[0]));
        return tns::wrap([&]() { return tns::g_reshape(vm, self, ns); });
    });
    if (name == "transpose") return bind("transpose", {}, [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
        return tns::wrap([&]() { return tns::g_transpose(vm, self); });
    });
    if (name == "permute") return bind("permute", {"axes"}, [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        tensor::Shape ax = tns::readShape(Value(vm, a[0]));
        return tns::wrap([&]() { return tns::g_permute(vm, self, ax); });
    });
    if (name == "flatten") return bind("flatten", {}, [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
        return tns::wrap([&]() { return tns::g_flatten(vm, self); });
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
    if (name == "matmul") return bind("matmul", {"other"}, [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        const auto* o = dynamic_cast<const TensorVal*>(&vm.arena().deref(a[0]));
        if (!o) throw KiritoError("matmul expects a Tensor");
        return tns::wrap([&]() { return tns::g_matmul(vm, self, a[0]); });
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
    if (name == "sum") return bind("sum", {"axis"}, [self, axisOf](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        int64_t axis = axisOf(vm, a);
        return tns::wrap([&]() { return tns::g_sum(vm, self, axis); });
    });
    if (name == "mean") return bind("mean", {"axis"}, [self, axisOf](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        int64_t axis = axisOf(vm, a);
        return tns::wrap([&]() { return tns::g_mean(vm, self, axis); });
    });
    if (name == "prod") return bind("prod", {"axis"}, [self, self_t, axisOf](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& t = self_t(vm, self);
        int64_t axis = axisOf(vm, a);
        return tns::wrap([&]() -> Handle {
            if (axis < 0) {
                if (t.isComplex()) return cpx::make(vm, tensor::prodAll(std::get<CT>(t.store)));
                return vm.makeFloat(tensor::prodAll(std::get<FT>(t.store)));
            }
            std::size_t ax = static_cast<std::size_t>(axis);
            if (t.isComplex()) return tns::make(vm, tensor::reduceAxis(std::get<CT>(t.store), ax, [](cdouble x, cdouble y) { return x * y; }));
            return tns::make(vm, tensor::reduceAxis(std::get<FT>(t.store), ax, [](double x, double y) { return x * y; }));
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

    // ---- autograd surface ----
    if (name == "grad") {  // the accumulated gradient (a Float Tensor), or None
        auto& t = self_t(vm, self);
        if (t.grad) return tns::make(vm, *t.grad);
        return vm.none();
    }
    if (name == "requiresgrad") return bind("requiresgrad", {"flag"}, [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& t = tns::asT(vm, self);
        if (a.empty()) return vm.makeBool(t.requiresGrad);  // getter
        bool f = vm.arena().deref(a[0]).truthy();
        if (f && t.isComplex()) throw KiritoError("gradients are Float-only (a Complex tensor cannot require grad)");
        t.requiresGrad = f;
        if (!f) t.node.reset();  // turning grad off detaches from the graph
        return vm.none();
    });
    if (name == "backward") return bind("backward", {"seed"}, [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        std::optional<FT> seed;
        if (!a.empty() && vm.arena().deref(a[0]).kind() != ValueKind::None) {
            const auto* st = dynamic_cast<const TensorVal*>(&vm.arena().deref(a[0]));
            if (!st || st->isComplex()) throw KiritoError("backward: the seed must be a Float Tensor");
            seed = std::get<FT>(st->store);
        }
        tns::runBackward(vm, self, seed);
        return vm.none();
    });
    if (name == "zerograd") return bind("zerograd", {}, [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
        tns::asT(vm, self).grad.reset();
        return vm.none();
    });
    if (name == "detach") return bind("detach", {}, [self](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto& t = tns::asT(vm, self);
        if (t.isComplex()) return tns::make(vm, std::get<CT>(t.store));
        return tns::make(vm, std::get<FT>(t.store));
    });
    if (name == "pow") return bind("pow", {"p"}, [self](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        double p = Value(vm, a[0]).asFloat("p");
        return tns::wrap([&]() { return tns::g_pow(vm, self, p); });
    });

    // ---- differentiable element-wise math (a generous unary set) ----
    static const std::unordered_map<std::string, MathOp> kMath = {
        {"exp", MathOp::Exp}, {"log", MathOp::Log}, {"log10", MathOp::Log10}, {"log2", MathOp::Log2},
        {"sqrt", MathOp::Sqrt}, {"cbrt", MathOp::Cbrt}, {"square", MathOp::Square},
        {"reciprocal", MathOp::Reciprocal}, {"abs", MathOp::Abs}, {"sign", MathOp::Sign},
        {"sin", MathOp::Sin}, {"cos", MathOp::Cos}, {"tan", MathOp::Tan},
        {"asin", MathOp::Asin}, {"acos", MathOp::Acos}, {"atan", MathOp::Atan},
        {"sinh", MathOp::Sinh}, {"cosh", MathOp::Cosh}, {"tanh", MathOp::Tanh},
        {"asinh", MathOp::Asinh}, {"acosh", MathOp::Acosh}, {"atanh", MathOp::Atanh},
        {"relu", MathOp::Relu}, {"sigmoid", MathOp::Sigmoid}, {"softplus", MathOp::Softplus},
        {"erf", MathOp::Erf}};
    if (auto mit = kMath.find(std::string(name)); mit != kMath.end()) {
        MathOp k = mit->second;
        return makeMethod(vm, std::string(name), {}, [self, k](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return tns::wrap([&]() { return tns::g_math(vm, self, k); });
        }, std::vector<Handle>{self});
    }
    return Object::getAttr(vm, self, name);
}

// `with tensor.nograd():` — disable grad tracking for the duration of the block (PyTorch's no_grad).
class NoGradCtx : public NativeClass<NoGradCtx> {
public:
    static constexpr const char* kTypeName = "NoGradContext";
    bool prev = true;
    std::string str(StringifyCtx&) const override { return "<nograd>"; }
    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override {
        if (name == "_enter_") return makeMethod(vm, "_enter_", {}, [self](KiritoVM& v, std::span<const Handle>) -> Handle {
            auto& c = static_cast<NoGradCtx&>(v.arena().deref(self));
            auto& gm = tns::gradMode(v);
            c.prev = gm.enabled;
            gm.enabled = false;
            return self;
        }, std::vector<Handle>{self});
        if (name == "_exit_") return makeMethod(vm, "_exit_", {}, [self](KiritoVM& v, std::span<const Handle>) -> Handle {
            auto& c = static_cast<NoGradCtx&>(v.arena().deref(self));
            tns::gradMode(v).enabled = c.prev;
            return v.none();
        }, std::vector<Handle>{self});
        return Object::getAttr(vm, self, name);
    }
};

// ----------------------------------------------------------------------------------- the module
class TensorModule : public NativeModule {
public:
    std::string name() const override { return "tensor"; }
    void setup(ModuleBuilder& m) override {
        // The VM-scoped grad-tracking flag (hidden module member consulted by the autograd ops).
        m.value("_grad", m.vm().alloc(std::make_unique<TensorGradFlag>()));

        // Tensor(nested[, dtype][, requiresgrad]) — build from a (rectangular) nested list.
        m.fn("Tensor", {{"data"}, {"dtype", "", m.vm().none()}, {"requiresgrad", "", m.vm().makeBool(false)}}, "Tensor",
             [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "Tensor");
            bool wantC = a.size() > 1 && tns::wantsComplex(vm, a[1]);
            bool reqGrad = a.size() > 2 && vm.arena().deref(a[2]).truthy();
            Value arg = args[0];
            tensor::Shape shape;
            Value cur = arg;
            while (cur.isList()) {
                auto it = cur.items();
                shape.push_back(it.size());
                if (it.empty()) break;
                cur = it[0];
            }
            tns::checkSize(shape);
            std::vector<Handle> flat;
            std::function<void(Value, std::size_t)> rec = [&](Value v, std::size_t d) {
                if (d == shape.size()) { flat.push_back(v.handle()); return; }
                if (!v.isList()) throw KiritoError("Tensor: nested list is ragged or irregular");
                auto it = v.items();
                if (it.size() != shape[d]) throw KiritoError("Tensor: nested list is ragged (rows differ in length)");
                for (Value e : it) rec(e, d + 1);
            };
            rec(arg, 0);
            Handle h;
            if (wantC) {
                std::vector<cdouble> data;
                for (Handle hh : flat) data.push_back(cpx::asComplex(vm, hh, "Tensor element"));
                h = tns::make(vm, TensorVal::CT(shape, std::move(data)));
            } else {
                std::vector<double> data;
                for (Handle hh : flat) data.push_back(Value(vm, hh).asFloat("Tensor element"));
                h = tns::make(vm, TensorVal::FT(shape, std::move(data)));
            }
            if (reqGrad) {
                if (wantC) throw KiritoError("gradients are Float-only (a Complex tensor cannot require grad)");
                tns::asT(vm, h).requiresGrad = true;
            }
            return h;
        });
        m.alias("tensor", "Tensor");

        auto filled = [](const char* nm, double fillReal) {
            return [nm, fillReal](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                Args args(vm, a, nm);
                tensor::Shape shape = tns::readShape(args[0]);
                tns::checkSize(shape);
                bool wantC = a.size() > 1 && tns::wantsComplex(vm, a[1]);
                bool reqGrad = a.size() > 2 && vm.arena().deref(a[2]).truthy();
                Handle h;
                if (wantC) h = tns::make(vm, TensorVal::CT(shape, cdouble(fillReal, 0.0)));
                else h = tns::make(vm, TensorVal::FT(shape, fillReal));
                if (reqGrad) {
                    if (wantC) throw KiritoError("gradients are Float-only (a Complex tensor cannot require grad)");
                    tns::asT(vm, h).requiresGrad = true;
                }
                return h;
            };
        };
        m.fn("zeros", {{"shape", "List"}, {"dtype", "", m.vm().none()}, {"requiresgrad", "", m.vm().makeBool(false)}}, "Tensor", filled("zeros", 0.0));
        m.fn("ones", {{"shape", "List"}, {"dtype", "", m.vm().none()}, {"requiresgrad", "", m.vm().makeBool(false)}}, "Tensor", filled("ones", 1.0));
        m.fn("full", {{"shape", "List"}, {"value", "Number"}, {"dtype", "", m.vm().none()}, {"requiresgrad", "", m.vm().makeBool(false)}}, "Tensor",
             [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "full");
            tensor::Shape shape = tns::readShape(args[0]);
            tns::checkSize(shape);
            bool wantC = a.size() > 2 && tns::wantsComplex(vm, a[2]);
            bool reqGrad = a.size() > 3 && vm.arena().deref(a[3]).truthy();
            Handle h;
            if (wantC) h = tns::make(vm, TensorVal::CT(shape, cpx::asComplex(vm, a[1], "full value")));
            else h = tns::make(vm, TensorVal::FT(shape, args[1].asFloat("full value")));
            if (reqGrad) {
                if (wantC) throw KiritoError("gradients are Float-only (a Complex tensor cannot require grad)");
                tns::asT(vm, h).requiresGrad = true;
            }
            return h;
        });
        m.fn("eye", {{"n", "Integer"}, {"dtype", "", m.vm().none()}, {"requiresgrad", "", m.vm().makeBool(false)}}, "Tensor",
             [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::size_t n = static_cast<std::size_t>(Args(vm, a, "eye")[0].asInt("n"));
            bool wantC = a.size() > 1 && tns::wantsComplex(vm, a[1]);
            bool reqGrad = a.size() > 2 && vm.arena().deref(a[2]).truthy();
            Handle h;
            if (wantC) {
                TensorVal::CT t(tensor::Shape{n, n});
                for (std::size_t i = 0; i < n; ++i) t.data[i * n + i] = cdouble(1.0, 0.0);
                h = tns::make(vm, std::move(t));
            } else {
                TensorVal::FT t(tensor::Shape{n, n});
                for (std::size_t i = 0; i < n; ++i) t.data[i * n + i] = 1.0;
                h = tns::make(vm, std::move(t));
            }
            if (reqGrad) {
                if (wantC) throw KiritoError("gradients are Float-only (a Complex tensor cannot require grad)");
                tns::asT(vm, h).requiresGrad = true;
            }
            return h;
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

        // tensordot(a, b, axes): contract over axes. `axes` is an Integer N (the last N axes of `a`
        // with the first N of `b`) or a [a-axes, b-axes] pair (each an Integer or a List). Built from
        // permute/reshape/matmul, so it is differentiable under autograd.
        m.fn("tensordot", {{"a", "Tensor"}, {"b", "Tensor"}, {"axes", "", m.vm().makeInt(2)}}, "Tensor",
             [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "tensordot");
            if (!dynamic_cast<const TensorVal*>(&vm.arena().deref(a[0])) || !dynamic_cast<const TensorVal*>(&vm.arena().deref(a[1])))
                throw KiritoError("tensordot expects two Tensors");
            Value ax = args[2];
            std::vector<int64_t> aaxes, baxes;
            if (ax.isInt()) {
                int64_t n = ax.asInt("axes");
                if (n < 0) throw KiritoError("tensordot: the axis count must be non-negative");
                std::size_t an = tns::asT(vm, a[0]).ndim();
                if (static_cast<std::size_t>(n) > an) throw KiritoError("tensordot: axis count exceeds the tensor rank");
                for (int64_t i = 0; i < n; ++i) { aaxes.push_back(static_cast<int64_t>(an) - n + i); baxes.push_back(i); }
            } else if (ax.isList()) {
                auto it = ax.items();
                if (it.size() != 2) throw KiritoError("tensordot: axes must be an Integer or a [a-axes, b-axes] pair");
                aaxes = tns::readAxisList(it[0]);
                baxes = tns::readAxisList(it[1]);
            } else {
                throw KiritoError("tensordot: axes must be an Integer or a [a-axes, b-axes] pair");
            }
            return tns::wrap([&]() { return tns::tensordot(vm, a[0], a[1], aaxes, baxes); });
        });
        // contract(a, b, aaxes, baxes): tensordot with the two axis lists given explicitly.
        m.fn("contract", {{"a", "Tensor"}, {"b", "Tensor"}, {"aaxes"}, {"baxes"}}, "Tensor",
             [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "contract");
            if (!dynamic_cast<const TensorVal*>(&vm.arena().deref(a[0])) || !dynamic_cast<const TensorVal*>(&vm.arena().deref(a[1])))
                throw KiritoError("contract expects two Tensors");
            std::vector<int64_t> aaxes = tns::readAxisList(args[2]);
            std::vector<int64_t> baxes = tns::readAxisList(args[3]);
            return tns::wrap([&]() { return tns::tensordot(vm, a[0], a[1], aaxes, baxes); });
        });

        // nograd(): a context manager that disables grad tracking inside a `with` block.
        m.fn("nograd", {}, "NoGradContext", [](KiritoVM& vm, std::span<const Handle>) -> Handle {
            return vm.alloc(std::make_unique<NoGradCtx>());
        });
    }
};

}  // namespace kirito

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
#endif
