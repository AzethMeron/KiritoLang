#ifndef KIRITO_STDLIB_COMPLEX_HPP
#define KIRITO_STDLIB_COMPLEX_HPP

#include <cmath>
#include <complex>
#include <cstddef>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"

namespace kirito {

// The native-binding idiom below re-uses `vm`/`self` as bound-method lambda parameters that
// intentionally shadow the enclosing getAttr/setup `vm`/`self` (same VM, by design). Silence
// -Wshadow for these mechanical bindings; it stays active in the evaluator/parser/lexer core.
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#endif

using cdouble = std::complex<double>;

// Approximate equality (relative+absolute), local so this header needn't depend on runtime.hpp's
// floatEqual (which is included after the stdlib headers in the umbrella).
inline bool cEq(double a, double b) {
    return std::fabs(a - b) <= 1e-9 * (1.0 + std::fabs(a) + std::fabs(b));
}

// Render a complex value as "a+bi" / "a-bi" (negative zero on the imaginary part normalized away).
inline std::string complexToString(cdouble z) {
    double im = z.imag();
    if (im == 0.0) im = 0.0;  // collapse -0.0 -> +0.0 so we never print "+-0.0i"
    std::string s = floatToString(z.real());
    if (im < 0.0) return s + "-" + floatToString(-im) + "i";
    return s + "+" + floatToString(im) + "i";
}

// ------------------------------------------------------------------- the Complex number value
class ComplexVal : public NativeClass<ComplexVal> {
public:
    static constexpr const char* kTypeName = "Complex";
    cdouble z;

    ComplexVal() = default;
    explicit ComplexVal(cdouble v) : z(v) {}
    ComplexVal(double re, double im) : z(re, im) {}

    bool truthy() const override { return z != cdouble(0.0, 0.0); }
    std::string str(StringifyCtx&) const override { return complexToString(z); }
    bool equals(const ObjectArena&, const Object& other) const override {
        if (const auto* c = dynamic_cast<const ComplexVal*>(&other))
            return cEq(z.real(), c->z.real()) && cEq(z.imag(), c->z.imag());
        // A Complex equals a real number when its imaginary part is zero (so Complex(2,0) == 2).
        if (other.kind() == ValueKind::Integer)
            return z.imag() == 0.0 && cEq(z.real(), static_cast<double>(static_cast<const IntVal&>(other).value()));
        if (other.kind() == ValueKind::Float)
            return z.imag() == 0.0 && cEq(z.real(), static_cast<const FloatVal&>(other).value());
        return false;
    }

    std::vector<std::string> inspectMembers() const override {
        return {"re: Float", "im: Float", "conjugate() -> Complex", "modulus() -> Float",
                "argument() -> Float", "norm2() -> Float", "is_zero() -> Bool"};
    }

    Handle binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) override;
    Handle unary(KiritoVM& vm, UnOp op, Handle self) override;
    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override;
};

namespace cpx {

// Read a Complex, Integer or Float as std::complex<double> (numbers sit on the real axis).
inline cdouble asComplex(KiritoVM& vm, Handle h, const char* who = "Complex") {
    const Object& o = vm.arena().deref(h);
    if (const auto* c = dynamic_cast<const ComplexVal*>(&o)) return c->z;
    if (o.kind() == ValueKind::Integer) return cdouble(static_cast<double>(static_cast<const IntVal&>(o).value()), 0.0);
    if (o.kind() == ValueKind::Float) return cdouble(static_cast<const FloatVal&>(o).value(), 0.0);
    throw KiritoError(std::string(who) + " expects a Complex or a number");
}

inline Handle make(KiritoVM& vm, cdouble z) { return vm.alloc(std::make_unique<ComplexVal>(z)); }

}  // namespace cpx

