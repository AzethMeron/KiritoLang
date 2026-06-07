# imaging — a Pillow-style image library in pure Kirito

A from-scratch image-processing library written entirely in Kirito (`.ki`), in the spirit of
Python's [Pillow](https://python-pillow.org/) (PIL). Pixels are stored in a **`tensor`** of shape
`(height, width, channels)`, and the `tensor` standard library is the numerical backend: every
conversion, geometric transform, point operation and convolution filter is expressed as a vectorised
tensor operation rather than a per-pixel loop.

It is both a useful library and a substantial example program — and building it surfaced and fixed a
real interpreter bug (a GC-rooting gap in `tensor.tolist()` for Float tensors; see the repo history).

> Performance note: this is a *tree-walking interpreter*, so work that is unavoidably per-pixel in
> Kirito (PNG un-filtering, `getpixel`/`putpixel` loops, histogram passes) is slow. Keep images
> modest (a few hundred pixels per side). The heavy numeric paths — conversions, resizing, flips and
> all the convolution filters — run through bulk tensor ops and stay fast.

## Install

It's a normal Kirito package. With `kpm` (the package manager) it installs from its GitHub repo:

```
kpm install <owner>/<repo>
```

and then `import("imaging")` works anywhere. In *this* repository you can run the modules in place,
because a script's own directory is on the import path:

```
ki examples/big_projects/imaging/demo.ki          # writes a gallery of PNGs to the temp dir
ki examples/big_projects/imaging/test_imaging.ki   # the self-test (asserts; prints ALL TESTS PASSED)
```

## Layout

```
kirito.json        the kpm manifest (name/version/modules)
imaging.ki         the Image class + new/open/save/fromtensor/merge/blend + PNG/PPM/BMP codecs
imageops.ki        ImageOps: invert/grayscale/mirror/flip/posterize/solarize/autocontrast/equalize/...
imagefilter.ki     ImageFilter: convolution kernels (BLUR/SHARPEN/...) + Gaussian/Box/rank filters
imagedraw.ki       ImageDraw: point/line/rectangle/ellipse/polygon (mutating primitives)
demo.ki            a tour that produces ~18 real PNGs
test_imaging.ki    93-check self-test (also wired into CTest as `script_imaging`)
compare_pillow.py  cross-validates the output against Pillow, pixel for pixel
```

## Quick start

```kirito
var Image = import("imaging")
var ImageFilter = import("imagefilter")
var ImageOps = import("imageops")

var im = Image.open("photo.png")        # PNG / PPM / PGM / BMP, sniffed from the header
io.print(im.mode, im.size)              # e.g. RGB [640, 480]

var small = im.resize([128, 96], Image.BILINEAR).convert("L")
var edges = small.filter(ImageFilter.FIND_EDGES)
ImageOps.autocontrast(edges).save("edges.png")
```

The module object *is* the `Image` namespace, so it reads just like `from PIL import Image`:
`Image.new(...)`, `Image.open(...)`, plus the `Image.FLIP_LEFT_RIGHT` / `Image.ROTATE_90` /
`Image.BILINEAR` constants.

## The `Image` class

