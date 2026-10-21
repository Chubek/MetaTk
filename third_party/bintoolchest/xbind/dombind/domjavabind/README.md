# domjavabind

JNI virtual machines, references, strings, arrays, calls and exceptions. Like `dompybind`, this library uses Q templates with per-module
`PREFIX` parameters, guarded `static inline` C functions and an aggregate
`javabind.q` with explicit import prefixes. Its C++17 ownership helpers live in
`include/domjavabind/domjavabind.hpp`.

## Build

Requires a JDK with JNI headers and libjvm (set JAVA_HOME when discovery needs it). Header generation additionally requires Python with
Tcl/Tk and the repository's `q` package from `domlibs/domqlib` on its normal
Python import path. CMake 3.20+ is required for the build integration.

From `dombind/`:

```sh
python domjavabind/generate.py /tmp/include/domjavabind/capi.h
cmake -S domjavabind -B /tmp/domjavabind-build
cmake --build /tmp/domjavabind-build
cmake --install /tmp/domjavabind-build --prefix /tmp/domjavabind-install
```

```cmake
# Source tree:
add_subdirectory(path/to/dombind/domjavabind domjavabind-build)
target_link_libraries(host PRIVATE DomJavaBind::DomJavaBind)
# Or, after installation:
# find_package(DomJavaBind CONFIG REQUIRED)
```

`#include <domjavabind/capi.h>` for C or
`#include <domjavabind/domjavabind.hpp>` for C++. To generate an individual
module, add `--module NAME`; `--prefix custom` renames that module's functions
and types. Aggregate imports intentionally retain each submodule's own prefix.

## Modules

| Module | Operations (prepend the module name and `_`) |
|---|---|
| `javaarray` | `size`, `bytes`, `bytes_get`, `bytes_set`, `objects`, `get`, `set` |
| `javacall` | `find_class`, `method`, `static_method`, `construct`, `register`, `object`, `integer`, `long`, `double`, `void` |
| `javaerr` | `pending`, `get`, `clear`, `throw` |
| `javainit` | `create`, `attach`, `detach`, `destroy` |
| `javaref` | `retain`, `release`, `local_release`, `frame_push`, `frame_pop`, `same` |
| `javastr` | `new`, `size`, `acquire`, `release`, `modified_utf8` |

`javabind` imports every module. The source `.q` files document the exact
native signatures; runtime error conventions are preserved.

## Ownership and runtime contract

- `javainit_create` creates a JVM from caller-supplied `JavaVMInitArgs`. Hosts own its shutdown. JNI generally does not support destroying and recreating a JVM in the same process.
- A `JNIEnv *` belongs to the current attached thread. Use attach/detach for native threads; do not detach a thread merely because it was already attached by the Java caller.
- Returned JNI object references are local unless explicitly retained with `javaref_retain`. Release global references before JVM shutdown. Local frames bound temporary references.
- JNI operations can return NULL/zero with a pending exception. Check `javaerr_pending` after fallible calls and stop ordinary JNI work until the exception is handled. Clearing an exception is an explicit caller decision.
- `javastr_new` accepts length-delimited UTF-16, including embedded NULs. Pair `javastr_acquire` with `javastr_release`. `javastr_modified_utf8` accepts JNI modified UTF-8, not arbitrary UTF-8.
- Calls take `jvalue` arrays matching the method descriptor; no C variadic promotion assumptions are involved. Primitive method wrappers cover int, long, double, void and object results. Native registration uses `JNINativeMethod`.
- C++ `global_ref` is thread-bound to its captured JNIEnv, including destruction. `local_frame` scopes local references. `check` throws a C++ error while preserving the pending Java exception. Catch C++ exceptions before returning through a JNI native callback.
- Class generation, automatic method-signature inference and JVM thread ownership are left to the host.

## Example

See `examples/embed.cpp`:

```cpp
#include <domjavabind/domjavabind.hpp>
int main() {
    JavaVM *vm = nullptr;
    JNIEnv *env = nullptr;
    JavaVMInitArgs args = {};
    args.version = JNI_VERSION_1_8;
    if (javainit_create(&vm, &env, &args) != JNI_OK) return 1;
    int status = 0;
    {
        domjavabind::local_frame frame(env);
        jchar text[] = {'o', 'k'};
        jstring str = javastr_new(env, text, 2);
        if (javaerr_pending(env)) { javaerr_clear(env); status = 1; }
        else status = javastr_size(env, str) == 2 ? 0 : 1;
    }
    return javainit_destroy(vm) == JNI_OK ? status : 1;
}
```

## Tests

From `dombind/`, run `python -m unittest discover -s tests -v`. The suite
compiles each module and a custom-prefix instantiation in C and C++, compiles
the C++ ownership layer, and executes real runtime or shared-library examples.
Missing development toolchains are reported as skips. See `../tests/README.md`.
