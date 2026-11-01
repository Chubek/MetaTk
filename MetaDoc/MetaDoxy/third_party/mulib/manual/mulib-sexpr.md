# MuLIB sexpr: Reference Manual

**Header:** `include/mulib-sexpr.h` · **Switch:** `MULIB_IMPL_SEXPR` · **Language:** C11 (C++17-linkable)

## Purpose and design

A tree-based S-expression parser with atoms, lists, and deterministic rendering. The parser accepts proper nested lists, unquoted atoms, and double-quoted atoms with basic escapes. It has a recursion-depth limit of 128. The returned tree owns all atom strings; destroy it recursively. Parse does not enforce end-of-input; consult the optional end pointer.

This component is intended to be selected independently from MuLIB's other headers. It does not force clients to link a binary runtime. It follows explicit lifecycle management and uses public `mulib_` names to avoid common clashes with the C standard library and other toolkits. The implementation is intentionally small enough to audit. Prefer using this module in code where its documented representation, algorithmic behavior, and error contract match the workload rather than treating it as an automatic replacement for a large specialized dependency.

## Getting started

```c
#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_SEXPR
#include "mulib-sexpr.h"
int main(void){
  mulib_sexpr_t*s=mulib_sexpr_parse("(a (b c))",NULL); assert(s); char*t=mulib_sexpr_render(s); assert(t); free(t); mulib_sexpr_destroy(s);
  return 0;
}
```

Compile a one-file example with `cc -std=c11 -Iinclude example.c -o example` (add `-lm` for mathematical functions when required). To share this header across translation units, put its `#define MULIB_IMPL_SEXPR` in exactly one `.c` or `.cpp` file. Every other translation unit includes the header with no implementation switch. Defining it in several translation units causes duplicate definitions at link time. Defining a switch after the header has already been included in a translation unit is not supported.

## Public declarations

```c
typedef enum mulib_sexpr_kind { MULIB_SEXPR_ATOM=1, MULIB_SEXPR_LIST=2 } mulib_sexpr_kind_t;
typedef struct mulib_sexpr { mulib_sexpr_kind_t kind; char *atom; struct mulib_sexpr *child,*next; } mulib_sexpr_t;
mulib_sexpr_t *mulib_sexpr_cont(void);
mulib_sexpr_t *mulib_sexpr_parse(const char*source,const char**end);
int mulib_sexpr_push(mulib_sexpr_t*list,mulib_sexpr_t*value);
char *mulib_sexpr_render(const mulib_sexpr_t*value);
void mulib_sexpr_destroy(mulib_sexpr_t*value);
```

The declarations are stable C entry points, with `extern "C"` linkage when compiled as C++. Public data structures are intentionally visible and inspectable rather than hiding allocation behind an ABI. Treat any `data`, `items`, or linked pointers as borrowed views unless the specific function explicitly transfers ownership. No function is implicitly thread-safe for mutable shared objects.

## Ownership, errors, and lifetime

The parser accepts proper nested lists, unquoted atoms, and double-quoted atoms with basic escapes. It has a recursion-depth limit of 128. The returned tree owns all atom strings; destroy it recursively. Parse does not enforce end-of-input; consult the optional end pointer. Values returned through output parameters are only guaranteed after a successful call. For functions returning an integer success code, **1** means success and **0** means failure unless a more specific result is documented by the API. Do not infer a `errno` value from an API that does not explicitly promise one. For heap-allocated state, pair creation or initialization with its corresponding destroy or free operation. Never reuse internal buffer pointers after an operation that can grow or release storage.

## Behavior, constraints, and interoperability

Operations are designed around bounded input, explicit lengths where relevant, and ordinary C ownership. They do not perform locking and do not provide a global allocator override. File, terminal and parser functions are intended as lightweight building blocks; inputs that require complete standard compliance, hostile-input hardening or advanced Unicode processing may need a dedicated mature library. Confirm prerequisites and endianness for binary serialization; never serialize the memory layout of MuLIB structures directly.

## Example-driven validation

The examples folder demonstrates the overall build pattern, and the test suite exercises representative successful and unsuccessful operations. Compile with warnings and run with AddressSanitizer/UndefinedBehaviorSanitizer for new integrations. Keep tests for empty inputs, repeated updates, integer bounds, and failure behavior when extending a module. The implementation uses standard C runtime functions and deliberately keeps dependencies discoverable rather than embedding hidden platform-specific services.

## Implementation guidance

The source implementation is appended in the header below the public interface guard. It is included only if `MULIB_IMPL_SEXPR` is defined before inclusion. This arrangement allows separate declaration-only compilation and a single translation unit that emits the definitions. API additions should include an unambiguous ownership rule, Doxygen documentation, a regression test, and an updated entry in the root `MANIFEST.yaml`. See the suite's `docs/`-style manuals in `manual/` for sibling modules and `README.md` for integration into existing projects.
