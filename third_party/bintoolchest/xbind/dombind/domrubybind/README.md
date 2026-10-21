# domrubybind

CRuby lifecycle, GC roots, conversions, arrays, hashes, protected calls and modules. Like `dompybind`, this library uses Q templates with per-module
`PREFIX` parameters, guarded `static inline` C functions and an aggregate
`rubybind.q` with explicit import prefixes. Its C++17 ownership helpers live in
`include/domrubybind/domrubybind.hpp`.

## Build

Requires CRuby 3.0+ development headers and libraries. Header generation additionally requires Python with
Tcl/Tk and the repository's `q` package from `domlibs/domqlib` on its normal
Python import path. CMake 3.20+ is required for the build integration.

From `dombind/`:

```sh
python domrubybind/generate.py /tmp/include/domrubybind/capi.h
cmake -S domrubybind -B /tmp/domrubybind-build
cmake --build /tmp/domrubybind-build
cmake --install /tmp/domrubybind-build --prefix /tmp/domrubybind-install
```

```cmake
# Source tree:
add_subdirectory(path/to/dombind/domrubybind domrubybind-build)
target_link_libraries(host PRIVATE DomRubyBind::DomRubyBind)
# Or, after installation:
# find_package(DomRubyBind CONFIG REQUIRED)
```

`#include <domrubybind/capi.h>` for C or
`#include <domrubybind/domrubybind.hpp>` for C++. To generate an individual
module, add `--module NAME`; `--prefix custom` renames that module's functions
and types. Aggregate imports intentionally retain each submodule's own prefix.

## Modules

| Module | Operations (prepend the module name and `_`) |
|---|---|
| `rubyarray` | `new`, `size`, `get`, `set`, `push`, `pop` |
| `rubycall` | `invoke`, `eval`, `protect`, `error`, `clear_error` |
| `rubyconv` | `from_int64`, `to_int64`, `from_double`, `to_double`, `from_boolean`, `string`, `string_data`, `string_size` |
| `rubyhash` | `new`, `get`, `set`, `remove` |
| `rubyinit` | `setup`, `cleanup`, `loadpath` |
| `rubymodule` | `new`, `constant` |
| `rubyobj` | `retain`, `release`, `is_nil`, `truth`, `inspect`, `class_of` |

`rubybind` imports every module. The source `.q` files document the exact
native signatures; runtime error conventions are preserved.

## Ownership and runtime contract

- Embedding entry points first call `ruby_sysinit(&argc, &argv)` and `RUBY_INIT_STACK`, then `rubyinit_setup`. Initialize the load path if needed. Destroy every root before `rubyinit_cleanup`.
- Calls and conversions can raise through Ruby's nonlocal control flow. Use `rubycall_protect` or `rubycall_eval`; inspect the returned state, retain `rubycall_error()` if needed, and clear it after handling.
- `rubyobj_retain` registers the *address* of a VALUE slot. Its address must stay stable until `rubyobj_release`; copies of a VALUE alone are not persistent roots.
- Strings use explicit lengths. Pointers from `rubyconv_string_data` are borrowed and can be invalidated by mutation or collection. Root the string and copy bytes before releasing the GVL.
- C++ `object` owns a stable heap root. `eval` and `protect` translate Ruby failures into `error`, which roots the original exception. The function passed to `protect` must not create C++ objects needing destruction across a Ruby jump, or throw a C++ exception through Ruby frames.
- All APIs require the GVL on a Ruby-registered thread. Ruby owns runtime initialization and shutdown for extension modules. Register variable-arity module functions with `rubymodule_function`; callback signature is `VALUE fn(int argc, VALUE *argv, VALUE self)`.
- The library does not automatically wrap arbitrary C++ classes or manage GVL handoffs.

## Example

See `examples/embed.cpp`:

```cpp
#include <domrubybind/domrubybind.hpp>
int main(int argc, char **argv) {
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK;
    if (rubyinit_setup() != 0) return 1;
    int status = 0;
    {
        try {
            auto result = domrubybind::eval("6 * 7");
            status = rubyconv_to_int64(result.get()) == 42 ? 0 : 1;
        } catch (const domrubybind::error &) { status = 1; }
    }
    return rubyinit_cleanup(status);
}
```

## Tests

From `dombind/`, run `python -m unittest discover -s tests -v`. The suite
compiles each module and a custom-prefix instantiation in C and C++, compiles
the C++ ownership layer, and executes real runtime or shared-library examples.
Missing development toolchains are reported as skips. See `../tests/README.md`.
