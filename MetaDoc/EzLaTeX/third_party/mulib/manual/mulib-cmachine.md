# mulib-cmachine

Checked lowering from common MIR to the existing `mulib-minivm` interpreter.
This module does not generate native machine code and makes no platform ABI claim.

## Integration

Include `mulib-cmachine.h`. Define these implementation switches exactly once,
in one or several translation units:

* `MULIB_IMPL_MIR`
* `MULIB_IMPL_MINIVM`
* `MULIB_IMPL_CMACHINE`

No dependency implementation switch is defined by this header. No third-party
dependencies. C11 implementations, C++ C-linkage declarations.

## Lowering contract

Zero initialize `mulib_cmachine_t`. Call
`mulib_cmachine_lower(program, arguments, argument_count, record_limit,
code_limit, &machine, diagnostic)`. Output must be empty. The entire MIR is
validated before allocation; every opcode in `manual/mulib-mir.md` is supported.
The supplied argument count must exactly match `parameter_count`. A nonempty
argument array cannot be NULL. Every BOOL PARAM argument is preflight checked
for 0/1 before any VM arithmetic. Invalid arguments return INVALID/BAD_TYPE.

PARAM records become PUSH literals: **lowering specializes the program to a
copy of the supplied argument values**. Changing/freeing arguments or MIR after
successful lowering does not change the machine. Lower again to change inputs.
No host callbacks are needed; no unfinished emitter/assembler APIs are used.

Each non-return value ID becomes a minivm local. CONST/PARAM become PUSH, STORE;
COPY becomes LOAD, STORE; unary operations LOAD, operation, STORE; binary
operations LOAD left, LOAD right, operation, STORE. RETURN becomes LOAD, HALT.
This preserves original record order, including unused faulting operations.
Maximum operand-stack depth is two slots. Code records are the existing minivm
C instruction records, not serialized portable bytecode.

## Execution and errors

`mulib_cmachine_run(&machine, limits, result)` invokes `mulib_minivm_run`.
NULL limits selects minivm's defaults. Explicit limits bound stack slots,
locals, and executed steps; all zero fields really mean zero. Each run starts
with zeroed locals and fresh stack. The returned value is the returned MIR
value (Boolean returns are 0/1).

Execution returns **minivm statuses**, not MIR statuses. Signed overflow,
zero divisors, insufficient stack/locals, allocation failure and step limits
remain explicit. `result.pc` is the **lowered bytecode PC**; there is no source
map. Lowering diagnostics instead identify MIR record indices and use MIR
statuses. Lowering is atomic: failure leaves empty output. A NULL result is
allowed. Execution of a NULL machine reports INVALID_PROGRAM.

## Ownership and limits

The machine owns `code`. Its `program` field is a borrowed view of that code;
do not free/change the view independently, or copy a live owning machine.
`mulib_cmachine_free` frees code and zeros the entire object, accepting NULL.
Execution retains no memory and does not mutate the machine.

`record_limit` bounds validation. `code_limit` bounds the exact emitted
instruction count. The allocation size and every count addition are checked.
Value IDs must be representable by signed-64-bit minivm operands. The only
lowering allocation is the exact code array. Runtime allocation limits belong
to minivm and are independent of lowering limits; its default 1024-local bound
can reject a successfully lowered large program until explicitly increased.
No rollback is needed because this subset has no external side effects.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/cmachine.c -o /tmp/cmachine
/tmp/cmachine
```

The example lowers `argument * 7`, changes the original argument, and still
executes to 42, asserting the specialization/ownership contract. The compiler
IR suite also checks optimized/unoptimized equivalence and runtime faults.