inline Handle ComplexVal::binary(KiritoVM& vm, BinOp op, Handle, Handle rhs) {
    // Equality is handled by the evaluator via equals(); ordering is undefined for complex numbers.
    if (op == BinOp::Lt || op == BinOp::Le || op == BinOp::Gt || op == BinOp::Ge)
        throw KiritoError("complex numbers are not ordered (no <, <=, >, >=)");
    cdouble b = cpx::asComplex(vm, rhs, "Complex arithmetic");
    switch (op) {
        case BinOp::Add: return cpx::make(vm, z + b);
        case BinOp::Sub: return cpx::make(vm, z - b);
        case BinOp::Mul: return cpx::make(vm, z * b);
        case BinOp::Div:
            if (b == cdouble(0.0, 0.0)) throw KiritoError("complex division by zero");
            return cpx::make(vm, z / b);
        case BinOp::Pow: return cpx::make(vm, std::pow(z, b));
        case BinOp::Eq: return vm.makeBool(cEq(z.real(), b.real()) && cEq(z.imag(), b.imag()));
        case BinOp::Ne: return vm.makeBool(!(cEq(z.real(), b.real()) && cEq(z.imag(), b.imag())));
        default: break;
    }
    throw KiritoError("Complex does not support this operator");
}

inline Handle ComplexVal::unary(KiritoVM& vm, UnOp op, Handle) {
    if (op == UnOp::Neg) return cpx::make(vm, -z);
    throw KiritoError("Complex does not support this unary operator");
}

inline Handle ComplexVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    auto self_z = [](KiritoVM& vm, Handle self) -> cdouble {
        return static_cast<ComplexVal&>(vm.arena().deref(self)).z;
    };
    auto bind = [&](const char* nm, NativeFn fn) {
        return makeMethod(vm, nm, {}, std::move(fn), std::vector<Handle>{self});
    };
    if (name == "re" || name == "real") return vm.makeFloat(z.real());
    if (name == "im" || name == "imag") return vm.makeFloat(z.imag());
    if (name == "conjugate") return bind("conjugate", [self, self_z](KiritoVM& vm, std::span<const Handle>) { return cpx::make(vm, std::conj(self_z(vm, self))); });
    if (name == "modulus" || name == "magnitude" || name == "abs")
        return bind("modulus", [self, self_z](KiritoVM& vm, std::span<const Handle>) { return vm.makeFloat(std::abs(self_z(vm, self))); });
    if (name == "argument" || name == "arg" || name == "phase")
        return bind("argument", [self, self_z](KiritoVM& vm, std::span<const Handle>) { return vm.makeFloat(std::arg(self_z(vm, self))); });
    if (name == "norm2")
        return bind("norm2", [self, self_z](KiritoVM& vm, std::span<const Handle>) { return vm.makeFloat(std::norm(self_z(vm, self))); });
    if (name == "is_zero")
        return bind("is_zero", [self, self_z](KiritoVM& vm, std::span<const Handle>) { return vm.makeBool(std::norm(self_z(vm, self)) < 1e-20); });
    return Object::getAttr(vm, self, name);
}

// ------------------------------------------------------------------- the complex Matrix value
class ComplexMatrixVal : public NativeClass<ComplexMatrixVal> {
public:
    static constexpr const char* kTypeName = "ComplexMatrix";
    std::size_t rows = 0, cols = 0;
    std::vector<cdouble> data;  // row-major

    ComplexMatrixVal() = default;
    ComplexMatrixVal(std::size_t r, std::size_t c, cdouble fill = cdouble(0.0, 0.0))
        : rows(r), cols(c), data(r * c, fill) {}

    cdouble& at(std::size_t r, std::size_t c) { return data[r * cols + c]; }
    cdouble at(std::size_t r, std::size_t c) const { return data[r * cols + c]; }

    // A ComplexMatrix with one dimension == 1 is a vector (row 1×n or column n×1).
    bool isVector() const { return rows == 1 || cols == 1; }

    std::string str(StringifyCtx&) const override {
        std::string s = "[";
        for (std::size_t r = 0; r < rows; ++r) {
            if (r) s += ", ";
            s += "[";
            for (std::size_t c = 0; c < cols; ++c) {
                if (c) s += ", ";
                s += complexToString(at(r, c));
            }
            s += "]";
        }
        return s + "]";
    }
    bool equals(const ObjectArena&, const Object& other) const override {
        const auto* m = dynamic_cast<const ComplexMatrixVal*>(&other);
        if (!m || m->rows != rows || m->cols != cols) return false;
        for (std::size_t i = 0; i < data.size(); ++i)
            if (!cEq(data[i].real(), m->data[i].real()) || !cEq(data[i].imag(), m->data[i].imag()))
                return false;
        return true;
    }
    std::vector<std::string> inspectMembers() const override {
        return {"rows() -> Integer", "cols() -> Integer", "shape() -> List",
                "get(row, col) -> Complex", "set(row, col, value)", "row(i) -> List",
                "transpose() -> ComplexMatrix", "conjugate() -> ComplexMatrix",
                "hermitian() -> ComplexMatrix", "determinant() -> Complex",
                "inverse() -> ComplexMatrix", "trace() -> Complex",
                "dot(other) -> Complex", "cross(other) -> ComplexMatrix", "norm() -> Float"};
    }

