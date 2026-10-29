# MuLIB minivm

**Header:** `include/mulib-minivm.h`  
**Implementation switch:** `MULIB_IMPL_MINIVM`  
**Dependencies:** ISO C11 only; no other MuLIB module or runtime library.

## Scope and model

minivm is a real, checked signed-64-bit stack interpreter. Programs are arrays
of `{ opcode, int64_t operand }` C records, not a stable on-disk bytecode format.
Each run owns a fresh stack and zero-initialized locals, has hard allocation
and instruction limits, supports absolute branches, and can call explicitly
registered host functions. There is no compiler, assembler, heap/object model,
floating point, byte-addressed memory, bytecode subroutine stack, or JIT.

All arithmetic is deterministic: signed overflow faults instead of wrapping or
invoking C undefined behavior. Division truncates toward zero; remainder has
the dividend's sign. Division and remainder by zero fault. Both `INT64_MIN / -1`
and `INT64_MIN % -1` report overflow, as does negating `INT64_MIN`.

This bounds bytecode execution, not arbitrary host callbacks. A callback can
block, allocate, or have side effects; the step limit does not interrupt it.
Programs/host arrays must be valid, immutable C storage throughout the run.
The interpreter is not a sandbox against invalid C pointers or hostile native
callbacks. It has no global mutable state; separate runs are independent.

## Program, limits, and API

```c
typedef int (*mulib_minivm_host_fn)(void *user, const int64_t *arguments,
                                  size_t count, int64_t *result);
typedef struct mulib_minivm_host {
    mulib_minivm_host_fn function;
    void *user;
    size_t argument_count;
} mulib_minivm_host_t;
typedef struct mulib_minivm_program {
    const mulib_minivm_instruction_t *code;
    size_t code_count, local_count;
    const mulib_minivm_host_t *hosts;
    size_t host_count;
} mulib_minivm_program_t;
typedef struct mulib_minivm_limits { size_t stack, locals, steps; } mulib_minivm_limits_t;

mulib_minivm_limits_t mulib_minivm_default_limits(void);
mulib_minivm_status_t mulib_minivm_run(const mulib_minivm_program_t *,
    const mulib_minivm_limits_t *, mulib_minivm_result_t *);
const char *mulib_minivm_status_string(mulib_minivm_status_t);
```

NULL limits selects 1024 stack slots, 1024 allowed locals, and 100000 steps.
An explicit zero means zero, never "unlimited". The stack reserves exactly the
configured capacity; locals allocate only `program.local_count` slots. Sizes
are checked before multiplication/allocation. A zero-slot stack can execute
HALT or jumps, but cannot push a value. There is no retained VM object to free.
Every run frees its temporary allocations, including on errors. Descriptors,
code, host callbacks, and user data remain borrowed.

`run` returns `MULIB_MINIVM_OK` only after HALT. Its optional result contains:

* `status`: same as the returned status;
* `pc`: halted/faulting instruction index, or `code_count` on fallthrough;
* `steps`: attempted instructions, including HALT and a faulting instruction;
* `stack_size`: number of values at halt/fault;
* `value`: top value, or zero when empty;
* `host_status`: the failed host's exact nonzero return code (otherwise zero).

Preflight failures have pc/steps zero. A step-limit fault identifies the next
unexecuted instruction and does **not** add a step. Falling off the array is
checked before the step limit, so a missing HALT is reported as such even when
the final instruction exactly exhausts the step budget.

## Instruction set

All names below have the `MULIB_MINIVM_` prefix. `a b` means `b` is stack top.
Unused operands are ignored. No automatic final HALT is inserted.

| Opcode | Operand | Effect |
|---|---|---|
| HALT | ignored | Success; return top or zero if stack empty |
| PUSH | signed literal | Push literal |
| DROP | ignored | Pop one |
| DUP | ignored | Duplicate top |
| SWAP | ignored | Swap top two |
| LOAD | local index | Push local (initially zero) |
| STORE | local index | Pop into local |
| ADD, SUB, MUL, DIV, MOD | ignored | Replace `a b` by `a+b`, `a-b`, `a*b`, `a/b`, `a%b` |
| NEG | ignored | Negate top, checked |
| EQ, LT, LE | ignored | Replace `a b` by comparison result 0/1 |
| NOT | ignored | Replace top by logical-not 0/1 |
| JMP | absolute instruction index | Branch |
| JZ, JNZ | absolute instruction index | Pop condition; branch if zero/nonzero |
| CALL | host table index | Consume declared arguments; push one host result |

Indices must be nonnegative and in range. Conditional targets are validated
even when the branch is not taken. Operands/opcodes are checked when executed;
unreachable invalid records are not a validation error. Faulting VM instructions
leave stack and locals unchanged. Conditional branch conditions are consumed
only after operand/stack validation.

CALL passes contiguous read-only arguments in left-to-right order and a result
output, whose initial value is zero. A host returns 0 on success and should set
the result. A nonzero return faults without consuming arguments; the exact code
is preserved. No-argument calls still require room for their one result.
Callbacks must not mutate/retain argument storage or mutate the active program.
Native side effects are not rolled back.

## Failures

Statuses (all prefixed `MULIB_MINIVM_`): `INVALID_PROGRAM`, `NO_MEMORY`,
`RESOURCE_LIMIT`, `STACK_OVERFLOW`, `STACK_UNDERFLOW`, `BAD_OPCODE`, `BAD_JUMP`,
`BAD_LOCAL`, `BAD_HOST`, `OVERFLOW`, `DIVIDE_BY_ZERO`, `STEP_LIMIT`, `HOST_ERROR`,
and `FELL_OFF_END`. Empty/NULL code and a missing nonempty host table are invalid
program descriptors. A NULL host function faults only if called. The status
string function returns static text and accepts unknown codes. There is no
`errno` contract.

## Runnable example

```c
#define MULIB_IMPL_MINIVM
#include "mulib-minivm.h"
#include <assert.h>
int main(void) {
    const mulib_minivm_instruction_t code[] = {
        { MULIB_MINIVM_PUSH, 6 }, { MULIB_MINIVM_PUSH, 7 },
        { MULIB_MINIVM_MUL, 0 }, { MULIB_MINIVM_HALT, 0 }
    };
    const mulib_minivm_program_t p = { code, 4, 0, NULL, 0 };
    mulib_minivm_result_t result;
    assert(mulib_minivm_run(&p, NULL, &result) == MULIB_MINIVM_OK);
    assert(result.value == 42 && result.steps == 4);
    return 0;
}
```

Build with `cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude example.c -o example`.
`examples/modules/minivm.c` demonstrates locals and a terminating loop.
`tests/test_runtime.c` additionally covers arithmetic bounds/signs, division,
underflow, invalid operands/opcodes, fallthrough, instruction/allocation limits,
zero limits, host argument order/errors, and fault snapshots. It builds under
strict C11 and runs without any external interpreter. Enable ASan+UBSan with
`-fsanitize=address,undefined -fno-omit-frame-pointer -g` when available.

Define the implementation switch before inclusion in exactly one translation
unit. Other units include declarations only, including C++17 callers.
