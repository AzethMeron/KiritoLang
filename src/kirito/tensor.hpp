#ifndef KIRITO_TENSOR_HPP
#define KIRITO_TENSOR_HPP

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// A small, dependency-free N-dimensional tensor engine. It is intentionally separate from anything
// Kirito-specific (no VM, no handles) so it is reusable, unit-testable on its own, and so the real
// `matrix` / `complex` matrix types and the Kirito `tensor` module can all be built on top of one
// implementation of the core operations.
//
// Element type `T` is a template parameter, so the same engine serves Float (double), Complex
// (std::complex<double>) or any other arithmetic-like type. Storage is a single contiguous,
// row-major (C-order) buffer. That single-buffer design is deliberate: it is CPU-only today, but a
// future device backend can replace `data` with a GPU buffer and swap these element loops for
// kernels without changing the public shape/stride contract. There is no autograd — this is a plain
// numeric container.

namespace kirito::tensor {

using Shape = std::vector<std::size_t>;

// Errors are a distinct type so the Kirito module layer can translate them into KiritoError without
// the engine depending on Kirito's error type.
struct TensorError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline std::size_t numel(const Shape& s) {
    std::size_t n = 1;
    for (std::size_t d : s) n *= d;
    return n;
}

// Row-major (C-order) strides for a shape: the step, in elements, to move one index along each axis.
inline Shape rowMajorStrides(const Shape& shape) {
    Shape st(shape.size());
    std::size_t acc = 1;
    for (std::size_t i = shape.size(); i-- > 0;) {
        st[i] = acc;
        acc *= shape[i];
    }
    return st;
}

// NumPy broadcasting: align shapes from the right; each axis must be equal or one of them 1.
inline Shape broadcastShapes(const Shape& a, const Shape& b) {
    std::size_t n = std::max(a.size(), b.size());
    Shape out(n);
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t da = i + a.size() < n ? 1 : a[i + a.size() - n];
        std::size_t db = i + b.size() < n ? 1 : b[i + b.size() - n];
        if (da == db || da == 1 || db == 1) out[i] = std::max(da, db);
        else throw TensorError("tensors are not broadcastable to a common shape");
    }
    return out;
}

template <class T>
class Tensor {
public:
    Shape shape;
    std::vector<T> data;  // contiguous, row-major (a future device backend would own this buffer)

    Tensor() = default;
    explicit Tensor(Shape s, T fill = T{}) : shape(std::move(s)), data(numel(shape), fill) {}
    Tensor(Shape s, std::vector<T> d) : shape(std::move(s)), data(std::move(d)) {
        if (data.size() != numel(shape)) throw TensorError("tensor data size does not match its shape");
    }

    std::size_t ndim() const { return shape.size(); }
    std::size_t size() const { return data.size(); }
    Shape strides() const { return rowMajorStrides(shape); }
    bool isScalar() const { return shape.empty(); }

    std::size_t flatIndex(const Shape& idx) const {
        if (idx.size() != shape.size()) throw TensorError("wrong number of indices for this tensor");
        Shape st = strides();
        std::size_t off = 0;
        for (std::size_t i = 0; i < idx.size(); ++i) {
            if (idx[i] >= shape[i]) throw TensorError("tensor index out of range");
            off += idx[i] * st[i];
        }
        return off;
    }
    T& at(const Shape& idx) { return data[flatIndex(idx)]; }
    T at(const Shape& idx) const { return data[flatIndex(idx)]; }
};

// ---- shape ops -------------------------------------------------------------------------------

template <class T>
Tensor<T> reshape(const Tensor<T>& t, const Shape& newshape) {
    if (numel(newshape) != t.size()) throw TensorError("reshape: total number of elements must be unchanged");
    return Tensor<T>(newshape, t.data);
}

template <class T>
Tensor<T> flatten(const Tensor<T>& t) {
    return Tensor<T>(Shape{t.size()}, t.data);
}

