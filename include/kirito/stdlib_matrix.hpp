#ifndef KIRITO_STDLIB_MATRIX_HPP
#define KIRITO_STDLIB_MATRIX_HPP

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "builtins.hpp"
#include "collections.hpp"
#include "native.hpp"

namespace kirito {

// A dense real-valued matrix (row-major). Element type is double; there are no complex numbers.
class MatrixVal : public NativeClass<MatrixVal> {
public:
    static constexpr const char* kTypeName = "Matrix";
    std::size_t rows = 0, cols = 0;
    std::vector<double> data;

    MatrixVal() = default;
    MatrixVal(std::size_t r, std::size_t c, double fill = 0.0) : rows(r), cols(c), data(r * c, fill) {}

    double& at(std::size_t r, std::size_t c) { return data[r * cols + c]; }
    double at(std::size_t r, std::size_t c) const { return data[r * cols + c]; }

    std::string str(StringifyCtx&) const override {
        std::string s = "[";
        for (std::size_t r = 0; r < rows; ++r) {
            if (r) s += ", ";
            s += "[";
            for (std::size_t c = 0; c < cols; ++c) {
                if (c) s += ", ";
                s += floatToString(at(r, c));
            }
            s += "]";
        }
        return s + "]";
    }
    bool equals(const ObjectArena&, const Object& other) const override {
        if (other.kind() != ValueKind::Instance) return false;
        const auto* m = dynamic_cast<const MatrixVal*>(&other);
        if (!m || m->rows != rows || m->cols != cols) return false;
        for (std::size_t i = 0; i < data.size(); ++i)
            if (std::fabs(data[i] - m->data[i]) > 1e-9) return false;
        return true;
    }

    std::vector<std::string> inspectMembers() const override {
        return {
            "rows() -> Integer", "cols() -> Integer", "shape() -> List",
            "get(row, col) -> Float", "set(row, col, value)", "row(i) -> List",
            "transpose() -> Matrix", "determinant() -> Float", "inverse() -> Matrix",
            "trace() -> Float", "sum() -> Float", "apply(fn) -> Matrix",
        };
    }

    Handle binary(KiritoVM& vm, BinOp op, Handle self, Handle rhs) override;
    Handle getAttr(KiritoVM& vm, Handle self, std::string_view name) override;
    // m[i] -> a row (List); m[i, j] -> an element; m[i, j] = v -> set element.
    Handle getItem(KiritoVM& vm, std::span<const Handle> keys) override;
    void setItem(KiritoVM& vm, std::span<const Handle> keys, Handle value) override;

