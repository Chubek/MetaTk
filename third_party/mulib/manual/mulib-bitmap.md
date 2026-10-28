# MuLIB bitmap: Reference Manual

**Header:** `include/mulib-bitmap.h` · **Switch:** `MULIB_IMPL_BITMAP` · **Language:** C11 (C++17-linkable)

## Purpose and design

Packed one-bit raster image with per-pixel access. Useful for monochrome masks and occupancy maps. Coordinates are zero-based; invalid reads return zero and invalid writes fail.

This component is intended to be selected independently from MuLIB's other headers. It does not force clients to link a binary runtime. It follows explicit lifecycle management and uses public `mulib_` names to avoid common clashes with the C standard library and other toolkits. The implementation is intentionally small enough to audit. Prefer using this module in code where its documented representation, algorithmic behavior, and error contract match the workload rather than treating it as an automatic replacement for a large specialized dependency.

## Getting started

```c
#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_BITMAP
#include "mulib-bitmap.h"
int main(void){
  mulib_bitmap_t b; assert(mulib_bitmap_init(&b,10,10)); assert(mulib_bitmap_put(&b,3,4,1)); assert(mulib_bitmap_get(&b,3,4)); mulib_bitmap_free(&b);
  return 0;
}
```

Compile a one-file example with `cc -std=c11 -Iinclude example.c -o example` (add `-lm` for mathematical functions when required). To share this header across translation units, put its `#define MULIB_IMPL_BITMAP` in exactly one `.c` or `.cpp` file. Every other translation unit includes the header with no implementation switch. Defining it in several translation units causes duplicate definitions at link time. Defining a switch after the header has already been included in a translation unit is not supported.

## Public declarations

```c
typedef struct mulib_bitmap { unsigned char *pixels; size_t width,height; } mulib_bitmap_t;
int mulib_bitmap_init(mulib_bitmap_t*b,size_t width,size_t height);
int mulib_bitmap_get(const mulib_bitmap_t*b,size_t x,size_t y);
int mulib_bitmap_put(mulib_bitmap_t*b,size_t x,size_t y,int on);
void mulib_bitmap_free(mulib_bitmap_t*b);
```

The declarations are stable C entry points, with `extern "C"` linkage when compiled as C++. Public data structures are intentionally visible and inspectable rather than hiding allocation behind an ABI. Treat any `data`, `items`, or linked pointers as borrowed views unless the specific function explicitly transfers ownership. No function is implicitly thread-safe for mutable shared objects.

## Ownership, errors, and lifetime

Useful for monochrome masks and occupancy maps. Coordinates are zero-based; invalid reads return zero and invalid writes fail. Values returned through output parameters are only guaranteed after a successful call. For functions returning an integer success code, **1** means success and **0** means failure unless a more specific result is documented by the API. Do not infer a `errno` value from an API that does not explicitly promise one. For heap-allocated state, pair creation or initialization with its corresponding destroy or free operation. Never reuse internal buffer pointers after an operation that can grow or release storage.

## Behavior, constraints, and interoperability

Operations are designed around bounded input, explicit lengths where relevant, and ordinary C ownership. They do not perform locking and do not provide a global allocator override. File, terminal and parser functions are intended as lightweight building blocks; inputs that require complete standard compliance, hostile-input hardening or advanced Unicode processing may need a dedicated mature library. Confirm prerequisites and endianness for binary serialization; never serialize the memory layout of MuLIB structures directly.

## Example-driven validation

The examples folder demonstrates the overall build pattern, and the test suite exercises representative successful and unsuccessful operations. Compile with warnings and run with AddressSanitizer/UndefinedBehaviorSanitizer for new integrations. Keep tests for empty inputs, repeated updates, integer bounds, and failure behavior when extending a module. The implementation uses standard C runtime functions and deliberately keeps dependencies discoverable rather than embedding hidden platform-specific services.

## Implementation guidance

The source implementation is appended in the header below the public interface guard. It is included only if `MULIB_IMPL_BITMAP` is defined before inclusion. This arrangement allows separate declaration-only compilation and a single translation unit that emits the definitions. API additions should include an unambiguous ownership rule, Doxygen documentation, a regression test, and an updated entry in the root `MANIFEST.yaml`. See the suite's `docs/`-style manuals in `manual/` for sibling modules and `README.md` for integration into existing projects.