    Handle binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) override;
    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override;
    Handle getItem(KiritoVM& vm, std::span<const Handle> keys) override;
    void setItem(KiritoVM& vm, std::span<const Handle> keys, Handle value) override;

    static std::size_t indexOf(KiritoVM& vm, Handle h) {
        const Object& o = vm.arena().deref(h);
        if (o.kind() != ValueKind::Integer) throw KiritoError("ComplexMatrix index must be Integer");
        int64_t v = static_cast<const IntVal&>(o).value();
        if (v < 0) throw KiritoError("ComplexMatrix index out of range");
        return static_cast<std::size_t>(v);
    }
};

namespace cpx {

inline constexpr std::size_t kMaxElems = 16ull * 1024 * 1024;
inline std::unique_ptr<ComplexMatrixVal> makeMatrix(std::size_t r, std::size_t c, cdouble fill = cdouble(0.0, 0.0)) {
    if (c != 0 && r > kMaxElems / c) throw KiritoError("ComplexMatrix too large");
    return std::make_unique<ComplexMatrixVal>(r, c, fill);
}

// Determinant via Gaussian elimination with partial pivoting (pivot = largest |element|).
inline cdouble determinant(const ComplexMatrixVal& m) {
    if (m.rows != m.cols) throw KiritoError("determinant requires a square ComplexMatrix");
    std::size_t n = m.rows;
    std::vector<cdouble> a = m.data;
    cdouble det(1.0, 0.0);
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t piv = k;
        for (std::size_t i = k + 1; i < n; ++i)
            if (std::abs(a[i * n + k]) > std::abs(a[piv * n + k])) piv = i;
        if (std::abs(a[piv * n + k]) < 1e-15) return cdouble(0.0, 0.0);
        if (piv != k) { for (std::size_t j = 0; j < n; ++j) std::swap(a[k * n + j], a[piv * n + j]); det = -det; }
        det *= a[k * n + k];
        for (std::size_t i = k + 1; i < n; ++i) {
            cdouble f = a[i * n + k] / a[k * n + k];
            for (std::size_t j = k; j < n; ++j) a[i * n + j] -= f * a[k * n + j];
        }
    }
    return det;
}

// Inverse via Gauss-Jordan elimination on the augmented matrix [A | I] — O(n^3), the fast method
// (no cofactor/adjugate expansion). Partial pivoting keeps it numerically stable.
inline std::unique_ptr<ComplexMatrixVal> inverse(const ComplexMatrixVal& m) {
    if (m.rows != m.cols) throw KiritoError("inverse requires a square ComplexMatrix");
    std::size_t n = m.rows;
    std::vector<cdouble> a = m.data;
    auto inv = makeMatrix(n, n);
    for (std::size_t i = 0; i < n; ++i) inv->at(i, i) = cdouble(1.0, 0.0);
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t piv = k;
        for (std::size_t i = k + 1; i < n; ++i)
            if (std::abs(a[i * n + k]) > std::abs(a[piv * n + k])) piv = i;
        if (std::abs(a[piv * n + k]) < 1e-15) throw KiritoError("ComplexMatrix is singular (no inverse)");
        if (piv != k)
            for (std::size_t j = 0; j < n; ++j) {
                std::swap(a[k * n + j], a[piv * n + j]);
                std::swap(inv->at(k, j), inv->at(piv, j));
            }
        cdouble d = a[k * n + k];
        for (std::size_t j = 0; j < n; ++j) { a[k * n + j] /= d; inv->at(k, j) /= d; }
        for (std::size_t i = 0; i < n; ++i) {
            if (i == k) continue;
            cdouble f = a[i * n + k];
            for (std::size_t j = 0; j < n; ++j) { a[i * n + j] -= f * a[k * n + j]; inv->at(i, j) -= f * inv->at(k, j); }
        }
    }
    return inv;
}

}  // namespace cpx

