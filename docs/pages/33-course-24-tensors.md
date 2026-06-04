# Lesson 24 — Tensors

A **tensor** is a dense array of any number of dimensions — the generalization of a matrix beyond
2-D. The `tensor` module is the engine the `matrix` and `complex` matrix types are built on: a 2-D
tensor *is* a matrix. It's CPU-only but ships with all the usual array operations, NumPy-style
broadcasting, **general contraction** (`tensordot`), and a **reverse-mode autograd** for
differentiation (covered at the end of this lesson).

## Creating tensors

Build one from a (rectangular) nested list — the nesting depth sets the **rank** — or with a factory:

```kirito
var io = import("io")
var T = import("tensor")

var a = T.Tensor([[1, 2, 3], [4, 5, 6]])      # a 2×3 tensor
io.print(a.shape(), a.ndim(), a.dtype())      # [2, 3] 2 Float
io.print(T.zeros([2, 2]))                      # [[0.0, 0.0], [0.0, 0.0]]
io.print(T.ones([3]))                          # [1.0, 1.0, 1.0]
io.print(T.eye(3))                             # the 3×3 identity
io.print(T.arange(0, 6, 2))                    # [0.0, 2.0, 4.0]
io.print(T.full([2, 2], 9))                    # [[9.0, 9.0], [9.0, 9.0]]
```

A 3-D tensor is just a deeper nesting:

```kirito
var io = import("io")
var T = import("tensor")
var cube = T.Tensor([[[1, 2], [3, 4]], [[5, 6], [7, 8]]])   # shape [2, 2, 2]
io.print(cube.shape())          # [2, 2, 2]
io.print(cube.ndim())           # 3
```

## Indexing

A **full** index (one integer per dimension) returns the scalar element; a **partial** index returns
the sub-tensor of the remaining axes:

```kirito
var io = import("io")
var T = import("tensor")
var cube = T.Tensor([[[1, 2], [3, 4]], [[5, 6], [7, 8]]])

io.print(cube[1, 0, 1])         # 6.0   — a single element
io.print(cube[0])               # [[1.0, 2.0], [3.0, 4.0]]   — a 2×2 sub-tensor
cube[0, 0, 0] = 99              # assign an element
io.print(cube[0, 0, 0])         # 99.0
```

## Element-wise arithmetic and broadcasting

`+`, `-`, `*`, `/` are **element-wise** — note `*` is the Hadamard (element-wise) product, **not**
matrix multiply. Shapes are combined with NumPy **broadcasting**: align from the right, and each axis
must be equal or `1`.

```kirito
var io = import("io")
var T = import("tensor")
var a = T.Tensor([[1, 2, 3], [4, 5, 6]])

io.print(a + a)                       # [[2.0, 4.0, 6.0], [8.0, 10.0, 12.0]]
io.print(a * a)                       # element-wise square
io.print(a * 2)                       # scalar multiply
io.print(a + T.Tensor([[10, 20, 30]]))   # broadcast a row over both rows
io.print(a + T.Tensor([[100], [200]]))   # broadcast a column over all columns
```

## Matrix products and reductions

Matrix multiply is the `matmul` method (since `*` is element-wise); it also works **batched** over the
leading dimensions. Reductions take an optional `axis`:

```kirito
var io = import("io")
var T = import("tensor")
var a = T.Tensor([[1, 2, 3], [4, 5, 6]])

io.print(T.Tensor([[1, 2], [3, 4]]).matmul(T.Tensor([[5, 6], [7, 8]])))  # [[19.0, 22.0], [43.0, 50.0]]
io.print(T.Tensor([1, 2, 3]).dot(T.Tensor([4, 5, 6])))                   # 32.0

io.print(a.sum())               # 21.0   — the whole tensor
io.print(a.sum(0))              # [5.0, 7.0, 9.0]   — reduce axis 0
io.print(a.sum(1))              # [6.0, 15.0]       — reduce axis 1
io.print(a.mean(), a.min(), a.max())   # 3.5 1.0 6.0
```

## Reshaping and mapping

```kirito
var io = import("io")
var T = import("tensor")
var a = T.Tensor([[1, 2, 3], [4, 5, 6]])

io.print(a.transpose())         # [[1.0, 4.0], [2.0, 5.0], [3.0, 6.0]]
io.print(a.reshape([3, 2]))     # same data, new shape
io.print(a.flatten())           # [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]
io.print(a.permute([1, 0]))     # general axis reorder (== transpose for 2-D)

# apply maps a function over every element — the loop runs in C++:
io.print(T.Tensor([1, 4, 9]).apply(Function(x): return x ** 0.5))   # [1.0, 2.0, 3.0]
```

## Contraction: `tensordot`

`matmul` contracts one pair of axes; `tensordot` contracts any number of them. Pass an integer `N` to
contract the last `N` axes of the first tensor with the first `N` of the second (`N = 1` is matrix
multiply; `N = 2` sums every element-product — the Frobenius inner product), or a `[a-axes, b-axes]`
pair to name exactly which axes to fold together. `contract(a, b, aaxes, baxes)` is the explicit form.

