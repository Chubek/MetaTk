# mulib-mir

Typed record IR shared by `cmachine`, `vmweave`, and `ccweave`. This is a focused
straight-line SSA language, **not** a control-flow IR, parser, ABI, or object format.

## Integration and ownership

Include `mulib-mir.h`. Define `MULIB_IMPL_MIR` in exactly one translation unit.
There are no implementation dependencies. Declarations have C linkage in C++.

`mulib_mir_program_t` borrows immutable `code[count]`. Value IDs are record
indices, not pointers. Its lifetime must cover validation and any transformation.
It owns nothing. `parameter_count` is the exact argument-array length expected
by backends, including unused parameters.

`mulib_mir_buffer_t` owns a fixed-capacity record allocation. Zero initialize it,
call `mulib_mir_buffer_init(&b, capacity, parameters)`, append copied records with
`mulib_mir_append`, borrow a descriptor with `mulib_mir_view`, then call
`mulib_mir_buffer_free`. Do not copy a live owner, reinitialize it, or change its
capacity/pointer fields. Free zeroes the object and accepts NULL. Appending does
not validate; a partially built program is not yet executable. Capacity zero is
valid, but cannot accept an append. All allocations use the C allocator.

## Exact language

Every record has `opcode`, declared result `type`, operands `a`, `b`, and signed
`immediate`. Unused operands must equal `MULIB_MIR_NONE` (`SIZE_MAX`). Only CONST
and PARAM may have nonzero immediate. Every used operand must reference an
**earlier** record: forward references, self references, and cycles are rejected.

| Opcode | Operands / immediate | Result |
|---|---|---|
| CONST | immediate literal | I64 or BOOL; BOOL literal is exactly 0 or 1 |
| PARAM | immediate argument index, nonnegative and below parameter_count | I64 or BOOL |
| COPY | a | same type as a |
| ADD SUB MUL DIV MOD | a, b both I64 | I64 |
| NEG | a I64 | I64 |
| EQ | a, b with equal I64 or BOOL types | BOOL |
| LT LE | a, b I64 | BOOL |
| NOT | a BOOL | BOOL |
| RETURN | a I64 or BOOL | VOID |

There must be exactly one RETURN, at the final record. Nothing else is VOID.
No branches, loops, phi nodes, addresses, loads/stores, calls, floating-point,
implicit conversions, or external side effects are supported. Unsupported
opcodes/encodings produce diagnostics rather than disappearing.

Integer semantics are signed 64-bit with explicit overflow faults. DIV/MOD use
C11 truncation toward zero. Both `INT64_MIN / -1` and `INT64_MIN % -1` fault;
division by zero is distinct. NEG of `INT64_MIN` faults. BOOL values are canonical
0/1. Backends validate all Boolean PARAM arguments before arithmetic executes.
Records otherwise execute in their original order, including dead arithmetic:
an unused overflowing ADD still faults.

## Validation and diagnostics

`mulib_mir_validate(program, record_limit, diagnostic)` is allocation-free and
linear. It checks the entire input, not only returned dependencies. Zero records
are INVALID. A record count above the explicit limit, or one whose byte size
cannot be represented, returns RESOURCE_LIMIT. On failure, `diagnostic.record`
identifies the first offending record (zero for descriptor/limit failures).
The record index on success is not an error location and should be ignored.
`mulib_mir_status_string` returns static, non-owned diagnostic text.

Statuses distinguish INVALID descriptors/immediates, BAD_OPCODE,
BAD_REFERENCE (including cycles), BAD_TYPE, BAD_RETURN, RESOURCE_LIMIT,
NO_MEMORY, OVERFLOW and DIVIDE_BY_ZERO. The last two are evaluation faults;
validation does not execute arithmetic.

`mulib_mir_arity` returns 0/1/2, or -1 for an invalid opcode.
`mulib_mir_evaluate(op,a,b,immediate,&value)` is a checked scalar evaluator for
CONST, COPY, arithmetic, and comparisons/NOT. PARAM and RETURN are invalid in
this API. It does not validate operand types; first validate typed records.
Failure leaves `value` unchanged. There is no implicit error-global state.

## Bounds and composition

Builder allocation is exactly `capacity * sizeof(mulib_mir_instruction_t)`;
multiplication is checked before allocation. Appending to a full builder returns
RESOURCE_LIMIT without changing it. NO_MEMORY also preserves an empty builder.
Limits of zero mean zero, not unlimited. Callers must provide valid readable
arrays; C descriptors cannot establish the physical size of an arbitrary pointer.

`cmachine` lowers every supported opcode to checked `minivm`. `vmweave` returns
another owned MIR buffer with the same argument contract. `ccweave` emits an
independent C11 implementation of the same semantics. Generic e-graphs and
selectors can use MIR opcode numbers, but their clients must explicitly map
operands/types and preserve fault ordering; they are not silently typed MIR.

## Runnable example

`examples/modules/mir.c` constructs and validates a literal/return pair, then
asserts rejection of a self reference:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/mir.c -o /tmp/mir
/tmp/mir
```

`tests/test_compiler_ir.c` exercises malformed types/references/opcodes, empty
input, capacities, arithmetic boundaries, and intermodule execution equivalence.