    static std::size_t indexOf(KiritoVM& vm, Handle h) {
        const Object& o = vm.arena().deref(h);
        if (o.kind() != ValueKind::Integer) throw KiritoError("Matrix index must be Integer");
        int64_t v = static_cast<const IntVal&>(o).value();
        if (v < 0) throw KiritoError("Matrix index out of range");
        return static_cast<std::size_t>(v);
    }
};

namespace mat {

inline double numOf(KiritoVM& vm, Handle h) {
    const Object& o = vm.arena().deref(h);
    if (o.kind() == ValueKind::Integer) return static_cast<double>(static_cast<const IntVal&>(o).value());
    if (o.kind() == ValueKind::Float) return static_cast<const FloatVal&>(o).value();
    throw KiritoError("Matrix expects numbers");
}

// Cap total element count so a hostile/absurd dimension raises a catchable error instead of an
// uncaught std::bad_alloc (which would terminate the VM). ~16M doubles = 128 MB is plenty for CPU.
inline constexpr std::size_t kMaxMatrixElems = 16ull * 1024 * 1024;
inline std::unique_ptr<MatrixVal> make(std::size_t r, std::size_t c, double fill = 0.0) {
    if (c != 0 && r > kMaxMatrixElems / c) throw KiritoError("Matrix too large");
    return std::make_unique<MatrixVal>(r, c, fill);
}

// Determinant via Gaussian elimination with partial pivoting (works on a copy).
inline double determinant(const MatrixVal& m) {
    if (m.rows != m.cols) throw KiritoError("determinant requires a square Matrix");
    std::size_t n = m.rows;
    std::vector<double> a = m.data;
    double det = 1.0;
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t piv = k;
        for (std::size_t i = k + 1; i < n; ++i)
            if (std::fabs(a[i * n + k]) > std::fabs(a[piv * n + k])) piv = i;
        if (std::fabs(a[piv * n + k]) < 1e-15) return 0.0;
        if (piv != k) { for (std::size_t j = 0; j < n; ++j) std::swap(a[k * n + j], a[piv * n + j]); det = -det; }
        det *= a[k * n + k];
        for (std::size_t i = k + 1; i < n; ++i) {
            double f = a[i * n + k] / a[k * n + k];
            for (std::size_t j = k; j < n; ++j) a[i * n + j] -= f * a[k * n + j];
        }
    }
    return det;
}

// Inverse via Gauss-Jordan on [A | I].
inline std::unique_ptr<MatrixVal> inverse(const MatrixVal& m) {
    if (m.rows != m.cols) throw KiritoError("inverse requires a square Matrix");
    std::size_t n = m.rows;
    std::vector<double> a = m.data;
    auto inv = make(n, n);
    for (std::size_t i = 0; i < n; ++i) inv->at(i, i) = 1.0;
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t piv = k;
        for (std::size_t i = k + 1; i < n; ++i)
            if (std::fabs(a[i * n + k]) > std::fabs(a[piv * n + k])) piv = i;
        if (std::fabs(a[piv * n + k]) < 1e-15) throw KiritoError("Matrix is singular (no inverse)");
        if (piv != k)
            for (std::size_t j = 0; j < n; ++j) {
                std::swap(a[k * n + j], a[piv * n + j]);
                std::swap(inv->at(k, j), inv->at(piv, j));
            }
        double d = a[k * n + k];
        for (std::size_t j = 0; j < n; ++j) { a[k * n + j] /= d; inv->at(k, j) /= d; }
        for (std::size_t i = 0; i < n; ++i) {
            if (i == k) continue;
            double f = a[i * n + k];
            for (std::size_t j = 0; j < n; ++j) { a[i * n + j] -= f * a[k * n + j]; inv->at(i, j) -= f * inv->at(k, j); }
        }
    }
    return inv;
}

}  // namespace mat

inline Handle MatrixVal::binary(KiritoVM& vm, BinOp op, Handle, Handle rhs) {
    const Object& b = vm.arena().deref(rhs);
    const auto* other = dynamic_cast<const MatrixVal*>(&b);
    if (op == BinOp::Add || op == BinOp::Sub) {
        if (!other || other->rows != rows || other->cols != cols)
            throw KiritoError("Matrix +/- requires Matrices of equal shape");
        auto r = mat::make(rows, cols);
        for (std::size_t i = 0; i < data.size(); ++i)
            r->data[i] = op == BinOp::Add ? data[i] + other->data[i] : data[i] - other->data[i];
        return vm.alloc(std::move(r));
    }
    if (op == BinOp::Mul) {
        if (other) {  // matrix multiply
            if (cols != other->rows) throw KiritoError("Matrix multiply: inner dimensions differ");
            auto r = mat::make(rows, other->cols);
            for (std::size_t i = 0; i < rows; ++i)
                for (std::size_t k = 0; k < cols; ++k) {
                    double v = at(i, k);
                    for (std::size_t j = 0; j < other->cols; ++j) r->at(i, j) += v * other->at(k, j);
                }
            return vm.alloc(std::move(r));
        }
        double s = mat::numOf(vm, rhs);  // scalar
        auto r = mat::make(rows, cols);
        for (std::size_t i = 0; i < data.size(); ++i) r->data[i] = data[i] * s;
        return vm.alloc(std::move(r));
    }
    throw KiritoError("Matrix does not support this operator");
}

