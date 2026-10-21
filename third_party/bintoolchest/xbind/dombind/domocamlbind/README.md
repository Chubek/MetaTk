# domocamlbind

OCaml runtime lifecycle, rooted values, conversions, blocks and protected callbacks. Like `dompybind`, this library uses Q templates with per-module
`PREFIX` parameters, guarded `static inline` C functions and an aggregate
`ocamlbind.q` with explicit import prefixes. Its C++17 ownership helpers live in
`include/domocamlbind/domocamlbind.hpp`.

## Build

Requires OCaml development headers and ocamlc; runtime linking is handled by the OCaml toolchain. Header generation additionally requires Python with
Tcl/Tk and the repository's `q` package from `domlibs/domqlib` on its normal
Python import path. CMake 3.20+ is required for the build integration.

From `dombind/`:

```sh
python domocamlbind/generate.py /tmp/include/domocamlbind/capi.h
cmake -S domocamlbind -B /tmp/domocamlbind-build
cmake --build /tmp/domocamlbind-build
cmake --install /tmp/domocamlbind-build --prefix /tmp/domocamlbind-install
```

```cmake
# Source tree:
add_subdirectory(path/to/dombind/domocamlbind domocamlbind-build)
target_link_libraries(host PRIVATE DomOCamlBind::DomOCamlBind)
# Or, after installation:
# find_package(DomOCamlBind CONFIG REQUIRED)
```

`#include <domocamlbind/capi.h>` for C or
`#include <domocamlbind/domocamlbind.hpp>` for C++. To generate an individual
module, add `--module NAME`; `--prefix custom` renames that module's functions
and types. Aggregate imports intentionally retain each submodule's own prefix.

## Modules

| Module | Operations (prepend the module name and `_`) |
|---|---|
| `ocamlblock` | `pair`, `some`, `none`, `get`, `set` |
| `ocamlcall` | `named`, `call`, `call2`, `is_exception`, `exception` |
| `ocamlconv` | `from_int`, `to_int`, `from_int64`, `to_int64`, `from_double`, `to_double`, `string`, `string_size`, `string_data` |
| `ocamlinit` | `startup`, `shutdown`, `release`, `acquire` |
| `ocamlroot` | `retain`, `set`, `release` |

`ocamlbind` imports every module. The source `.q` files document the exact
native signatures; runtime error conventions are preserved.

## Ownership and runtime contract

- The host owns `ocamlinit_startup`/`shutdown`; an OCaml-called C stub must not initialize another runtime. Hold the runtime lock for all value operations, and release it only around native work that neither touches values nor invokes OCaml.
- `value` can move during GC. Use `CAMLparam`/`CAMLlocal` in allocating C stubs. A persistent root slot must have a stable address and be registered through `ocamlroot_retain`; update it with `ocamlroot_set` and unregister it before destruction.
- `ocamlblock_pair`/`some` and callback helpers root their value inputs while allocating/calling. Callers must root returned values before another allocation. Block field access requires a valid scanned block and in-range index; these are low-level primitives, not dynamic type checks.
- Integer helpers target OCaml's tagged native integer range; use int64 helpers for a full 64-bit value. Conversion accessors require the corresponding OCaml runtime type.
- String lengths are explicit. `ocamlconv_string_data` is borrowed and invalidated by moving GC. Input pointers to `ocamlconv_string` must refer to stable native memory, not an unrooted movable OCaml allocation.
- `ocamlcall_named` returns NULL for an unregistered name. Callbacks use `_exn`; inspect `ocamlcall_is_exception`, then extract the exception before treating the result as an ordinary value.
- C++ `root` owns a stable generational-root slot. C++ `call` translates encoded callback exceptions into a rooted `error`. OCaml allocation failures and raising C APIs can still perform a nonlocal jump; do not let those jumps cross C++ destructors. Catch C++ errors before returning from an OCaml C stub.
- Link stubs with `ocamlc -custom` or the corresponding native OCaml workflow. The CMake interface provides headers, not a guessed runtime archive or startup object.

## Example

See `examples/stub.c`:

```c
#include <domocamlbind/capi.h>
CAMLprim value make_pair(value input) {
    CAMLparam1(input);
    CAMLlocal2(text, pair);
    text = ocamlconv_string("ok", 2);
    pair = ocamlblock_pair(input, text);
    CAMLreturn(pair);
}
```

Build the example after generating `/tmp/include/domocamlbind/capi.h`:

```sh
cc -I/tmp/include -I"$(ocamlc -where)" -c examples/stub.c -o /tmp/domocaml-stub.o
ocamlc -custom -o /tmp/domocaml-example /tmp/domocaml-stub.o examples/main.ml
/tmp/domocaml-example
```

## Tests

From `dombind/`, run `python -m unittest discover -s tests -v`. The suite
compiles each module and a custom-prefix instantiation in C and C++, compiles
the C++ ownership layer, and executes real runtime or shared-library examples.
Missing development toolchains are reported as skips. See `../tests/README.md`.
