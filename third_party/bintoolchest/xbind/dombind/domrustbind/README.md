# domrustbind

C ABI buffers and callbacks, with native Rust byte views and panic containment. Like `dompybind`, this library uses Q templates with per-module
`PREFIX` parameters, guarded `static inline` C functions and an aggregate
`rustbind.q` with explicit import prefixes. Its C++17 ownership helpers live in
`include/domrustbind/domrustbind.hpp`.

## Build

Requires Rust edition 2021 toolchain for the native adapter; no crate dependencies. Header generation additionally requires Python with
Tcl/Tk and the repository's `q` package from `domlibs/domqlib` on its normal
Python import path. CMake 3.20+ is required for the build integration.

From `dombind/`:

```sh
python domrustbind/generate.py /tmp/include/domrustbind/capi.h
cmake -S domrustbind -B /tmp/domrustbind-build
cmake --build /tmp/domrustbind-build
cmake --install /tmp/domrustbind-build --prefix /tmp/domrustbind-install
```

```cmake
# Source tree:
add_subdirectory(path/to/dombind/domrustbind domrustbind-build)
target_link_libraries(host PRIVATE DomRustBind::DomRustBind)
# Or, after installation:
# find_package(DomRustBind CONFIG REQUIRED)
```

`#include <domrustbind/capi.h>` for C or
`#include <domrustbind/domrustbind.hpp>` for C++. To generate an individual
module, add `--module NAME`; `--prefix custom` renames that module's functions
and types. Aggregate imports intentionally retain each submodule's own prefix.

## Modules

| Module | Operations (prepend the module name and `_`) |
|---|---|
| `rustbuf` | `copy`, `free`, `slice` |
| `rustcall` | `invoke` |

`rustbind` imports every module. The source `.q` files document the exact
native signatures; runtime error conventions are preserved.

## Ownership and runtime contract

- `rustbuf_t` owns memory from the C allocator. Initialize it to `{0}`; `rustbuf_copy` requires an empty output, returns 0 on success, -1 for invalid input, -2 for allocation failure, and leaves the output unchanged on failure. A nonempty source must address at least `size` readable bytes.
- `rustbuf_free` releases the allocation and zeroes the descriptor. It is idempotent for the same descriptor. Do not shallow-copy owning descriptors; the C++ `buffer` provides deep copies and ownership-transferring moves.
- `rustbuf_view` is borrowed. `rustbuf_slice` checks ranges without overflow, accepts empty slices, and never transfers ownership. Neither buffers nor views add NUL terminators.
- `rustcall_t` pairs a C function pointer with an opaque, borrowed context. The callback returns its own integer status; `invoke` returns -1 for a missing callback or invalid byte input. The caller coordinates context lifetime and thread safety.
- Do not pass language-native strings, slices, objects or exceptions directly through the C ABI. Use pointer/length pairs, fixed-width scalars and opaque handles. Memory must be freed by the allocator/runtime that created it.
- The native adapter supplies conversion and panic/exception containment. It does not invent a runtime interpreter API or automatically export arbitrary native types.
- `ByteView::as_slice` is unsafe: the caller guarantees allocation bounds, lifetime and immutability. `Callback::invoke` is unsafe because the caller supplies the code/context lifetime and thread contract.
- `boundary` catches unwinding panics (`panic=unwind`). Abort-mode panics, allocation aborts and invalid pointers cannot be caught. The crate never reconstructs a Vec from a C-allocated buffer.

## Native adapter

Use a Cargo path dependency on this directory. `src/lib.rs` supplies `ByteView`,
`Callback`, `Status` and `boundary`. `examples/export.rs` demonstrates an
`extern "C"` export with a checked output pointer.

```sh
cargo test --offline
cargo build --offline
rustc --edition=2021 --crate-type=cdylib examples/export.rs \
  --extern domrustbind=target/debug/libdomrustbind.rlib -o /tmp/libdomrust.so
```

## Tests

From `dombind/`, run `python -m unittest discover -s tests -v`. The suite
compiles each module and a custom-prefix instantiation in C and C++, compiles
the C++ ownership layer, and executes real runtime or shared-library examples.
Missing development toolchains are reported as skips. See `../tests/README.md`.