inline Handle MatrixVal::getItem(KiritoVM& vm, std::span<const Handle> keys) {
    if (keys.size() == 1) {  // a whole row, as a List
        std::size_t r = indexOf(vm, keys[0]);
        if (r >= rows) throw KiritoError("Matrix row index out of range");
        RootScope rs(vm);
        auto list = std::make_unique<ListVal>();
        for (std::size_t c = 0; c < cols; ++c) list->elems.push_back(rs.add(vm.makeFloat(at(r, c))));
        return vm.alloc(std::move(list));
    }
    if (keys.size() == 2) {  // a single element
        std::size_t r = indexOf(vm, keys[0]), c = indexOf(vm, keys[1]);
        if (r >= rows || c >= cols) throw KiritoError("Matrix index out of range");
        return vm.makeFloat(at(r, c));
    }
    throw KiritoError("Matrix index needs 1 (row) or 2 (element) indices");
}

inline void MatrixVal::setItem(KiritoVM& vm, std::span<const Handle> keys, Handle value) {
    if (keys.size() != 2) throw KiritoError("Matrix element assignment needs two indices: m[i, j] = v");
    std::size_t r = indexOf(vm, keys[0]), c = indexOf(vm, keys[1]);
    if (r >= rows || c >= cols) throw KiritoError("Matrix index out of range");
    at(r, c) = mat::numOf(vm, value);
}

inline Handle MatrixVal::getAttr(KiritoVM& vm, Handle self, std::string_view name) {
    auto bind = [&](const char* nm, std::vector<std::string> params, NativeFn fn) {
        return makeMethod(vm, nm, std::move(params), std::move(fn), std::vector<Handle>{self});
    };
    auto self_m = [](KiritoVM& vm, Handle self) -> MatrixVal& {
        return static_cast<MatrixVal&>(vm.arena().deref(self));
    };
    auto idx = [](KiritoVM& vm, Handle h) -> std::size_t {
        const Object& o = vm.arena().deref(h);
        if (o.kind() != ValueKind::Integer) throw KiritoError("Matrix index must be Integer");
        int64_t v = static_cast<const IntVal&>(o).value();
        if (v < 0) throw KiritoError("Matrix index out of range");
        return static_cast<std::size_t>(v);
    };
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
        if (r >= m.rows || c >= m.cols) throw KiritoError("Matrix index out of range");
        return vm.makeFloat(m.at(r, c));
    });
    if (name == "set") return bind("set", {"row", "col", "value"}, [self, self_m, idx](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& m = self_m(vm, self);
        std::size_t r = idx(vm, a[0]), c = idx(vm, a[1]);
        if (r >= m.rows || c >= m.cols) throw KiritoError("Matrix index out of range");
        m.at(r, c) = mat::numOf(vm, a[2]);
        return vm.none();
    });
    if (name == "transpose") return bind("transpose", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto& m = self_m(vm, self);
        auto t = mat::make(m.cols, m.rows);
        for (std::size_t r = 0; r < m.rows; ++r)
            for (std::size_t c = 0; c < m.cols; ++c) t->at(c, r) = m.at(r, c);
        return vm.alloc(std::move(t));
    });
    if (name == "determinant") return bind("determinant", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) { return vm.makeFloat(mat::determinant(self_m(vm, self))); });
    if (name == "inverse") return bind("inverse", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) { return vm.alloc(mat::inverse(self_m(vm, self))); });
    if (name == "sum") return bind("sum", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) -> Handle {
        double s = 0; for (double v : self_m(vm, self).data) s += v; return vm.makeFloat(s);
    });
    if (name == "trace") return bind("trace", {}, [self, self_m](KiritoVM& vm, std::span<const Handle>) -> Handle {
        auto& m = self_m(vm, self);
        if (m.rows != m.cols) throw KiritoError("trace requires a square Matrix");
        double s = 0; for (std::size_t i = 0; i < m.rows; ++i) s += m.at(i, i); return vm.makeFloat(s);
    });
    if (name == "apply") return bind("apply", {"fn"}, [self, self_m](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        Handle fn = a[0];
        auto& m = self_m(vm, self);
        auto out = mat::make(m.rows, m.cols);
        for (std::size_t i = 0; i < m.data.size(); ++i) {
            std::array<Handle, 1> args{vm.makeFloat(m.data[i])};
            out->data[i] = mat::numOf(vm, vm.arena().deref(fn).call(vm, args));
        }
        return vm.alloc(std::move(out));
    });
    if (name == "row") return bind("row", {"i"}, [self, self_m, idx](KiritoVM& vm, std::span<const Handle> a) -> Handle {
        auto& m = self_m(vm, self);
        std::size_t r = idx(vm, a[0]);
        if (r >= m.rows) throw KiritoError("row index out of range");
        RootScope rs(vm);
        auto list = std::make_unique<ListVal>();
        for (std::size_t c = 0; c < m.cols; ++c) list->elems.push_back(rs.add(vm.makeFloat(m.at(r, c))));
        return vm.alloc(std::move(list));
    });
    return Object::getAttr(vm, self, name);
}