// Reorder axes. `axes` is a permutation of 0..ndim-1.
template <class T>
Tensor<T> permute(const Tensor<T>& t, const Shape& axes) {
    if (axes.size() != t.ndim()) throw TensorError("permute: axes count must equal the tensor rank");
    std::vector<bool> seen(t.ndim(), false);
    Shape newshape(t.ndim());
    for (std::size_t i = 0; i < axes.size(); ++i) {
        if (axes[i] >= t.ndim() || seen[axes[i]]) throw TensorError("permute: axes must be a permutation");
        seen[axes[i]] = true;
        newshape[i] = t.shape[axes[i]];
    }
    Tensor<T> out(newshape);
    Shape oldst = t.strides(), nst = out.strides();
    for (std::size_t lin = 0; lin < out.size(); ++lin) {
        std::size_t rem = lin, oldoff = 0;
        for (std::size_t i = 0; i < out.ndim(); ++i) {
            std::size_t coord = rem / nst[i];
            rem %= nst[i];
            oldoff += coord * oldst[axes[i]];
        }
        out.data[lin] = t.data[oldoff];
    }
    return out;
}

// Transpose = reverse every axis (the matrix transpose when the tensor is 2-D).
template <class T>
Tensor<T> transpose(const Tensor<T>& t) {
    Shape axes(t.ndim());
    for (std::size_t i = 0; i < t.ndim(); ++i) axes[i] = t.ndim() - 1 - i;
    return permute(t, axes);
}

// ---- elementwise -----------------------------------------------------------------------------

template <class T, class Fn>
Tensor<T> mapUnary(const Tensor<T>& t, Fn fn) {
    Tensor<T> out(t.shape);
    for (std::size_t i = 0; i < t.size(); ++i) out.data[i] = fn(t.data[i]);
    return out;
}

// Binary elementwise with NumPy broadcasting.
template <class T, class Op>
Tensor<T> elementwise(const Tensor<T>& a, const Tensor<T>& b, Op op) {
    Shape outshape = broadcastShapes(a.shape, b.shape);
    Tensor<T> out(outshape);
    Shape ost = out.strides(), ast = a.strides(), bst = b.strides();
    std::size_t apad = outshape.size() - a.ndim(), bpad = outshape.size() - b.ndim();
    for (std::size_t lin = 0; lin < out.size(); ++lin) {
        std::size_t rem = lin, aoff = 0, boff = 0;
        for (std::size_t i = 0; i < outshape.size(); ++i) {
            std::size_t coord = rem / ost[i];
            rem %= ost[i];
            if (i >= apad) { std::size_t ai = i - apad; aoff += (a.shape[ai] == 1 ? 0 : coord) * ast[ai]; }
            if (i >= bpad) { std::size_t bi = i - bpad; boff += (b.shape[bi] == 1 ? 0 : coord) * bst[bi]; }
        }
        out.data[lin] = op(a.data[aoff], b.data[boff]);
    }
    return out;
}

template <class T> Tensor<T> add(const Tensor<T>& a, const Tensor<T>& b) { return elementwise(a, b, [](T x, T y) { return x + y; }); }
template <class T> Tensor<T> sub(const Tensor<T>& a, const Tensor<T>& b) { return elementwise(a, b, [](T x, T y) { return x - y; }); }
template <class T> Tensor<T> mul(const Tensor<T>& a, const Tensor<T>& b) { return elementwise(a, b, [](T x, T y) { return x * y; }); }
template <class T> Tensor<T> div(const Tensor<T>& a, const Tensor<T>& b) {
    return elementwise(a, b, [](T x, T y) {
        if (y == T{}) throw TensorError("tensor division by zero");
        return x / y;
    });
}
template <class T> Tensor<T> scalarOp(const Tensor<T>& a, T s, char op) {
    return mapUnary(a, [s, op](T x) -> T {
        switch (op) {
            case '+': return x + s;
            case '-': return x - s;
            case '*': return x * s;
            case '/': if (s == T{}) throw TensorError("tensor division by zero"); return x / s;
        }
        return x;
    });
}
template <class T> Tensor<T> negate(const Tensor<T>& a) { return mapUnary(a, [](T x) { return -x; }); }

// ---- matmul (2-D, and N-D batched over the leading dims) --------------------------------------

