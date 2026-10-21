# domnodebind

Node-API objects, arrays, buffers, conversions, calls, references and exceptions. Like `dompybind`, this library uses Q templates with per-module
`PREFIX` parameters, guarded `static inline` C functions and an aggregate
`nodebind.q` with explicit import prefixes. Its C++17 ownership helpers live in
`include/domnodebind/domnodebind.hpp`.

## Build

Requires Node.js with Node-API version 6 or later and node_api.h development headers. Header generation additionally requires Python with
Tcl/Tk and the repository's `q` package from `domlibs/domqlib` on its normal
Python import path. CMake 3.20+ is required for the build integration.

From `dombind/`:

```sh
python domnodebind/generate.py /tmp/include/domnodebind/capi.h
cmake -S domnodebind -B /tmp/domnodebind-build
cmake --build /tmp/domnodebind-build
cmake --install /tmp/domnodebind-build --prefix /tmp/domnodebind-install
```

```cmake
# Source tree:
add_subdirectory(path/to/dombind/domnodebind domnodebind-build)
target_link_libraries(host PRIVATE DomNodeBind::DomNodeBind)
# Or, after installation:
# find_package(DomNodeBind CONFIG REQUIRED)
```

`#include <domnodebind/capi.h>` for C or
`#include <domnodebind/domnodebind.hpp>` for C++. To generate an individual
module, add `--module NAME`; `--prefix custom` renames that module's functions
and types. Aggregate imports intentionally retain each submodule's own prefix.

## Modules

| Module | Operations (prepend the module name and `_`) |
|---|---|
| `nodearray` | `new`, `size`, `get`, `set`, `buffer_copy` |
| `nodecall` | `new`, `args`, `invoke`, `construct` |
| `nodeconv` | `from_int64`, `to_int64`, `from_double`, `to_double`, `from_boolean`, `to_boolean`, `string`, `string_copy`, `bigint`, `bigint_value` |
| `nodeerr` | `throw`, `pending`, `take` |
| `nodeobj` | `new`, `get`, `set`, `type`, `wrap`, `unwrap` |
| `noderef` | `retain`, `get`, `release`, `scope_open`, `scope_close` |

`nodebind` imports every module. The source `.q` files document the exact
native signatures; runtime error conventions are preserved.

## Ownership and runtime contract

- Every fallible C wrapper returns `napi_status`; check it before using outputs. JavaScript exceptions remain pending until explicitly taken through `nodeerr_take`.
- `napi_value` is borrowed within its current handle scope. `noderef_retain` creates a strong reference to a supported reference value (typically an object/function); delete it with `noderef_release` before environment teardown. Box primitive values in objects when persistence is needed.
- String/buffer APIs use explicit lengths. `nodearray_buffer_copy` copies data into Node-managed storage. `nodeconv_from_int64` produces a JavaScript Number and can lose precision; use the BigInt helpers for exact 64-bit integers and inspect the lossless flag on conversion back.
- `nodeobj_wrap` associates native state and a finalizer with a JS object. The caller defines ownership in the finalizer; no C++ exception may escape it.
- C++ `reference` and `scope` provide RAII. Wrap the entire native callback body in `domnodebind::callback` to translate C++ exceptions without replacing a pending JavaScript exception.
- Node owns the environment. Objects are confined to the environment's JavaScript thread and lifetime; worker-thread dispatch and asynchronous callbacks require Node's thread-safe-function API, which is outside this wrapper surface.
- `nodemodule_define(name, init)` emits the Node-API registration entry point. `domnodebind_add_module` creates a `.node` CMake target; it is also available from the installed package.

## Example

See `examples/addon.cpp`:

```cpp
#include <domnodebind/domnodebind.hpp>
static napi_value init(napi_env env, napi_value exports) {
    return domnodebind::callback(env, [&]() {
        napi_value answer;
        domnodebind::check(nodeconv_from_int64(env, 42, &answer));
        domnodebind::check(nodeobj_set(env, exports, "answer", answer));
        return exports;
    });
}
nodemodule_define(answer, init)
```

```cmake
domnodebind_add_module(answer examples/addon.cpp)
```

Load the resulting module with `require('./answer.node').answer`.

## Tests

From `dombind/`, run `python -m unittest discover -s tests -v`. The suite
compiles each module and a custom-prefix instantiation in C and C++, compiles
the C++ ownership layer, and executes real runtime or shared-library examples.
Missing development toolchains are reported as skips. See `../tests/README.md`.
