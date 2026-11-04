# Chapter 31 — DRMVM graphics and optional libdrm

[Previous](30-ssavm-analysis.md) · [Contents](README.md) · [Next](32-jvm-integer-subset.md)

## Deterministic software surface

`metavm::machines::Graphics`, in `VMLibrary/DRMVM/Graphics.hpp`, owns a
row-major XRGB8888 surface. Width and height must be nonzero and their product
may not exceed 16,777,216 pixels. The constructor allocates the pixel vector;
invalid dimensions throw, and allocation can fail normally.

`width()`, `height()`, and `pixels()` expose dimensions and a const vector view.
`clear(rgb)` fills with the low 24 color bits. `rectangle(x, y, w, h, rgb)`
clips width/height against the remaining surface; an origin outside the surface
has no effect. It avoids overflowing `x+w` or `y+h` while clipping. There is
no alpha blending, display connection, command queue, GPU shader, or presenter.

```cpp
metavm::machines::Graphics surface(4, 4);
surface.clear(0);
surface.rectangle(1, 1, 2, 2, 0xff0000);
// surface.pixels()[5] == 0x00ff0000
```

## Machinery import contract

`surface.bind(vm, &error)` atomically installs:

| Import | Arity | Arguments |
|---|---:|---|
| `drm_clear` | 1 | Integer color |
| `drm_rect` | 5 | Integer x, y, width, height, color |

Rectangle geometry must fit unsigned 32-bit ranges; negative values fail.
Colors are converted to u32 and reduced to RGB by the surface. Successful
callbacks return nil. Existing-name collisions or any batch failure leave
the registry unchanged. There is no unregister implied by `bind`.

Keep the surface alive and at a stable address until all bound uses end;
callbacks retain its `this` pointer. Do not move a bound object or assume a
copy automatically becomes the callback target. Synchronize surface changes
and presentation against VM writes.

The DRMVM catalog assembly imports these functions. Running that example in
the generic runner without bindings fails; that is correct capability checking,
not evidence of an invalid example. A host binds a surface, runs its module,
and presents the pixels separately.

## Optional KMS dumb buffer

Enable `METAVM_ENABLE_LIBDRM=ON` only with real pkg-config/libdrm development
files. The exported native header exposes `DumbBuffer` under
`METAVM_HAVE_LIBDRM`; consumers should use the target's interface definition,
not define the macro against a library built without support.

`DumbBuffer(fd, width, height)` duplicates a caller-authorized KMS-capable fd,
creates a 32-bpp dumb GEM allocation, validates pitch/size, and maps it. On
failure it releases acquired resources. Destruction unmaps, destroys the GEM
allocation, and closes the duplicate in reverse order. Copying is disabled.
`handle()` and `pitch()` expose allocation metadata; `upload(surface)` requires
matching dimensions and copies each row using the kernel-provided pitch.

It does **not** acquire DRM master, discover/select devices or connectors,
create a scanout framebuffer, set a CRTC, perform atomic modesetting, page-flip,
negotiate DMA-BUF formats, or synchronize scanout. Render nodes generally do
not provide the required dumb-buffer KMS interface. Never guess a privileged
fd or a display on the user's behalf.

The software path is the deterministic tested backend. Hardware/libdrm package
validation remains open. The upload representation assumes the documented
XRGB8888 pixel layout; extending beyond tested little-endian behavior needs
an explicit format/endian contract and hardware tests.

**References:** [Graphics.hpp](../VMLibrary/DRMVM/Graphics.hpp), [implementation](../VMLibrary/DRMVM/Graphics.cpp).
