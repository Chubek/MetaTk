# MuLIB base64: Reference Manual

**Header:** `include/mulib-base64.h` · **Switch:** `MULIB_IMPL_BASE64` · **Language:** C11 (C++17-linkable)

## Purpose and design

RFC 4648 padded Base64 encoder and strict decoder. Encoder requires a destination buffer including one final NUL. Decoder accepts padded standard alphabet only (not URL-safe alphabet or whitespace); errors do not guarantee that the output buffer was left untouched.

This component is intended to be selected independently from MuLIB's other headers. It does not force clients to link a binary runtime. It follows explicit lifecycle management and uses public `mulib_` names to avoid common clashes with the C standard library and other toolkits. The implementation is intentionally small enough to audit. Prefer using this module in code where its documented representation, algorithmic behavior, and error contract match the workload rather than treating it as an automatic replacement for a large specialized dependency.

## Getting started

```c
#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_BASE64
#include "mulib-base64.h"
int main(void){
  char s[16]; assert(mulib_base64_encode("Hi",2,s,sizeof s)==4); assert(!strcmp(s,"SGk="));
  return 0;
}
```

Compile a one-file example with `cc -std=c11 -Iinclude example.c -o example` (add `-lm` for mathematical functions when required). To share this header across translation units, put its `#define MULIB_IMPL_BASE64` in exactly one `.c` or `.cpp` file. Every other translation unit includes the header with no implementation switch. Defining it in several translation units causes duplicate definitions at link time. Defining a switch after the header has already been included in a translation unit is not supported.

## Public declarations

```c
size_t mulib_base64_encode(const void*data,size_t n,char*out,size_t capacity);
int mulib_base64_decode(const char*data,size_t n,unsigned char*out,size_t capacity,size_t*written);
```

The declarations are stable C entry points, with `extern "C"` linkage when compiled as C++. Public data structures are intentionally visible and inspectable rather than hiding allocation behind an ABI. Treat any `data`, `items`, or linked pointers as borrowed views unless the specific function explicitly transfers ownership. No function is implicitly thread-safe for mutable shared objects.

## Ownership, errors, and lifetime

Encoder requires a destination buffer including one final NUL. Decoder accepts
padded standard alphabet only (not URL-safe alphabet or whitespace), including
canonical zero padding bits. It validates all input and output capacity before
writing, so failure leaves output and optional `written` unchanged. Exact in-place
decoding is supported; other overlap is not promised. NULL source/output is valid
at length zero. Decoder returns 1 on success, 0 on failure; no errno contract.

## Behavior, constraints, and interoperability

Operations are designed around bounded input, explicit lengths where relevant, and ordinary C ownership. They do not perform locking and do not provide a global allocator override. File, terminal and parser functions are intended as lightweight building blocks; inputs that require complete standard compliance, hostile-input hardening or advanced Unicode processing may need a dedicated mature library. Confirm prerequisites and endianness for binary serialization; never serialize the memory layout of MuLIB structures directly.

## Example-driven validation

The examples folder demonstrates the overall build pattern, and the test suite exercises representative successful and unsuccessful operations. Compile with warnings and run with AddressSanitizer/UndefinedBehaviorSanitizer for new integrations. Keep tests for empty inputs, repeated updates, integer bounds, and failure behavior when extending a module. The implementation uses standard C runtime functions and deliberately keeps dependencies discoverable rather than embedding hidden platform-specific services.

## Implementation guidance

The source implementation is appended in the header below the public interface guard. It is included only if `MULIB_IMPL_BASE64` is defined before inclusion. This arrangement allows separate declaration-only compilation and a single translation unit that emits the definitions. API additions should include an unambiguous ownership rule, Doxygen documentation, a regression test, and an updated entry in the root `MANIFEST.yaml`. See the suite's `docs/`-style manuals in `manual/` for sibling modules and `README.md` for integration into existing projects.