inline Handle ComplexMatrixVal::binary(KiritoVM& vm, BinOp op, Handle, Handle rhs) {
    const Object& b = vm.arena().deref(rhs);
    const auto* other = dynamic_cast<const ComplexMatrixVal*>(&b);
    if (op == BinOp::Add || op == BinOp::Sub) {
        if (!other || other->rows != rows || other->cols != cols)
            throw KiritoError("ComplexMatrix +/- requires matrices of equal shape");
        auto r = cpx::makeMatrix(rows, cols);
        for (std::size_t i = 0; i < data.size(); ++i)
            r->data[i] = op == BinOp::Add ? data[i] + other->data[i] : data[i] - other->data[i];
        return vm.alloc(std::move(r));
    }
    if (op == BinOp::Mul) {
        if (other) {
            // Two vectors of the same shape: `*` is the scalar (Hermitian inner) product — a Complex,
            // sum(conj(a_i)·b_i), so v*v = sum|v_i|² is real and non-negative.
            if (isVector() && other->isVector() && rows == other->rows && cols == other->cols) {
                cdouble acc(0.0, 0.0);
                for (std::size_t i = 0; i < data.size(); ++i) acc += std::conj(data[i]) * other->data[i];
                return cpx::make(vm, acc);
            }
            // matrix product
            if (cols != other->rows) throw KiritoError("ComplexMatrix multiply: inner dimensions differ");
            auto r = cpx::makeMatrix(rows, other->cols);
            for (std::size_t i = 0; i < rows; ++i)
                for (std::size_t k = 0; k < cols; ++k) {
                    cdouble v = at(i, k);
                    for (std::size_t j = 0; j < other->cols; ++j) r->at(i, j) += v * other->at(k, j);
                }
            return vm.alloc(std::move(r));
        }
        cdouble s = cpx::asComplex(vm, rhs, "ComplexMatrix scalar");  // scalar (Complex/number)
        auto r = cpx::makeMatrix(rows, cols);
        for (std::size_t i = 0; i < data.size(); ++i) r->data[i] = data[i] * s;
        return vm.alloc(std::move(r));
    }
    throw KiritoError("ComplexMatrix does not support this operator");
}

inline Handle ComplexMatrixVal::getItem(KiritoVM& vm, std::span<const Handle> keys) {
    if (keys.size() == 1) {  // a whole row, as a List of Complex
        std::size_t r = indexOf(vm, keys[0]);
        if (r >= rows) throw KiritoError("ComplexMatrix row index out of range");
        RootScope rs(vm);
        auto list = std::make_unique<ListVal>();
        for (std::size_t c = 0; c < cols; ++c) list->elems.push_back(rs.add(cpx::make(vm, at(r, c))));
        return vm.alloc(std::move(list));
    }
    if (keys.size() == 2) {  // a single element
        std::size_t r = indexOf(vm, keys[0]), c = indexOf(vm, keys[1]);
        if (r >= rows || c >= cols) throw KiritoError("ComplexMatrix index out of range");
        return cpx::make(vm, at(r, c));
    }
    throw KiritoError("ComplexMatrix index needs 1 (row) or 2 (element) indices");
}

inline void ComplexMatrixVal::setItem(KiritoVM& vm, std::span<const Handle> keys, Handle value) {
    if (keys.size() != 2) throw KiritoError("ComplexMatrix element assignment needs two indices: m[i, j] = v");
    std::size_t r = indexOf(vm, keys[0]), c = indexOf(vm, keys[1]);
    if (r >= rows || c >= cols) throw KiritoError("ComplexMatrix index out of range");
    at(r, c) = cpx::asComplex(vm, value, "ComplexMatrix element");
}

