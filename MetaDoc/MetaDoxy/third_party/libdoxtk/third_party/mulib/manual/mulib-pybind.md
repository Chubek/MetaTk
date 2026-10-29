# MuLIB pybind

**Header:** `include/mulib-pybind.h`  
**Implementation switches:** `MULIB_IMPL_PYBIND`, `MULIB_IMPL_STRBUF`  
**Dependencies:** MuLIB strbuf and ISO C11 only.

## Scope

This module **generates Python ctypes source**. It does not embed Python, load
a library in the C generator, discover signatures, or link to a Python SDK,
interpreter, or runtime library. Only running the emitted source requires
Python 3 with its standard-library ctypes and the described shared library.
Generating source, including the example below, requires neither.

Each descriptor specifies a native C calling-convention function, an exported
Python identifier, its native symbol, its result type, and its argument types.
Generated source imports ctypes and os under reserved `_mulib_` names, loads one
library with `CDLL`, and exposes typed ctypes callables as module globals. It
sets both `restype` and `argtypes`, including empty argument lists. All emitted
source is ASCII. Library paths are byte-escaped literals decoded with
`os.fsdecode`; native symbol lookup uses escaped bytes directly, preserving
non-UTF-8 symbol names. Neither is interpolated executable source.
Library existence and symbol availability are checked only when the emitted
module is loaded; normal ctypes exceptions propagate at that point.

Unsupported: Python embedding, extension-module compilation, C declaration
parsing, structs/unions by value, arrays by value, callbacks, variadic functions,
wide strings, long double, C++ name discovery, and non-default calling conventions.
Describe pointed-to storage as POINTER and manage it explicitly. A wrong ABI
description can crash the Python process; validation cannot verify a native
library's actual ABI or make untrusted native code safe.

## API

The common descriptors and validator are also used by rubybind:

```c
typedef struct mulib_bind_function {
    const char *name;                  /* borrowed exported identifier */
    const char *symbol;                /* borrowed; NULL means name */
    mulib_bind_type_t result;
    const mulib_bind_type_t *arguments; /* borrowed array */
    size_t argument_count;
} mulib_bind_function_t;

mulib_bind_status_t mulib_bind_validate(const mulib_bind_function_t *,
                                      size_t count, mulib_bind_error_t *);
const char *mulib_bind_status_string(mulib_bind_status_t);
mulib_bind_status_t mulib_pybind_generate(const char *library,
    const mulib_bind_function_t *, size_t count,
    char **source, size_t *length, mulib_bind_error_t *error);
```

`MULIB_BIND_OK` is success. The generator requires a non-NULL `source` output;
`length` and `error` are optional. On entry it clears `*source` and `*length`
(when supplied); no partial source is returned on failure. Success transfers
one NUL-terminated, `malloc`-allocated string to the caller: release it with
`free`. Its byte length excludes NUL. Inputs remain borrowed and unmodified.
An empty function list is valid, including a NULL descriptor pointer.

`mulib_bind_error_t` contains `status`, `function_index`, and `argument_index`.
Indices are zero-based; `SIZE_MAX` means not applicable. The validator checks
all types, names, argument arrays, native-symbol text, counts, and duplicate
exported names. The generator additionally rejects Python keywords and names
beginning with `__`. Common names must match `[A-Za-z_][A-Za-z0-9_]*` and cannot
begin with `_mulib_`. Native symbols need not be C identifiers; they are looked
up literally. A NULL symbol selects the exported name; an empty symbol is invalid.

Hard bounds (inclusive) are 1024 functions, 64 arguments per function, 128 bytes
per identifier, and 4096 bytes per nonempty library path/native symbol. All
text inputs must be readable, NUL-terminated C strings. Embedded NUL cannot be
represented in a path or symbol. Names/keywords are ASCII; paths use the native
filesystem encoding. Symbol lookup still follows ctypes/platform encoding rules.

Statuses are `INVALID_ARGUMENT` (including invalid path/symbol text or missing
arrays), `INVALID_NAME`, `INVALID_TYPE`, `DUPLICATE_NAME`, `LIMIT`, and `NO_MEMORY`,
all prefixed `MULIB_BIND_`. Overlong identifiers are `INVALID_NAME`; overlong
path/symbol text is `INVALID_ARGUMENT`; excessive descriptor counts are `LIMIT`.
No `errno` contract is provided. `mulib_bind_status_string` accepts unknown codes
and returns a static fallback. No global mutable generator state is used.

## Types and ownership

| `MULIB_BIND_` suffix | Native C type | ctypes |
|---|---|---|
| `VOID` | void (result only) | None |
| `SCHAR`, `UCHAR` | signed/unsigned char | c_byte, c_ubyte |
| `SHORT`, `USHORT` | short / unsigned short | c_short, c_ushort |
| `INT`, `UINT` | int / unsigned int | c_int, c_uint |
| `LONG`, `ULONG` | long / unsigned long | c_long, c_ulong |
| `LLONG`, `ULLONG` | long long / unsigned long long | c_longlong, c_ulonglong |
| `FLOAT`, `DOUBLE` | float, double | c_float, c_double |
| `POINTER` | void * | c_void_p |
| `CSTRING` | borrowed NUL-terminated char * | c_char_p |

These are native C types, **not fixed-width wire types**; e.g. `long` is
platform-dependent. VOID is never a valid argument type. POINTER arguments
accept ctypes-compatible pointers/addresses/None and results are addresses or
None. No allocation is freed automatically. CSTRING arguments are bytes/None;
ctypes copies non-NULL returned strings into Python bytes, and NULL yields
None. The original returned C allocation is **not freed**. Only use CSTRING
for borrowed string results; use POINTER plus an explicit native deallocator
when the result transfers ownership. Returned strings must remain readable
through the copy. Python argument/range conversion follows ctypes rules.

## Runnable C example

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PYBIND
#include "mulib-pybind.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    const mulib_bind_type_t args[] = { MULIB_BIND_INT, MULIB_BIND_INT };
    const mulib_bind_function_t fn = { "add", "demo_add", MULIB_BIND_INT, args, 2 };
    char *source;
    size_t length;
    assert(mulib_pybind_generate("./libdemo.so", &fn, 1,
                                &source, &length, NULL) == MULIB_BIND_OK);
    assert(length == strlen(source));
    assert(strstr(source, "add.argtypes"));
    free(source);
    return 0;
}
```

Compile with `cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude example.c -o example`.
The repository's `examples/modules/pybind.c` additionally writes the generated
source to stdout. With a real library exporting `int demo_add(int,int)`, save
that output as `bindings.py`, then `import bindings; bindings.add(20, 22)` yields
42. This last, optional step needs Python and the library; the C example does not.

## Tests

`tests/test_runtime.c` tests common validation, reserved/duplicate names,
malformed/VOID types, count/text limits, source escaping, and empty lists.
Compile and run using only C:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/test_runtime.c -o ./test-runtime
./test-runtime
```

Optional interpreter integration uses the same file as a native fixture:

```sh
cc -std=c11 -fPIC -shared -DMULIB_RUNTIME_BINDING_FIXTURE tests/test_runtime.c -o ./runtime.so
./test-runtime ./bindings.py ./bindings.rb ./runtime.so
python3 -c 'import runpy; b=runpy.run_path("./bindings.py"); assert b["add"](20,22)==42'
```

No dependency implementation switch is defined implicitly. Put all switches
before any includes in exactly one translation unit. Other translation units
include declarations only; those declarations support C++17 with C linkage.
