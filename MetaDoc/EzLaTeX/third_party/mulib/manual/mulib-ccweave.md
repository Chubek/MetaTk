# mulib-ccweave

Standalone checked C11 source generation for the common MIR, not a C parser or
general source-to-source compiler. Every opcode in `manual/mulib-mir.md` is
implemented; no extra control flow, pointers, calls or memory operations are
accepted.

## Integration and output

Include `mulib-ccweave.h`. Explicitly implement MIR, STRBUF and CCWEAVE with
`MULIB_IMPL_MIR`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_CCWEAVE`, each exactly once.
The header never enables another module's implementation. STRBUF stores the
output text, with growth explicitly clamped to the caller's byte limit.

Initialize `char *source = NULL`, then call
`mulib_ccweave_generate(program, symbol, record_limit, byte_limit, &source,
diagnostic)`. Success returns one allocated, NUL-terminated complete translation
unit. Caller frees it with `free`. No borrowed input pointers are retained.
Failure leaves `source` NULL and frees partial text. Nonempty output is rejected.

The generated translation unit includes only standard C headers, with no MuLIB
link dependency. It defines exactly one externally visible function:

```c
int mulib_generated_name(const int64_t *args, size_t argc, int64_t *out);
```

`symbol` must start with `mulib_generated_`, have a nonempty suffix containing
only ASCII letters, digits or underscores, and be at most 127 bytes total.
This intentionally avoids C keywords, reserved identifiers and standard header
name collisions; arbitrary injected C text is rejected. The caller is responsible
for uniqueness across generated functions and for avoiding conflicting macros.

## Generated semantics

Status numbers equal the public `mulib_mir_status_t` enumerators in the generating
header. Zero is success. Runtime errors return INVALID for NULL output, wrong
argument count or NULL nonempty argument array; BAD_TYPE for a noncanonical
Boolean argument; OVERFLOW and DIVIDE_BY_ZERO for arithmetic faults. `*out` is
unchanged on all errors. There is no generated source map or diagnostic object.

The function first validates argument shape and all Boolean PARAM values, then
evaluates records in order, including unused arithmetic. Each SSA value becomes
an `int64_t` local. Safe preconditions guard every ADD/SUB/MUL/NEG and DIV/MOD;
only after checks does C perform the operation. Minimum signed literals use
`INT64_MIN`; other literals use standard integer-constant macros. EQ/LT/LE/NOT
produce 0/1. RETURN writes one result. Thus no supported operation depends on
signed overflow, unchecked division, implementation-defined narrowing, or a
compiler's `-fwrapv` behavior. Argument/result aliasing is safe because the
only result write occurs after all argument reads.

## Bounds and diagnostics

Common MIR validation occurs first and returns its exact diagnostic on invalid
input. The input record count is bounded by `record_limit`. `byte_limit` includes
the trailing NUL and bounds **both output length and backing allocation**. Zero
is zero, not unlimited. Capacity grows geometrically but never above that bound;
addition/size arithmetic is checked. Temporary formatting uses a fixed 2048-byte
stack buffer. There are no dynamic per-record scratch allocations.

RESOURCE_LIMIT means record/output/format size limits; NO_MEMORY means allocator
failure; INVALID includes an invalid symbol/output descriptor. Generation is
O(input records + emitted bytes); no compiler subprocess runs inside the API.
Generated execution is not step-limited because this subset has no loops/calls.

## Runnable example and generated-source verification

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/ccweave.c -o /tmp/ccweave
/tmp/ccweave > /tmp/generated.c
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -c /tmp/generated.c -o /tmp/generated.o
```

The assert example emits a function returning 42. For a generated executable
covering every scalar operation and checked arithmetic failures:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/test_compiler_ir.c -o /tmp/compiler-ir
/tmp/compiler-ir --emit > /tmp/generated-tests.c
cc -std=c11 -Wall -Wextra -Wpedantic -Werror /tmp/generated-tests.c -o /tmp/generated-tests
/tmp/generated-tests
```