template <class T>
Tensor<T> matmul(const Tensor<T>& a, const Tensor<T>& b) {
    if (a.ndim() < 2 || b.ndim() < 2) throw TensorError("matmul requires tensors of rank >= 2");
    std::size_t m = a.shape[a.ndim() - 2], k = a.shape[a.ndim() - 1];
    std::size_t k2 = b.shape[b.ndim() - 2], n = b.shape[b.ndim() - 1];
    if (k != k2) throw TensorError("matmul: inner dimensions differ");
    Shape abatch(a.shape.begin(), a.shape.end() - 2), bbatch(b.shape.begin(), b.shape.end() - 2);
    Shape batch = broadcastShapes(abatch, bbatch);
    Shape outshape = batch;
    outshape.push_back(m);
    outshape.push_back(n);
    Tensor<T> out(outshape);
    std::size_t batchN = numel(batch);
    std::size_t aMat = m * k, bMat = k * n, oMat = m * n;
    Shape bst = rowMajorStrides(batch), ast = rowMajorStrides(abatch), bbst = rowMajorStrides(bbatch);
    for (std::size_t bi = 0; bi < batchN; ++bi) {
        // map the broadcast batch index back to each operand's batch offset
        std::size_t rem = bi, aBatchOff = 0, bBatchOff = 0;
        for (std::size_t i = 0; i < batch.size(); ++i) {
            std::size_t coord = rem / bst[i];
            rem %= bst[i];
            std::size_t apad = batch.size() - abatch.size(), bpad = batch.size() - bbatch.size();
            if (i >= apad) { std::size_t ai = i - apad; aBatchOff += (abatch[ai] == 1 ? 0 : coord) * ast[ai]; }
            if (i >= bpad) { std::size_t bj = i - bpad; bBatchOff += (bbatch[bj] == 1 ? 0 : coord) * bbst[bj]; }
        }
        const T* A = a.data.data() + aBatchOff * aMat;
        const T* B = b.data.data() + bBatchOff * bMat;
        T* O = out.data.data() + bi * oMat;
        for (std::size_t i = 0; i < m; ++i)
            for (std::size_t p = 0; p < k; ++p) {
                T v = A[i * k + p];
                for (std::size_t j = 0; j < n; ++j) O[i * n + j] += v * B[p * n + j];
            }
    }
    return out;
}

// Dot product of two 1-D tensors of equal length (a scalar).
template <class T>
T dot(const Tensor<T>& a, const Tensor<T>& b) {
    if (a.ndim() != 1 || b.ndim() != 1) throw TensorError("dot requires two 1-D tensors");
    if (a.size() != b.size()) throw TensorError("dot requires vectors of equal length");
    T acc = T{};
    for (std::size_t i = 0; i < a.size(); ++i) acc += a.data[i] * b.data[i];
    return acc;
}

// ---- reductions ------------------------------------------------------------------------------

template <class T> T sumAll(const Tensor<T>& t) { T s = T{}; for (const T& x : t.data) s += x; return s; }
template <class T> T prodAll(const Tensor<T>& t) { T p = T{1}; for (const T& x : t.data) p *= x; return p; }

template <class T> T minAll(const Tensor<T>& t) {
    if constexpr (std::is_arithmetic_v<T>) {
        if (t.data.empty()) throw TensorError("min of an empty tensor");
        T m = t.data[0];
        for (const T& x : t.data) if (x < m) m = x;
        return m;
    } else {
        throw TensorError("min is not defined for this dtype (it is not ordered)");
    }
}
template <class T> T maxAll(const Tensor<T>& t) {
    if constexpr (std::is_arithmetic_v<T>) {
        if (t.data.empty()) throw TensorError("max of an empty tensor");
        T m = t.data[0];
        for (const T& x : t.data) if (m < x) m = x;
        return m;
    } else {
        throw TensorError("max is not defined for this dtype (it is not ordered)");
    }
}