inline Handle ComplexMatrixVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    auto self_m = [](KiritoVM& vm, Handle self) -> ComplexMatrixVal& {
        return static_cast<ComplexMatrixVal&>(vm.arena().deref(self));
    };
    auto bind = [&](const char* nm, std::vector<std::string> params, NativeFn fn) {
        return makeMethod(vm, nm, std::move(params), std::move(fn), std::vector<Handle>{self});
    };
    auto idx = [](KiritoVM& vm, Handle h) -> std::size_t { return ComplexMatrixVal::indexOf(vm, h); };
    if (name == "rows") return bind("rows", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) { return vm.makeInt(static_cast<int64_t>(self_m(vm, self).rows)); });
    if (name == "cols") return bind("cols", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) { return vm.makeInt(static_cast<int64_t>(self_m(vm, self).cols)); });
    if (name == "shape") return bind("shape", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto& m = self_m(vm, self);
        auto list = std::make_unique<ListVal>();
        list->elems.push_back(vm.makeInt(static_cast<int64_t>(m.rows)));
        list->elems.push_back(vm.makeInt(static_cast<int64_t>(m.cols)));
        return vm.alloc(std::move(list));
    });
    if (name == "get") return bind("get", {"row", "col"}, [self, self_m, idx](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& m = self_m(vm, self);
        std::size_t r = idx(vm, a[0]), c = idx(vm, a[1]);
        if (r >= m.rows || c >= m.cols) throw KiritoError("ComplexMatrix index out of range");
        return cpx::make(vm, m.at(r, c));
    });
    if (name == "set") return bind("set", {"row", "col", "value"}, [self, self_m, idx](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& m = self_m(vm, self);
        std::size_t r = idx(vm, a[0]), c = idx(vm, a[1]);
        if (r >= m.rows || c >= m.cols) throw KiritoError("ComplexMatrix index out of range");
        m.at(r, c) = cpx::asComplex(vm, a[2], "ComplexMatrix element");
        return vm.none();
    });
    if (name == "row") return bind("row", {"i"}, [self, self_m, idx](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& m = self_m(vm, self);
        std::size_t r = idx(vm, a[0]);
        if (r >= m.rows) throw KiritoError("row index out of range");
        RootScope rs(vm);
        auto list = std::make_unique<ListVal>();
        for (std::size_t c = 0; c < m.cols; ++c) list->elems.push_back(rs.add(cpx::make(vm, m.at(r, c))));
        return vm.alloc(std::move(list));
    });
    if (name == "transpose") return bind("transpose", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto& m = self_m(vm, self);
        auto t = cpx::makeMatrix(m.cols, m.rows);
        for (std::size_t r = 0; r < m.rows; ++r)
            for (std::size_t c = 0; c < m.cols; ++c) t->at(c, r) = m.at(r, c);
        return vm.alloc(std::move(t));
    });
    if (name == "conjugate") return bind("conjugate", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto& m = self_m(vm, self);
        auto out = cpx::makeMatrix(m.rows, m.cols);
        for (std::size_t i = 0; i < m.data.size(); ++i) out->data[i] = std::conj(m.data[i]);
        return vm.alloc(std::move(out));
    });
    // hermitian / conjugate transpose: (M*)^T
    if (name == "hermitian" || name == "conjugatetranspose") return bind("hermitian", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto& m = self_m(vm, self);
        auto t = cpx::makeMatrix(m.cols, m.rows);
        for (std::size_t r = 0; r < m.rows; ++r)
            for (std::size_t c = 0; c < m.cols; ++c) t->at(c, r) = std::conj(m.at(r, c));
        return vm.alloc(std::move(t));
    });
    if (name == "determinant") return bind("determinant", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) { return cpx::make(vm, cpx::determinant(self_m(vm, self))); });
    if (name == "inverse") return bind("inverse", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) { return vm.alloc(cpx::inverse(self_m(vm, self))); });
    if (name == "trace") return bind("trace", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto& m = self_m(vm, self);
        if (m.rows != m.cols) throw KiritoError("trace requires a square ComplexMatrix");
        cdouble s(0.0, 0.0);
        for (std::size_t i = 0; i < m.rows; ++i) s += m.at(i, i);
        return cpx::make(vm, s);
    });
    // --- vector operations (a ComplexMatrix with one dimension == 1 is a vector) ---
    if (name == "dot") return bind("dot", {"other"}, [self, self_m](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& m = self_m(vm, self);
        const auto* o = dynamic_cast<const ComplexMatrixVal*>(&vm.arena().deref(a[0]));
        if (!o) throw KiritoError("dot expects a ComplexMatrix vector");
        if (!m.isVector() || !o->isVector()) throw KiritoError("dot requires vectors (a 1×n or n×1 ComplexMatrix)");
        if (m.data.size() != o->data.size()) throw KiritoError("dot requires vectors of equal length");
        cdouble acc(0.0, 0.0);  // Hermitian inner product: sum conj(a_i)·b_i
        for (std::size_t i = 0; i < m.data.size(); ++i) acc += std::conj(m.data[i]) * o->data[i];
        return cpx::make(vm, acc);
    });
    if (name == "cross") return bind("cross", {"other"}, [self, self_m](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& m = self_m(vm, self);
        const auto* o = dynamic_cast<const ComplexMatrixVal*>(&vm.arena().deref(a[0]));
        if (!o) throw KiritoError("cross expects a ComplexMatrix vector");
        if (!m.isVector() || !o->isVector() || m.data.size() != 3 || o->data.size() != 3)
            throw KiritoError("cross is only defined for two 3-element vectors");
        const auto& u = m.data; const auto& v = o->data;
        auto r = cpx::makeMatrix(m.rows, m.cols);  // keep this vector's orientation
        r->data = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        return vm.alloc(std::move(r));
    });
    if (name == "norm") return bind("norm", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto& m = self_m(vm, self);  // Euclidean / Frobenius 2-norm: sqrt(sum |z_i|²) -> Float
        double acc = 0.0;
        for (const cdouble& z : m.data) acc += std::norm(z);
        return vm.makeFloat(std::sqrt(acc));
    });
    return Object::getAttr(vm, self, name);
}

