# 11. Linux x86-64 native lowering

## Complete IL lowering, explicit target

`lower_linux_x86_64(program, vm_options, max_source_bytes)` returns a
`NativeSource` containing assembly or IL-style diagnostics. The collective
`native_source` method wraps that result in the common diagnostic type.
`executable` then assembles and links it with the configured built-in tools.

Every documented MetaIL opcode has a lowering. This includes signed division
and remainder checks, logical shifts, comparisons, conditional branches,
shared value/return stack behavior, decimal printing, and halt. There is no
fallback that silently ignores an unsupported IL operation.

The target is Linux x86-64 using direct syscalls. It is not the Windows x64
ABI, macOS syscall ABI, a C-callable function compiler, or a relocatable
position-independent code generator. The output is a standalone `_start`
program and uses no C runtime or dynamic loader.

## Frame and register conventions

The generated startup code saves the initial stack pointer in `rbp`, reserves
256 bytes for the 32 IL registers, and initializes all register slots to zero.
IL register `rN` resides at `[rbp - 8*(N+1)]`. Native scratch registers are not
exposed as IL registers, so x86 instruction constraints cannot corrupt a live
IL value accidentally.

`r14` counts IL instruction dispatches, and `r15` counts values on the shared
operand/return stack. Other native registers are scratch. There is no optimizing
register allocator; frame-based lowering trades code size/speed for simple,
auditable semantics. Every IL index has a private generated label.

Programs halt by Linux syscall 60. The full IL halt value is copied to the
exit argument, but a Unix parent observes only its low eight bits. The
interpreter's `Execution::value` remains full signed 64-bit.

## Arithmetic and comparisons

Addition, subtraction, multiplication, and bitwise operations use x86 integer
instructions whose low-bit behavior matches MetaIL. Signed comparisons produce
exactly zero or one. Right shifts use `shr`, not `sar`, matching logical IL
semantics for negative bit patterns.

Shift counts are checked before x86's masked-count instruction behavior can
hide an invalid IL count. Division checks zero and the `INT64_MIN/-1` overflow
case before executing `idiv`. Remainder takes the checked division's remainder.
These checks intentionally preserve IL traps instead of relying on C++ UB or
accidental CPU exceptions.

## Calls, returns, and stack checks

IL `call` pushes the numeric next instruction index, then jumps. It does not
use an x86 hardware call as an incompatible hidden return-address convention.
`push` and `call` check the configured stack-value count; `pop` and `ret` check
underflow. A return checks its numeric target and dispatches through a table of
relocated instruction addresses, including the legal fallthrough endpoint.

Because IL values and return addresses share one stack, the backend preserves
the same intentionally low-level semantics as the interpreter. A program can
construct a valid numeric return target, but it cannot use `ret` to jump outside
the generated IL dispatch table through ordinary IL operations.

The stack still lives in an operating-system stack mapping. The logical value
limit is not a guarantee that the OS stack mapping can accommodate that many
values. Choose a lower `max_stack_values` for deployments with restricted stack
space and test your target's resource configuration.

## Printing

The backend converts signed integers, including `INT64_MIN`, to decimal and
appends a newline. It uses a bounded stack scratch buffer and Linux `write` to
file descriptor 1. Partial writes are retried, and `EINTR` is retried. A zero or
other failed write exits with the I/O error code.

Printing is a runtime side effect in native executables. It may block according
to the host file descriptor's behavior. SIGPIPE or other operating-system
signals may terminate the process before a syscall error is returned; signal
policy belongs to the deployment environment.

## Budgets and error exits

Before each IL instruction, generated code checks the configured step budget.
Successful, trapping, and halt dispatches all follow the same attempted-step
model. Native helper instructions do not consume extra IL steps.

| Condition | Native exit status |
|---|---|
| normal halt | low 8 bits of halt value |
| end-of-program fallthrough | 0 |
| step budget exhausted | 124 |
| operand stack failure or invalid return | 125 |
| division/remainder/shift failure | 126 |
| output syscall failure | 127 |

These codes are not disjoint from normal program return values. If a protocol
must distinguish all successful integer results from all errors, use the
interpreter's structured `Execution` result or design an explicit IPC protocol.

## Artifacts and security

Generated symbols use a private `.mc_` namespace plus exported `_start`. The
backend emits assembly text, and the assembler/linker retain relocation checks
for branch and dispatch-table addresses. The ELF image uses RX code, read-only
headers, and a non-executable stack. The compiler does not allocate RWX memory
inside its own process.

This is not a sandbox. A malformed host-side C++ object, compiler defect,
operating-system resource failure, or a raw assembled program can still be
dangerous. Use process isolation, resource limits, and provenance checks when
running code outside your trust boundary. Producing an ELF artifact never
authorizes MetaChain itself to execute it.
