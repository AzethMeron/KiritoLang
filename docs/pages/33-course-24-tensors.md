# Lesson 24 — Tensors

A **tensor** is a dense array of any number of dimensions — the generalization of a matrix beyond
2-D. The `tensor` module is the engine the `matrix` and `complex` matrix types are built on: a 2-D
tensor *is* a matrix. It's CPU-only and has no autograd — a solid numeric container with all the
usual array operations and NumPy-style broadcasting.

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

## Recap

- A tensor is an N-dimensional array; `matrix` and `complex` matrices are 2-D tensors.
- Construct with `Tensor(nested)` or `zeros`/`ones`/`full`/`eye`/`arange`; `dtype` is `"Float"`
  (default) or `"Complex"`.
- `t[i, j, ...]` is a full index (element); `t[i]` is a partial index (sub-tensor).
- `+ - * /` are element-wise with broadcasting; **`matmul`** is the matrix product (also batched).
- `transpose`/`permute`/`reshape`/`flatten` reshape; **`apply`** maps a function over every element;
  `sum`/`mean`/`prod`/`min`/`max` reduce (whole-tensor or along an `axis`).

Next: **[Lesson 25 — Capstone](34-course-25-capstone.html)**.
