# MuLIB argparse: Reference Manual

**Header:** `include/mulib-argparse.h` · **Switch:** `MULIB_IMPL_ARGPARSE` · **Language:** C11 (C++17-linkable)

## Purpose and design

Lightweight GNU-style options and positional arguments parser. Parses `--key=value`, `--key value`, `-k value`, bare flags defaulting to "1", and operands after `--`. Short clusters are not expanded. Value interpretation is deliberately heuristic; a negative-number argument should be given with `=` to avoid option ambiguity. Positionals borrow argv pointers.

This component is intended to be selected independently from MuLIB's other headers. It does not force clients to link a binary runtime. It follows explicit lifecycle management and uses public `mulib_` names to avoid common clashes with the C standard library and other toolkits. The implementation is intentionally small enough to audit. Prefer using this module in code where its documented representation, algorithmic behavior, and error contract match the workload rather than treating it as an automatic replacement for a large specialized dependency.

## Getting started

```c
#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_ARGPARSE
#include "mulib-argparse.h"
int main(void){
  char *argv[]={"app","--mode=fast","file.txt"}; mulib_argparse_t*a=mulib_argparse_load(); assert(a); assert(mulib_argparse_parse(a,3,argv)); assert(!strcmp(mulib_argparse_get(a,"mode"),"fast")); mulib_argparse_destroy(a);
  return 0;
}
```

Compile a one-file example with `cc -std=c11 -Iinclude example.c -o example` (add `-lm` for mathematical functions when required). To share this header across translation units, put its `#define MULIB_IMPL_ARGPARSE` in exactly one `.c` or `.cpp` file. Every other translation unit includes the header with no implementation switch. Defining it in several translation units causes duplicate definitions at link time. Defining a switch after the header has already been included in a translation unit is not supported.

## Public declarations

```c
typedef struct mulib_argparse_option { char *key,*value; } mulib_argparse_option_t;
typedef struct mulib_argparse { mulib_argparse_option_t *items; size_t length,capacity; char **positionals; size_t positional_count; } mulib_argparse_t;
mulib_argparse_t *mulib_argparse_load(void);
int mulib_argparse_parse(mulib_argparse_t*a,int argc,char*const argv[]);
const char *mulib_argparse_get(const mulib_argparse_t*a,const char*name);
int mulib_argparse_has(const mulib_argparse_t*a,const char*name);
void mulib_argparse_destroy(mulib_argparse_t*a);
```

The declarations are stable C entry points, with `extern "C"` linkage when compiled as C++. Public data structures are intentionally visible and inspectable rather than hiding allocation behind an ABI. Treat any `data`, `items`, or linked pointers as borrowed views unless the specific function explicitly transfers ownership. No function is implicitly thread-safe for mutable shared objects.

## Ownership, errors, and lifetime

Parses `--key=value`, `--key value`, `-k value`, bare flags defaulting to "1", and operands after `--`. Short clusters are not expanded. Value interpretation is deliberately heuristic; a negative-number argument should be given with `=` to avoid option ambiguity. Positionals borrow argv pointers. Values returned through output parameters are only guaranteed after a successful call. For functions returning an integer success code, **1** means success and **0** means failure unless a more specific result is documented by the API. Do not infer a `errno` value from an API that does not explicitly promise one. For heap-allocated state, pair creation or initialization with its corresponding destroy or free operation. Never reuse internal buffer pointers after an operation that can grow or release storage.

## Behavior, constraints, and interoperability

Operations are designed around bounded input, explicit lengths where relevant, and ordinary C ownership. They do not perform locking and do not provide a global allocator override. File, terminal and parser functions are intended as lightweight building blocks; inputs that require complete standard compliance, hostile-input hardening or advanced Unicode processing may need a dedicated mature library. Confirm prerequisites and endianness for binary serialization; never serialize the memory layout of MuLIB structures directly.

## Example-driven validation

The examples folder demonstrates the overall build pattern, and the test suite exercises representative successful and unsuccessful operations. Compile with warnings and run with AddressSanitizer/UndefinedBehaviorSanitizer for new integrations. Keep tests for empty inputs, repeated updates, integer bounds, and failure behavior when extending a module. The implementation uses standard C runtime functions and deliberately keeps dependencies discoverable rather than embedding hidden platform-specific services.

## Implementation guidance

The source implementation is appended in the header below the public interface guard. It is included only if `MULIB_IMPL_ARGPARSE` is defined before inclusion. This arrangement allows separate declaration-only compilation and a single translation unit that emits the definitions. API additions should include an unambiguous ownership rule, Doxygen documentation, a regression test, and an updated entry in the root `MANIFEST.yaml`. See the suite's `docs/`-style manuals in `manual/` for sibling modules and `README.md` for integration into existing projects.
