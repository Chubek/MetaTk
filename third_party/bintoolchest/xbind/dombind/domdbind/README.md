# domdbind

C ABI buffers and callbacks, with native D exception containment. Like `dompybind`, this library uses Q templates with per-module
`PREFIX` parameters, guarded `static inline` C functions and an aggregate
`dbind.q` with explicit import prefixes. Its C++17 ownership helpers live in
`include/domdbind/domdbind.hpp`.

## Build

Requires D with druntime for the native adapter; C99/C++17 for consumers. Header generation additionally requires Python with
Tcl/Tk and the repository's `q` package from `domlibs/domqlib` on its normal
Python import path. CMake 3.20+ is required for the build integration.

From `dombind/`:

```sh
python domdbind/generate.py /tmp/include/domdbind/capi.h
cmake -S domdbind -B /tmp/domdbind-build
cmake --build /tmp/domdbind-build
cmake --install /tmp/domdbind-build --prefix /tmp/domdbind-install
```

```cmake
# Source tree:
add_subdirectory(path/to/dombind/domdbind domdbind-build)
target_link_libraries(host PRIVATE DomDBind::DomDBind)
# Or, after installation:
# find_package(DomDBind CONFIG REQUIRED)
```

`#include <domdbind/capi.h>` for C or
`#include <domdbind/domdbind.hpp>` for C++. To generate an individual
module, add `--module NAME`; `--prefix custom` renames that module's functions
and types. Aggregate imports intentionally retain each submodule's own prefix.

## Modules

| Module | Operations (prepend the module name and `_`) |
|---|---|
| `dbuf` | `copy`, `free`, `slice` |
| `dcall` | `invoke` |

`dbind` imports every module. The source `.q` files document the exact
native signatures; runtime error conventions are preserved.

## Ownership and runtime contract

- `dbuf_t` owns memory from the C allocator. Initialize it to `{0}`; `dbuf_copy` requires an empty output, returns 0 on success, -1 for invalid input, -2 for allocation failure, and leaves the output unchanged on failure. A nonempty source must address at least `size` readable bytes.
- `dbuf_free` releases the allocation and zeroes the descriptor. It is idempotent for the same descriptor. Do not shallow-copy owning descriptors; the C++ `buffer` provides deep copies and ownership-transferring moves.
- `dbuf_view` is borrowed. `dbuf_slice` checks ranges without overflow, accepts empty slices, and never transfers ownership. Neither buffers nor views add NUL terminators.
- `dcall_t` pairs a C function pointer with an opaque, borrowed context. The callback returns its own integer status; `invoke` returns -1 for a missing callback or invalid byte input. The caller coordinates context lifetime and thread safety.
- Do not pass language-native strings, slices, objects or exceptions directly through the C ABI. Use pointer/length pairs, fixed-width scalars and opaque handles. Memory must be freed by the allocator/runtime that created it.
- The native adapter supplies conversion and panic/exception containment. It does not invent a runtime interpreter API or automatically export arbitrary native types.
- `copyBytes` creates a D-managed copy of a borrowed C view. Root any D-managed object retained by foreign code using druntime's GC facilities; a pointer stored only in C is not a GC root.
- `boundary` converts D Throwable failures into a status before an `extern(C)` return. Native hosts linking a D shared library must follow their compiler's druntime initialization and thread-registration requirements.

## Native adapter

`source/domdbind.d` is importable as `domdbind`; `dub.json` describes the package.
The LDC example exports `domd_sum` with exception containment.

```sh
ldc2 -unittest -main source/domdbind.d -of=/tmp/domdbind-unit
/tmp/domdbind-unit
ldc2 -shared -relocation-model=pic -Isource source/domdbind.d \
  examples/export.d -of=/tmp/libdomd.so
```

## Tests

From `dombind/`, run `python -m unittest discover -s tests -v`. The suite
compiles each module and a custom-prefix instantiation in C and C++, compiles
the C++ ownership layer, and executes real runtime or shared-library examples.
Missing development toolchains are reported as skips. See `../tests/README.md`.
