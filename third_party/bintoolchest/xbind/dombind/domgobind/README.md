# domgobind

C ABI buffers and callbacks, with native cgo byte conversion and panic containment. Like `dompybind`, this library uses Q templates with per-module
`PREFIX` parameters, guarded `static inline` C functions and an aggregate
`gobind.q` with explicit import prefixes. Its C++17 ownership helpers live in
`include/domgobind/domgobind.hpp`.

## Build

Requires Go 1.20+ with cgo and a C compiler for the native adapter. Header generation additionally requires Python with
Tcl/Tk and the repository's `q` package from `domlibs/domqlib` on its normal
Python import path. CMake 3.20+ is required for the build integration.

From `dombind/`:

```sh
python domgobind/generate.py /tmp/include/domgobind/capi.h
cmake -S domgobind -B /tmp/domgobind-build
cmake --build /tmp/domgobind-build
cmake --install /tmp/domgobind-build --prefix /tmp/domgobind-install
```

```cmake
# Source tree:
add_subdirectory(path/to/dombind/domgobind domgobind-build)
target_link_libraries(host PRIVATE DomGoBind::DomGoBind)
# Or, after installation:
# find_package(DomGoBind CONFIG REQUIRED)
```

`#include <domgobind/capi.h>` for C or
`#include <domgobind/domgobind.hpp>` for C++. To generate an individual
module, add `--module NAME`; `--prefix custom` renames that module's functions
and types. Aggregate imports intentionally retain each submodule's own prefix.

## Modules

| Module | Operations (prepend the module name and `_`) |
|---|---|
| `gobuf` | `copy`, `free`, `slice` |
| `gocall` | `invoke` |

`gobind` imports every module. The source `.q` files document the exact
native signatures; runtime error conventions are preserved.

## Ownership and runtime contract

- `gobuf_t` owns memory from the C allocator. Initialize it to `{0}`; `gobuf_copy` requires an empty output, returns 0 on success, -1 for invalid input, -2 for allocation failure, and leaves the output unchanged on failure. A nonempty source must address at least `size` readable bytes.
- `gobuf_free` releases the allocation and zeroes the descriptor. It is idempotent for the same descriptor. Do not shallow-copy owning descriptors; the C++ `buffer` provides deep copies and ownership-transferring moves.
- `gobuf_view` is borrowed. `gobuf_slice` checks ranges without overflow, accepts empty slices, and never transfers ownership. Neither buffers nor views add NUL terminators.
- `gocall_t` pairs a C function pointer with an opaque, borrowed context. The callback returns its own integer status; `invoke` returns -1 for a missing callback or invalid byte input. The caller coordinates context lifetime and thread safety.
- Do not pass language-native strings, slices, objects or exceptions directly through the C ABI. Use pointer/length pairs, fixed-width scalars and opaque handles. Memory must be freed by the allocator/runtime that created it.
- The native adapter supplies conversion and panic/exception containment. It does not invent a runtime interpreter API or automatically export arbitrary native types.
- CopyBytes copies C-owned input into Go storage and validates null/size combinations; it cannot validate the allocation behind an arbitrary pointer. C must not retain Go pointers. Use `runtime/cgo.Handle` for persistent Go state and delete each handle when the C owner releases it.
- `Boundary` catches recoverable panics. Fatal runtime errors and process exit cannot be translated. Build exported functions with cgo `//export` and `-buildmode=c-shared`; the generated Go header is authoritative for those exports.

## Native adapter

The local Go module is `domweave/domgobind`; use a `replace` directive pointing
at this directory when consuming it from another module. `examples/export`
exports `domgo_sum`, including binary input and error handling.

```sh
go test ./...
go build -buildmode=c-shared -o /tmp/libdomgo.so ./examples/export
```

## Tests

From `dombind/`, run `python -m unittest discover -s tests -v`. The suite
compiles each module and a custom-prefix instantiation in C and C++, compiles
the C++ ownership layer, and executes real runtime or shared-library examples.
Missing development toolchains are reported as skips. See `../tests/README.md`.