| method | description |
|---|---|
| `Image.new(mode, size[, color])` | a blank image; `mode` is `"L"`/`"RGB"`/`"RGBA"`, `size` is `[w, h]` |
| `Image.open(path)` / `Image.frombytes(bytes)` | decode a file / an in-memory blob (format sniffed) |
| `img.save(path[, format])` / `img.tobytes(format)` | encode as PNG/PPM/BMP (by extension or explicit) |
| `img.size` / `img.width` / `img.height` / `img.mode` | dimensions and pixel mode |
| `img.getpixel([x, y])` / `img.putpixel([x, y], v)` | read / write one pixel (`putpixel` mutates) |
| `img.convert(mode)` | `L`↔`RGB`↔`RGBA` (RGB→L uses Pillow's ITU-R 601-2 luma) |
| `img.crop([l, u, r, b])` | a sub-image (right/lower exclusive) |
| `img.resize([w, h][, resample])` | `NEAREST` (default) or `BILINEAR` |
| `img.thumbnail([w, h])` | shrink to fit, preserving aspect ratio |
| `img.transpose(method)` | `FLIP_LEFT_RIGHT`/`FLIP_TOP_BOTTOM`/`ROTATE_90`/`180`/`270`/`TRANSPOSE`/`TRANSVERSE` |
| `img.rotate(angle)` | a multiple of 90° (counter-clockwise, Pillow's convention) |
| `img.paste(other, [x, y])` | composite another image in place |
| `img.point(fn)` | remap every channel value through `fn` (via a 256-entry LUT) |
| `img.split()` / `Image.merge(mode, bands)` | separate / recombine channels |
| `img.histogram()` | per-channel 256-bin histogram |
| `img.filter(flt)` | apply an `imagefilter` (see below) |
| `img.tensor()` / `Image.fromtensor(t, mode)` | the **tensor bridge** — drop to raw `(H,W,C)` and back |
| `img.tolist()` | a nested List of Integer pixels |
| `Image.blend(a, b, alpha)` | linear cross-fade of two same-size images |

## ImageOps, ImageFilter, ImageDraw

```kirito
var ImageOps = import("imageops")
# invert, grayscale, mirror, flip, posterize(bits), solarize(threshold), autocontrast(cutoff),
# equalize, expand(border, fill), colorize(black, white), scale(factor), fit(size)

var ImageFilter = import("imagefilter")
# kernels: BLUR, SHARPEN, SMOOTH, SMOOTH_MORE, DETAIL, EDGE_ENHANCE(_MORE), FIND_EDGES, EMBOSS, CONTOUR
# parametric: GaussianBlur(radius), BoxBlur(radius), MedianFilter(size), MinFilter(size), MaxFilter(size)
# custom:     Kernel(size, flat_kernel[, scale[, offset]])

var ImageDraw = import("imagedraw")
var d = ImageDraw.Draw(img)
d.line([0, 0, 99, 99], [255, 0, 0])
d.rectangle([10, 10, 40, 30], [0, 0, 80], [255, 255, 0])   # fill, outline
d.ellipse([20, 20, 60, 50], [0, 128, 0])
d.polygon([5, 5, 60, 8, 30, 55], [80, 80, 80], [200, 200, 200])
```

Convolution filters are the showcase of the tensor backend: an image is edge-padded once, then for
each of the kernel's *K×K* taps the **whole** shifted window is scaled and accumulated — `K*K`
vectorised tensor operations, no per-pixel loop. Rank filters (`Min`/`Max`/`Median`) stack the
shifted windows and reduce along the new axis.

## The tensor backend, and a note on mutation

`img.tensor()` hands you the underlying `(H, W, C)` Float tensor, so you can compute with the full
`tensor` library and wrap the result back with `Image.fromtensor(t, mode)`:

```kirito
var T = import("tensor")
var d = img.tensor()
# boost contrast around mid-grey, purely in tensor ops
var stretched = ((d + (-128.0)) * 1.4 + 128.0).clip(0.0, 255.0)
var out = Image.fromtensor(stretched, img.mode)
```

Kirito tensors are **immutable** under arithmetic — every op returns a new tensor. The one in-place
operation is element assignment (`t[i, j] = v`), and that is exactly what `putpixel`, `paste` and the
`ImageDraw` primitives use, so they can mutate an image in place like Pillow. Everything else
(`convert`, `crop`, `resize`, `filter`, the `ImageOps`) is functional and returns a **new** image.

## Formats

| format | read | write | notes |
|---|---|---|---|
| PNG | ✓ | ✓ | 8-bit, colour types 0/2/6 (L/RGB/RGBA); decodes all five scanline filters incl. Paeth; zlib via the stdlib |
| PPM/PGM | ✓ | ✓ | binary Netpbm (P6/P5); handles `#` comments on read |
| BMP | ✓ | ✓ | 24-bit uncompressed, bottom-up BGR |

PNG is the interesting one: encoding writes signature + IHDR + zlib-compressed filtered scanlines +
IEND, with CRC-32 per chunk (both from the `zlib` and `hash` stdlib modules); decoding parses the
chunks, inflates the IDAT stream and reverses the per-row filters into a tensor.

## Cross-validation against Pillow

`compare_pillow.py` proves the output is genuinely compatible: it has the Kirito side write a base
image and a set of operation results, then re-computes the same operations with Pillow and asserts a
**pixel-exact** match (a one-count tolerance only for the greyscale luma rounding). It also has
Pillow author adaptively-filtered PNGs (RGB/RGBA/L) and checks that Kirito decodes them exactly.

```
pip install pillow
python3 examples/big_projects/imaging/compare_pillow.py
# -> CROSS-VALIDATION PASSED -- Kirito imaging matches Pillow
```

## Serialization & inspection

Because an `Image` is an ordinary Kirito class wrapping a (serializable) tensor and a string, it
round-trips through both `serialize` (text) and `dump` (binary) with no extra work, and `inspect(img)`
lists its full method surface — the same guarantees every Kirito value carries.

## Limitations (room to grow)

- 8-bit channels only (no 16-bit / float-HDR images); palette PNGs (colour type 3) aren't decoded.
- `rotate` covers 90° multiples only; arbitrary-angle resampling isn't implemented.
- No text rendering (Pillow's `ImageFont`), no JPEG (it would need a DCT codec), no animation.
- `GaussianBlur` is a true discrete Gaussian rather than Pillow's box-approximation, so blurred
  output is visually equivalent but not bit-identical.