// Reduce one axis with a combiner `comb(acc, x)`, seeded with the first element along the axis.
template <class T, class Comb>
Tensor<T> reduceAxis(const Tensor<T>& t, std::size_t axis, Comb comb) {
    if (axis >= t.ndim()) throw TensorError("reduction axis out of range");
    Shape outshape;
    for (std::size_t i = 0; i < t.ndim(); ++i) if (i != axis) outshape.push_back(t.shape[i]);
    Tensor<T> out(outshape);
    Shape st = t.strides();
    std::size_t axislen = t.shape[axis], axisstep = st[axis];
    Shape ost = out.strides();
    // For each output cell, walk the reduced axis in the source.
    for (std::size_t olin = 0; olin < out.size(); ++olin) {
        // unravel olin over outshape, then build the source base offset (axis index 0)
        std::size_t rem = olin, base = 0, oi = 0;
        for (std::size_t i = 0; i < t.ndim(); ++i) {
            if (i == axis) continue;
            std::size_t coord = ost.empty() ? 0 : rem / ost[oi];
            if (!ost.empty()) rem %= ost[oi];
            base += coord * st[i];
            ++oi;
        }
        T acc = t.data[base];
        for (std::size_t a = 1; a < axislen; ++a) acc = comb(acc, t.data[base + a * axisstep]);
        out.data[olin] = acc;
    }
    return out;
}

// ---- 2-D linear algebra (square matrices) ----------------------------------------------------
// Pivot selection uses std::abs(T): a double for real, a magnitude for complex — so the same code
// is numerically sound for both. These back the `matrix` and `complex` matrix types.

template <class T>
T trace(const Tensor<T>& t) {
    if (t.ndim() != 2 || t.shape[0] != t.shape[1]) throw TensorError("trace requires a square 2-D tensor");
    std::size_t n = t.shape[0];
    T s = T{};
    for (std::size_t i = 0; i < n; ++i) s += t.data[i * n + i];
    return s;
}

// Determinant via Gaussian elimination with partial pivoting.
template <class T>
T determinant(const Tensor<T>& t) {
    if (t.ndim() != 2 || t.shape[0] != t.shape[1]) throw TensorError("determinant requires a square 2-D tensor");
    std::size_t n = t.shape[0];
    std::vector<T> a = t.data;
    T det = T{1};
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t piv = k;
        for (std::size_t i = k + 1; i < n; ++i)
            if (std::abs(a[i * n + k]) > std::abs(a[piv * n + k])) piv = i;
        if (std::abs(a[piv * n + k]) < 1e-15) return T{};
        if (piv != k) {
            for (std::size_t j = 0; j < n; ++j) std::swap(a[k * n + j], a[piv * n + j]);
            det = -det;
        }
        det = det * a[k * n + k];
        for (std::size_t i = k + 1; i < n; ++i) {
            T f = a[i * n + k] / a[k * n + k];
            for (std::size_t j = k; j < n; ++j) a[i * n + j] = a[i * n + j] - f * a[k * n + j];
        }
    }
    return det;
}

// Inverse via Gauss-Jordan elimination on [A | I] — O(n^3), the fast method.
template <class T>
Tensor<T> inverse(const Tensor<T>& t) {
    if (t.ndim() != 2 || t.shape[0] != t.shape[1]) throw TensorError("inverse requires a square 2-D tensor");
    std::size_t n = t.shape[0];
    std::vector<T> a = t.data;
    Tensor<T> inv(Shape{n, n});
    for (std::size_t i = 0; i < n; ++i) inv.data[i * n + i] = T{1};
    for (std::size_t k = 0; k < n; ++k) {
        std::size_t piv = k;
        for (std::size_t i = k + 1; i < n; ++i)
            if (std::abs(a[i * n + k]) > std::abs(a[piv * n + k])) piv = i;
        if (std::abs(a[piv * n + k]) < 1e-15) throw TensorError("matrix is singular (no inverse)");
        if (piv != k)
            for (std::size_t j = 0; j < n; ++j) {
                std::swap(a[k * n + j], a[piv * n + j]);
                std::swap(inv.data[k * n + j], inv.data[piv * n + j]);
            }
        T d = a[k * n + k];
        for (std::size_t j = 0; j < n; ++j) { a[k * n + j] = a[k * n + j] / d; inv.data[k * n + j] = inv.data[k * n + j] / d; }
        for (std::size_t i = 0; i < n; ++i) {
            if (i == k) continue;
            T f = a[i * n + k];
            for (std::size_t j = 0; j < n; ++j) {
                a[i * n + j] = a[i * n + j] - f * a[k * n + j];
                inv.data[i * n + j] = inv.data[i * n + j] - f * inv.data[k * n + j];
            }
        }
    }
    return inv;
}

}  // namespace kirito::tensor

#endif