```kirito
var io = import("io")
var T = import("tensor")
var a = T.Tensor([[1, 2], [3, 4]])
var b = T.Tensor([[5, 6], [7, 8]])

io.print(T.tensordot(a, b, 1))      # [[19.0, 22.0], [43.0, 50.0]]  — same as a.matmul(b)
io.print(T.tensordot(a, b, 2))      # 70.0  — sum of all a[i,j]*b[i,j]

var x = T.full([2, 3, 4], 1)
var y = T.full([4, 3, 5], 2)
io.print(T.contract(x, y, [1, 2], [1, 0]).shape())   # [2, 5]
```

## Autograd: gradients with `backward`

Tensors can compute their own derivatives. Tracking is **off by default** — mark a tensor with
`requiresgrad = True` (or `t.requiresgrad(True)` later) to make it a differentiable leaf. Operations on
grad-tracking tensors record a graph; calling `backward()` on a scalar result fills in each input's
`.grad`. Gradients are **Float-only** — a Complex tensor cannot require gradients (autograd is for the
real-valued Float dtype).

```kirito
var io = import("io")
var T = import("tensor")

var x = T.Tensor([1, 2, 3], requiresgrad = True)
var y = x.square().sum()        # y = x0^2 + x1^2 + x2^2
y.backward()
io.print(x.grad)                # [2.0, 4.0, 6.0]   — dy/dx = 2x
```

A great many operations are differentiable: `+ - * /` (with broadcasting), `matmul`, `tensordot`,
`sum`/`mean`, the shape ops, and a wide element-wise math set — `exp`, `log`, `sqrt`, `pow`, `sin`,
`cos`, `tanh`, `relu`, `sigmoid`, and more. That's enough to train a model. Here is a line fitted by
gradient descent:

```kirito
var io = import("io")
var T = import("tensor")

var xs = T.Tensor([[0], [1], [2], [3]])
var ys = T.Tensor([[1], [3], [5], [7]])         # y = 2x + 1
var w = T.zeros([1, 1], requiresgrad = True)
var b = T.zeros([1, 1], requiresgrad = True)
var step = 0
while step < 500:
    var loss = (xs.matmul(w) + b - ys).square().mean()
    w.zerograd()                                # gradients accumulate, so clear them each step
    b.zerograd()
    loss.backward()
    with T.nograd():                            # update the parameters without tracking
        w = w - w.grad * 0.05
        b = b - b.grad * 0.05
    w.requiresgrad(True)
    b.requiresgrad(True)
    step = step + 1
io.print(w[0, 0], b[0, 0])                       # ~ 2.0  ~ 1.0
```

The pieces: `t.grad` is the accumulated gradient (or `None`); `t.zerograd()` clears it; `t.detach()`
returns a copy that stops gradient flow; and `with tensor.nograd():` turns tracking off for a block —
exactly what you want around a parameter update or during inference.

## Complex tensors

Pass `dtype = "Complex"` to work over `std::complex<double>`. The same operations apply; `astype`
converts between dtypes, and mixing a Float and a Complex tensor promotes the result to Complex.

```kirito
var io = import("io")
var T = import("tensor")
var C = import("complex")

var z = T.Tensor([[1, 2], [3, 4]], dtype = "Complex")
io.print(z.dtype())             # Complex
io.print(z[0, 0])               # 1.0+0.0i   — elements are Complex values
io.print(z.matmul(z))           # complex matrix product
io.print(T.Tensor([1, 2]).astype("Complex").dtype())   # Complex
io.print((T.Tensor([1, 2]) + T.Tensor([1, 1], dtype = "Complex")).dtype())   # Complex (promoted)
```

(`min`/`max` raise for a Complex tensor — complex numbers are unordered.)

Complex tensors are a numeric container only: **autograd is Float-only**, so a Complex tensor cannot
require gradients (`requiresgrad(True)` on one raises), and the differentiable element-wise math
methods are Float-only too (for complex analytic functions use the `complex` module). Everything else
— arithmetic, `matmul`, `tensordot`, reshaping, reductions — works on both dtypes.

## Recap

- A tensor is an N-dimensional array; `matrix` and `complex` matrices are 2-D tensors.
- Construct with `Tensor(nested)` or `zeros`/`ones`/`full`/`eye`/`arange`; `dtype` is `"Float"`
  (default) or `"Complex"`.
- `t[i, j, ...]` is a full index (element); `t[i]` is a partial index (sub-tensor).
- `+ - * /` are element-wise with broadcasting; **`matmul`** is the matrix product (also batched).
- `transpose`/`permute`/`reshape`/`flatten` reshape; **`apply`** maps a function over every element;
  `sum`/`mean`/`prod`/`min`/`max` reduce (whole-tensor or along an `axis`).
- **`tensordot`**/`contract` contract over chosen axes; `matmul` is the single-axis special case.
- **Autograd** is opt-in (`requiresgrad = True`) and Float-only: differentiable ops record a graph,
  `backward()` fills each input's `.grad`; `zerograd()`, `detach()`, and `with tensor.nograd():`
  control gradient flow.

Next: **[Lesson 25 — Capstone](34-course-25-capstone.html)**.
