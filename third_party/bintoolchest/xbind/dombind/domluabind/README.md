# domluabind

Lua states, stacks, conversions, tables, protected calls and registry references. Like `dompybind`, this library uses Q templates with per-module
`PREFIX` parameters, guarded `static inline` C functions and an aggregate
`luabind.q` with explicit import prefixes. Its C++17 ownership helpers live in
`include/domluabind/domluabind.hpp`.

## Build

Requires Lua 5.4+ development headers and libraries. Header generation additionally requires Python with
Tcl/Tk and the repository's `q` package from `domlibs/domqlib` on its normal
Python import path. CMake 3.20+ is required for the build integration.

From `dombind/`:

```sh
python domluabind/generate.py /tmp/include/domluabind/capi.h
cmake -S domluabind -B /tmp/domluabind-build
cmake --build /tmp/domluabind-build
cmake --install /tmp/domluabind-build --prefix /tmp/domluabind-install
```

```cmake
# Source tree:
add_subdirectory(path/to/dombind/domluabind domluabind-build)
target_link_libraries(host PRIVATE DomLuaBind::DomLuaBind)
# Or, after installation:
# find_package(DomLuaBind CONFIG REQUIRED)
```

`#include <domluabind/capi.h>` for C or
`#include <domluabind/domluabind.hpp>` for C++. To generate an individual
module, add `--module NAME`; `--prefix custom` renames that module's functions
and types. Aggregate imports intentionally retain each submodule's own prefix.

## Modules

| Module | Operations (prepend the module name and `_`) |
|---|---|
| `luacall` | `protected`, `load`, `eval`, `push`, `get_global`, `set_global` |
| `luaconv` | `push_integer`, `integer`, `push_number`, `number`, `push_boolean`, `boolean`, `push_nil`, `push_string`, `string` |
| `luainit` | `new`, `openlibs`, `close` |
| `luaref` | `retain`, `release`, `push` |
| `luastack` | `size`, `resize`, `reserve`, `absolute`, `copy`, `pop`, `type` |
| `luatable` | `new`, `get`, `set`, `get_index`, `set_index`, `size`, `next` |

`luabind` imports every module. The source `.q` files document the exact
native signatures; runtime error conventions are preserved.

## Ownership and runtime contract

- C functions preserve Lua's stack effects. Indexes and argument counts must be valid; callers reserve enough stack space with `luastack_reserve`.
- `luacall_eval` loads and calls under protection. Success leaves the requested results; failure leaves the error value on the stack and returns a Lua status code. `luacall_protected` expects the function below its arguments.
- String lengths are explicit; embedded NUL bytes are preserved. A `luaconv_string` pointer is borrowed and requires its Lua value to remain live.
- `luaref_retain` copies an indexed value into the registry without consuming it. Release each registry reference before closing its state.
- C++ `state` owns a shared state; `object` registry references keep it alive. Copies retain, moves transfer. `state::eval` throws `error` and restores the stack on a Lua error. `stack_guard` restores the stack when leaving a scope.
- Serialize access to each state. Raw operations which allocate, invoke metamethods, or raise can longjmp; protect such work at a Lua callback boundary, and do not put C++ objects requiring destruction across that jump. C++ callbacks must catch their own exceptions before returning to Lua.

## Example

See `examples/embed.cpp`:

```cpp
#include <domluabind/domluabind.hpp>
int main() {
    domluabind::state lua;
    lua.eval("return 6 * 7", 1);
    int ok = 0;
    auto result = luaconv_integer(lua.get(), -1, &ok);
    return ok && result == 42 ? 0 : 1;
}
```

## Tests

From `dombind/`, run `python -m unittest discover -s tests -v`. The suite
compiles each module and a custom-prefix instantiation in C and C++, compiles
the C++ ownership layer, and executes real runtime or shared-library examples.
Missing development toolchains are reported as skips. See `../tests/README.md`.
