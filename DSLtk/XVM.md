# XVM: MetaML's default executor

Fresh C++ and D sessions now execute through XVM, including the C embedding
adapter, file/import evaluation, both C++ CLIs, the D CLI and primitive callbacks.
Parsing, type checking, module checking and diagnostics remain MetaML's own.
The previous tree evaluator remains available as an explicit reference engine.

## Selecting an engine

```sh
metaml --engine xvm -e '40 + 2'       # default: native blocks when available
metaml --engine portable -e '40 + 2'  # compiled bytecode without executable pages
metaml --engine jit -e '40 + 2'       # require native execution
metaml --engine tree -e '40 + 2'      # retained tree evaluator
```

C++ embedders select `session.interp().engine` using
`metaml::ExecutionEngine::{XVM, PortableXVM, NativeXVM, Tree}`. D exposes the same
enum names in `MetaML`. `execution_engine_from_name` returns an optional/Nullable
enum for CLI names. New worker interpreters inherit the selected engine and
limits but have independent code caches and operand stacks.

`XVM` automatically uses portable **compiled XVM**, not the tree evaluator, when
native executable memory or a supported native ABI is unavailable. `NativeXVM`
instead reports M0409. Defining C++ `METAML_NO_JIT`, or D version
`METAML_NO_JIT`, disables executable allocations without removing portable XVM.

## Compilation and execution

`XVMCode.hpp` and the template-only `XVMCode.d` lower every checked expression
kind into stack bytecode: constants, local/global/qualified loads, constructors,
closures, operators, saturated/partial application, concatenative composition,
pipelines, short-circuit branches, lexical/recursive bindings, pattern matches,
comprehensions, ranges, lists, tuples, records, updates and projections. The
compiler uses an explicit work stack and symbolic branch labels, not recursive
descent over the expression tree.

Closures retain their original lexical frames and parameter patterns. Calls use
the existing value/application ABI for primitives, constructors, partial and
over-application, and returned closures. Tail applications transfer to compiled
callee code without growing the host stack. Pattern matching, lazy module
forcing, comprehensions and boxed-value operations use shared semantic runtime
helpers; their subexpressions still execute as compiled XVM code. There is no
`eval_tree` fallback in the XVM executor.

Source nodes are retained as compiler metadata for source spans, names,
patterns, checked cells and closure ownership. They are not an executable
`ExecuteProgram` expression instruction. The v1 `Chunk` wire format remains a
source-backed portable module envelope: it intentionally does not serialize
session-specific checked cells or native pointers. Its contents are checked and
lowered in the destination session.

Checking can change application/composition resolution when a parsed AST is
reused under new bindings. Each checked unit advances the interpreter's compile
revision; cached code is rebuilt before reuse under another revision. Native
blocks retain their instruction metadata across nested evaluation and tail
transfers, so cache replacement cannot invalidate active machine code.

## Native tier

`XVMNative.hpp` and `XVMNative.d` compile basic blocks containing up to 32
specialized runtime calls. These are executable, subroutine-threaded native
blocks, **not a single jump to the tree interpreter**. Branches and tail calls
return to the VM's block scheduler; ordinary instructions run consecutively
inside generated code. An exception is captured by the called C++/D step helper
and rethrown only after returning from generated code, so unwinding never needs
synthetic native unwind metadata.

The same architecture layer backs `metalib/arch`: Amd64, Aarch64 and Riscv64.
Native code saves ABI callee-saved registers, preserves stack alignment, uses
aligned RISC-V literal loads and synchronizes instruction caches. Mappings are
RW during construction and RX afterwards, never RWX. Entry points have a
readable zero prefix for indirect-function sanitizer probes. Host architecture
matching is required; big-endian and arm64e ABIs use portable execution.

This is a baseline helper-call JIT, **not an optimizing register-allocating
compiler**. Pattern/value/allocation helpers are not inlined. Native execution
has been exercised on Linux amd64; other emitted targets require native hardware
or emulators for execution validation. Hardened processes may disallow JIT pages.

## Limits, lifetime and observability

Each interpreter exposes independent logical ceilings:

| Field | Default | Applies to |
| --- | ---: | --- |
| `xvm_code_limit` | 1,000,000 | instructions in one compiled expression and compiler work queue |
| `xvm_stack_limit` | 1,000,000 | values and saved lexical environments in one activation |
| `xvm_native_limit` | 64 MiB | generated machine-code bytes per compiled expression |

Code/stack/native ceiling failures report M0406. Zero native capacity disables
the native tier; strict native execution then reports M0409. The existing
`max_depth`, call-depth accounting and cancellation checkpoints remain active.
Parser/checker allocations, boxed-value storage and aggregate session history
are not charged to these ceilings; this is not a process-wide memory sandbox.

`xvm_instructions` and `xvm_native_blocks` are cumulative executed counters.
They allow tests and embedders to distinguish portable execution from actual
native execution. The tree engine does not increment them. Internal compiled
instruction/cache layouts are not a stable serialization or embedding ABI.

Caches belong to interpreters, not modules or shared source trees. Session and
VM evaluation still require caller synchronization. C++ uses RAII for native
pages. D's `Session.close()` releases currently cached pages after joining its
resources; garbage collection finalizes superseded code. D
`interp.clear_xvm_cache()` explicitly clears idle caches and rejects active
evaluation. `metalib.xvm.VM.close()` closes that adapter; its compiled code is now
owned by the destination session.

XVM does not grant primitive permissions. Nested runs preserve the destination
session's capability allowlist, output and source context. Foreground session
evaluation primitives and `current` source inspection are unavailable to worker
interpreters, avoiding concurrent access to the foreground checker/context.

## Verification

The shared toolchain tests compare the retained tree engine against portable and
native XVM, including closures as results, effects, recursive ADTs, records,
modules/functors, errors, source restoration, repeated chunks, code/operand/native
limits, cancellation and rechecking a shared AST with changed application
resolution. `scripts/check_xvm.py` additionally compares stdout, stderr and exit
status across existing fixtures and generated expressions in both dialects.

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

See MetaVM's `docs/XVM.md` for the separate C11 Machinery integration. Sharing the
block-compilation design does not conflate the two languages' value models or
instruction sets.

[XVM_VALIDATION.md](XVM_VALIDATION.md) records the observed test matrix,
reproduction commands and remaining platform/sanitizer limitations.