// ------------------------------------------------------------------------------- the module
class ComplexModule : public NativeModule {
public:
    std::string name() const override { return "complex"; }

    void setup(ModuleBuilder& m) override {
        KiritoVM& vm = m.vm();

        // Constructors. Complex(re[, im]); of(re, im); real(re); the unit imaginary i.
        m.fn("Complex", {{"re", "Number"}, {"im", "", vm.makeInt(0)}}, "Complex", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "Complex");
            double re = args[0].asFloat("Complex re");
            double im = args.size() > 1 ? args[1].asFloat("Complex im") : 0.0;
            return vm.alloc(std::make_unique<ComplexVal>(re, im));
        });
        m.fn("of", {{"re", "Number"}, {"im", "Number"}}, "Complex", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "of");
            return vm.alloc(std::make_unique<ComplexVal>(args[0].asFloat("of re"), args[1].asFloat("of im")));
        });
        m.fn("real", {{"re", "Number"}}, "Complex", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return vm.alloc(std::make_unique<ComplexVal>(Args(vm, a, "real")[0].asFloat("real"), 0.0));
        });
        // polar(r, theta) -> r·(cos θ + i sin θ); rect == Complex.
        m.fn("polar", {{"r", "Number"}, {"theta", "Number"}}, "Complex", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "polar");
            return cpx::make(vm, std::polar(args[0].asFloat("polar r"), args[1].asFloat("polar theta")));
        });
        m.value("zero", cpx::make(vm, cdouble(0.0, 0.0)));
        m.value("one", cpx::make(vm, cdouble(1.0, 0.0)));
        m.value("i", cpx::make(vm, cdouble(0.0, 1.0)));
        m.value("pi", val(vm, std::numbers::pi));
        m.value("e", val(vm, std::numbers::e));
        m.value("tau", val(vm, 2.0 * std::numbers::pi));

        // Scalar reductions: modulus/abs, phase/argument, norm2, conjugate.
        m.fn("modulus", {{"z"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle { return val(vm, std::abs(cpx::asComplex(vm, a[0]))); });
        m.fn("abs", {{"z"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle { return val(vm, std::abs(cpx::asComplex(vm, a[0]))); });
        m.fn("phase", {{"z"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle { return val(vm, std::arg(cpx::asComplex(vm, a[0]))); });
        m.fn("argument", {{"z"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle { return val(vm, std::arg(cpx::asComplex(vm, a[0]))); });
        m.fn("norm2", {{"z"}}, "Float", [](KiritoVM& vm, std::span<const Handle> a) -> Handle { return val(vm, std::norm(cpx::asComplex(vm, a[0]))); });
        m.fn("conjugate", {{"z"}}, "Complex", [](KiritoVM& vm, std::span<const Handle> a) -> Handle { return cpx::make(vm, std::conj(cpx::asComplex(vm, a[0]))); });

        // Analytic functions — the complex extensions of the `math` module's transcendental set.
        // Each accepts a Complex or a real number and returns a Complex.
        auto unary = [&](const char* nm, cdouble (*f)(const cdouble&)) {
            m.fn(nm, {{"z"}}, "Complex", [f](KiritoVM& vm, std::span<const Handle> a) -> Handle {
                return cpx::make(vm, f(cpx::asComplex(vm, a[0])));
            });
        };
        unary("exp", [](const cdouble& z) { return std::exp(z); });
        unary("log", [](const cdouble& z) { return std::log(z); });
        unary("log10", [](const cdouble& z) { return std::log10(z); });
        unary("sqrt", [](const cdouble& z) { return std::sqrt(z); });
        unary("sin", [](const cdouble& z) { return std::sin(z); });
        unary("cos", [](const cdouble& z) { return std::cos(z); });
        unary("tan", [](const cdouble& z) { return std::tan(z); });
        unary("asin", [](const cdouble& z) { return std::asin(z); });
        unary("acos", [](const cdouble& z) { return std::acos(z); });
        unary("atan", [](const cdouble& z) { return std::atan(z); });
        unary("sinh", [](const cdouble& z) { return std::sinh(z); });
        unary("cosh", [](const cdouble& z) { return std::cosh(z); });
        unary("tanh", [](const cdouble& z) { return std::tanh(z); });
        unary("asinh", [](const cdouble& z) { return std::asinh(z); });
        unary("acosh", [](const cdouble& z) { return std::acosh(z); });
        unary("atanh", [](const cdouble& z) { return std::atanh(z); });
        // pow(z, w) and a cube root (no std::cbrt for complex; use the principal value).
        m.fn("pow", {{"z"}, {"w"}}, "Complex", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return cpx::make(vm, std::pow(cpx::asComplex(vm, a[0]), cpx::asComplex(vm, a[1])));
        });
        m.fn("cbrt", {{"z"}}, "Complex", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            return cpx::make(vm, std::pow(cpx::asComplex(vm, a[0]), cdouble(1.0 / 3.0, 0.0)));
        });

        // -------- complex matrices --------
        // Matrix(nested-list) — elements may be Complex, Integer or Float (reals go on the real axis).
        m.fn("Matrix", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "Matrix");
            if (args.size() != 1) throw KiritoError("Matrix expects a nested list of rows");
            std::vector<std::vector<cdouble>> grid;
            for (Value row : args[0].items()) {
                std::vector<cdouble> r;
                for (Value cell : row.items()) r.push_back(cpx::asComplex(vm, cell.handle(), "Matrix element"));
                grid.push_back(std::move(r));
            }
            std::size_t r = grid.size(), c = r ? grid[0].size() : 0;
            for (const auto& row : grid)
                if (row.size() != c) throw KiritoError("Matrix rows must have equal length");
            auto mtx = cpx::makeMatrix(r, c);
            for (std::size_t i = 0; i < r; ++i)
                for (std::size_t j = 0; j < c; ++j) mtx->at(i, j) = grid[i][j];
            return vm.alloc(std::move(mtx));
        });
        m.fn("zeros", {{"rows", "Integer"}, {"cols", "Integer"}}, "ComplexMatrix", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "zeros");
            return vm.alloc(cpx::makeMatrix(static_cast<std::size_t>(args[0].asInt("rows")), static_cast<std::size_t>(args[1].asInt("cols"))));
        });
        m.fn("ones", {{"rows", "Integer"}, {"cols", "Integer"}}, "ComplexMatrix", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "ones");
            return vm.alloc(cpx::makeMatrix(static_cast<std::size_t>(args[0].asInt("rows")), static_cast<std::size_t>(args[1].asInt("cols")), cdouble(1.0, 0.0)));
        });
        m.fn("identity", {{"n", "Integer"}}, "ComplexMatrix", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::size_t n = static_cast<std::size_t>(Args(vm, a, "identity")[0].asInt("n"));
            auto mtx = cpx::makeMatrix(n, n);
            for (std::size_t i = 0; i < n; ++i) mtx->at(i, i) = cdouble(1.0, 0.0);
            return vm.alloc(std::move(mtx));
        });
        // vector(list) -> a 1×n row-vector ComplexMatrix (cells may be Complex or numbers).
        m.fn("vector", {{"values", "List"}}, "ComplexMatrix", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::vector<cdouble> xs;
            for (Value e : Args(vm, a, "vector")[0].items()) xs.push_back(cpx::asComplex(vm, e.handle(), "vector element"));
            auto mtx = cpx::makeMatrix(1, xs.size());
            mtx->data = std::move(xs);
            return vm.alloc(std::move(mtx));
        });
    }
};

}  // namespace kirito

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
#endif