class MatrixModule : public NativeModule {
public:
    std::string name() const override { return "matrix"; }
    void setup(ModuleBuilder& m) override {
        // Matrix(nested-list) or Matrix(rows, cols)
        m.fn("Matrix", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "Matrix");
            if (args.size() == 2) {
                int64_t r = args[0].asInt("Matrix rows");
                int64_t c = args[1].asInt("Matrix cols");
                if (r < 0 || c < 0) throw KiritoError("Matrix dimensions must be non-negative");
                return vm.alloc(mat::make(static_cast<std::size_t>(r), static_cast<std::size_t>(c)));
            }
            if (args.size() != 1) throw KiritoError("Matrix expects a nested list or (rows, cols)");
            std::vector<std::vector<double>> grid;
            for (Value row : args[0].items()) {
                std::vector<double> r;
                for (Value cell : row.items()) r.push_back(cell.asFloat("Matrix element"));
                grid.push_back(std::move(r));
            }
            std::size_t r = grid.size(), c = r ? grid[0].size() : 0;
            for (const auto& row : grid)
                if (row.size() != c) throw KiritoError("Matrix rows must have equal length");
            auto mtx = mat::make(r, c);
            for (std::size_t i = 0; i < r; ++i)
                for (std::size_t j = 0; j < c; ++j) mtx->at(i, j) = grid[i][j];
            return vm.alloc(std::move(mtx));
        });
        m.fn("zeros", {{"rows", "Integer"}, {"cols", "Integer"}}, "Matrix", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "zeros");
            return vm.alloc(mat::make(static_cast<std::size_t>(args[0].asInt("rows")), static_cast<std::size_t>(args[1].asInt("cols")), 0.0));
        });
        m.fn("ones", {{"rows", "Integer"}, {"cols", "Integer"}}, "Matrix", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            Args args(vm, a, "ones");
            return vm.alloc(mat::make(static_cast<std::size_t>(args[0].asInt("rows")), static_cast<std::size_t>(args[1].asInt("cols")), 1.0));
        });
        m.fn("identity", {{"n", "Integer"}}, "Matrix", [](KiritoVM& vm, std::span<const Handle> a) -> Handle {
            std::size_t n = static_cast<std::size_t>(Args(vm, a, "identity")[0].asInt("n"));
            auto mtx = mat::make(n, n);
            for (std::size_t i = 0; i < mtx->rows; ++i) mtx->at(i, i) = 1.0;
            return vm.alloc(std::move(mtx));
        });
    }
};

}  // namespace kirito

#endif
