# Tracing, OSR and deoptimization

## Use

Link `MetaJIT::Tracing` and (for MetaIL source input) `MetaVM::Weave`:

```cpp
#include <TracingJIT.hpp>
#include <vmweave/Weave.hpp>

auto program = metavm::weave(
    "fn sum(n) { let x=0; loop: br n,body,end; "
    "body: let x=x+n; let n=n-1; goto loop; end: return x; }", "sum");
metajit::TracingJIT jit(program);
auto result = jit.execute({20}, 10000);
// result.status == metavm::Status::Returned; result.result == 210
auto native_instructions = jit.stats().native;
```

`tests/tracing.cpp` compiles and exercises this path. `execute` creates an owned
frame; `run(frame,budget,host)` resumes an existing frame belonging to the same
Program identity. `program()` exposes a const view; `stats()` exposes cumulative
interpreted/native instruction, compiled trace, OSR, guard, budget and trap counts.
These views borrow the engine. Engine mutation/destruction requires caller
synchronization; callback reentry is rejected. Independent instances may run
concurrently. No native pointer outlives or escapes the engine-owned executable.

On unsupported execution platforms, or with `TraceOptions::native=false`, the
same validated VM runs without native compilation. `native_supported()` reports
the host backend. The initial native backend is Linux x86-64 System V only.

## Recording and native compilation

Static backward branch destinations identify loop headers. Visits warm a bounded
counter. At `hot_threshold` (default 3), the interpreter records an actual path,
including branch outcomes, until returning to that header, reaching a return or
trap, reaching a host call, exhausting the call's budget, or filling the trace.

The compiler emits native arithmetic, comparisons, slot loads/stores and guarded
control flow for that path. It is not a trampoline that calls an interpreter for
each operation. A matching path runs natively. A mismatching branch returns to
the interpreter at the **actual branch destination**, without executing the
branch twice. Arithmetic guards implement division-by-zero, signed division
overflow and invalid shifts as explicit VM traps, never process signals.

Calls are mandatory interpreter boundaries: they are not compiled, speculated,
duplicated or reordered. Arguments already computed before a boundary remain in
their materialized slots. Calls appearing at a loop header disable tracing for
that header rather than publishing a zero-progress trace. There is no generated
native-to-host ABI or implicit native callee resolution.

One trace is cached per header; alternate paths side-exit rather than pretending
to be compiled. This is a bounded baseline tracing implementation, not an SSA,
register-allocation, trace-linking, memory or alias-analysis framework.

## NativeFrame transition ABI v1

`Deopt.hpp` defines a standard-layout `NativeFrame` with these offsets on the
supported 64-bit backend:

| Byte offset | Field |
| ---: | --- |
| 0 | `int64_t *values`: pointer to all VM slots |
| 8 | `uint64_t pc` |
| 16 | `uint64_t remaining`: instruction budget |
| 24 | `uint64_t status`: VM Running=0, Returned=1, Trapped=2 |
| 32 | `uint64_t trap`: VM trap code |
| 40 | `int64_t result` |

The signature is `int64_t (*)(NativeFrame*)`, returning `NativeExit`:
Boundary=0, Guard=1, Budget=2, Returned=3, Trapped=4. Both layout and opcode numbers
are checked/defined in source. Changing these requires a new ABI version.
`Runtime.hpp` recognizes this signature in addition to the existing up-to-six
int64-argument function signature. Low-level `Executable::get` checks entry
existence and arity, not native code safety or a complete C++ type signature.
`CodeBuffer::append(vector<uint8_t>)` supports bounded generated trace bytes.

Every VM slot is materialized in memory at every instruction boundary. Before an
instruction, native code records its PC and checks budget; only then does it
decrement the remaining budget. Trapping instructions consume one step without
writing a destination. Return/trap retain their instruction PC. Successful
instructions and branch guards preserve the next interpreter PC.

`osr_enter(program,frame,header,budget,entry)` validates program identity, exact
entry PC, frame shape, state, reentry and step-counter bounds. It borrows existing
slots, invokes the transition and validates/commits scalar exit fields. This is
OSR from a running VM frame, not a call that restarts the source function.
Side exits are deoptimization: all values and the exact PC are immediately
available to resume the reference interpreter. No hidden native registers or
unwound interpreter call stack need reconstruction in this scalar baseline.

`DeoptSnapshot(program,frame,reason)` copies the complete deoptimized state and
keeps its program identity alive; `restore(program,out)` validates identity and
restores an owning frame. The snapshot remains valid after RX code destruction.
Snapshot reason/state mismatches are rejected.

## Limits and failure behavior

Default limits: 256 recorded instructions/trace, 64 traces, 8 MiB resident code.
Configured upper bounds are 4096 instructions/trace, 4096 traces, 64 MiB resident
code; each compilation is additionally capped at 1 MiB of generated bytes.
Reaching resident trace/code limits falls back to interpretation, not a no-op.
Temporary compilation can allocate one capped trace before checking the resident
budget. The decoder and compiler also enforce the VM's resource limits.

Programs outside signed disp32 slot addressing or imm32 PC addressing remain
interpreted. Step budgets count bytecode instructions identically in interpreter
and native execution, including partial traces; they do not bound callback wall
time or compilation cost. Zero budget does not execute or compile a trace.

Semantic failures are VM trap statuses. Invalid options/frame state and counter
overflow throw; C++ allocation and OS mapping failures propagate with the frame
at its latest materialized interpreter boundary. Callback exceptions follow the
VM contract. Native mappings are RW while constructed, then RX, never RWX.

The raw CodeBuffer/Transition APIs are **trusted machine-code interfaces**, not
a sandbox: invalid native code can corrupt memory before exit checks run. The
structured VM loader validates bytecode; the provided trace compiler only emits
bounded slot access and guarded arithmetic from those validated instructions.
Do not treat arbitrary native buffers as safe input to `osr_enter`.
