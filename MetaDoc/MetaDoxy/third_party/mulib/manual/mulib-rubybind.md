# MuLIB rubybind

**Header:** `include/mulib-rubybind.h`  
**Implementation switches:** `MULIB_IMPL_RUBYBIND`, `MULIB_IMPL_PYBIND`, `MULIB_IMPL_STRBUF`  
**Dependencies:** MuLIB pybind's shared descriptors/validator, strbuf, ISO C11.

## Scope

This is a **Ruby Fiddle source generator**, not a Ruby embedding API. The C
generator has no Ruby, Python, libffi, SDK, or other external-library dependency.
It does not load the target library. Only executing the emitted code requires
Ruby with standard-library Fiddle and the native shared library.

Output is an ASCII-only Ruby module `MuLIBBindings` with a retained library
handle, a frozen `FUNCTIONS` lookup table, and singleton methods with explicit
argument counts. Every native function receives its declared Fiddle argument
and result types. Path/symbol literals escape every byte as `\xHH` and are
binary Ruby strings, preventing quote/backslash/newline/interpolation injection.
Library/symbol lookup happens only when the emitted module is loaded and
propagates ordinary Fiddle exceptions; generation does not verify availability.

Only native, nonvariadic C calling convention functions are supported. There
is no C parser, Ruby extension compiler, embedded interpreter, C++ demangling,
struct/union-by-value support, callback generation, or ownership inference.
An incorrect signature can crash Ruby; native code is not sandboxed.

## API and errors

```c
mulib_bind_status_t mulib_rubybind_generate(const char *library,
    const mulib_bind_function_t *functions, size_t count,
    char **source, size_t *length, mulib_bind_error_t *error);
```

All common types, status codes, limits, ownership rules, and the
`mulib_bind_validate` / `mulib_bind_status_string` API are documented in
[`mulib-pybind.md`](mulib-pybind.md). They are declared by the dependency header
and are directly usable here. No extra module or descriptor conversion is
required to generate both languages from one signature list.

`MULIB_BIND_OK` transfers a malloc-owned NUL-terminated source string through
the required `source` output. `free` it. `length` excludes NUL and is optional.
Both outputs are cleared on failure; `error` is optional and contains the
status plus zero-based function/argument indices (`SIZE_MAX` if inapplicable).
Input descriptors are borrowed, not retained. A NULL list with count zero is
valid. Text inputs must be valid NUL-terminated C strings. Library paths and
native symbols must be nonempty, at most 4096 bytes. NULL symbols use the name.

Ruby exported names additionally must start with `[a-z_]` and must not be Ruby
keywords, numbered-parameter names `_1` through `_9`, magic tokens such as
`__FILE__`, or the `singleton_method_added`,
`singleton_method_removed`, and `singleton_method_undefined` hooks (which could
otherwise invoke a native function while loading/altering the module). Remaining bytes are ASCII
letters/digits/underscores, maximum 128 bytes. The `_mulib_` prefix is reserved.
Duplicate names, invalid/VOID argument types, more than 64 arguments, and more
than 1024 functions are rejected, just as in pybind. No partial source is
returned; allocation failure is `MULIB_BIND_NO_MEMORY`. The module has no
global mutable state or `errno` contract.

## ABI mapping

`MULIB_BIND_VOID` maps to `Fiddle::TYPE_VOID` (results only). SCHAR/UCHAR,
SHORT/USHORT, INT/UINT, LONG/ULONG, and LLONG/ULLONG map to Fiddle's CHAR,
SHORT, INT, LONG, and LONG_LONG types, with negative type constants for unsigned
variants. FLOAT and DOUBLE use their corresponding Fiddle types. These describe
**native C types**, not fixed-width serialized types.

POINTER maps to `TYPE_VOIDP`, accepts Fiddle-compatible addresses/pointers/nil,
and returns a `Fiddle::Pointer`. CSTRING uses the same ABI type but accepts
String/nil and converts the returned pointer immediately to a copied String,
or nil when NULL. The generator does not free native memory. Only use CSTRING
results for borrowed NUL-terminated storage that remains valid during the copy.
For an owned native string, bind it as POINTER and separately describe its
deallocator. Conversions and numeric range errors otherwise follow Fiddle.
The loaded library must match the Ruby process's architecture and ABI.

## Runnable C example

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PYBIND
#define MULIB_IMPL_RUBYBIND
#include "mulib-rubybind.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    const mulib_bind_type_t args[] = { MULIB_BIND_INT, MULIB_BIND_INT };
    const mulib_bind_function_t fn = { "add", "demo_add", MULIB_BIND_INT, args, 2 };
    char *source;
    assert(mulib_rubybind_generate("./libdemo.so", &fn, 1,
                                  &source, NULL, NULL) == MULIB_BIND_OK);
    assert(strstr(source, "def self.add(a0, a1)"));
    free(source);
    return 0;
}
```

Build with `cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude example.c -o example`.
No Ruby installation or demo library is necessary for this example. The full
`examples/modules/rubybind.c` writes the source to stdout. Saving its output as
`bindings.rb` permits `require_relative 'bindings'; MuLIBBindings.add(20, 22)`
when Ruby/Fiddle and a real `int demo_add(int,int)` shared library are available.
Generated modules use the fixed namespace `MuLIBBindings`; do not load multiple
independent generated files into that same namespace in one Ruby process.

## Validation

The default `tests/test_runtime.c` test is C-only and exercises both generators,
including escaping Ruby interpolation-like text, names, malformed types, and
limits. See pybind's manual for the C fixture/source-generation commands.
After that optional step, Ruby integration can be checked with:

```sh
ruby -c ./bindings.rb
ruby -r ./bindings.rb -e 'raise unless MuLIBBindings.add(20,22)==42'
```

Define dependency implementation switches explicitly before all includes in
exactly one translation unit. The header never turns them on itself.
Declaration-only C++17 inclusion is supported through `extern "C"`.
