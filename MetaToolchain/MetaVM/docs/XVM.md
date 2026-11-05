# XVM: default Machinery execution

## API additions and defaults

The existing C11 ABI is retained: `mvm_limits`, values, modules, serialization,
native callbacks and the numeric values of the old engines are unchanged.
`MVM_ENGINE_XVM = 2` is new; `MVM_ENGINE_DEFAULT` is its alias. The C++11
`metavm::VM::run`, Definition execution, `metavm-run` and Machinery-backed DVI
event replay now choose XVM unless an engine is supplied explicitly.

`Machinery/Exec-XVM.h`, also included by `MetaVM.h`, adds:

* `mvm_run_xvm` and `mvm_run_default`, with the same arguments and ownership as
  `mvm_run_bytecode`/`mvm_run_threaded`.
* `mvm_xvm_options {max_plan_bytes, max_native_bytes, native_mode}` and
  `mvm_xvm_default_options()`.
* `mvm_xvm_configure(vm, options, error)`: copies validated options into an idle
  VM, returning MVM_BUSY during execution. Failure preserves old options.
* `mvm_xvm_statistics {instructions, blocks, plan_bytes, native_bytes,
  executed_blocks, native}` and `mvm_xvm_stats(vm)`. Counters describe the last
  started invocation; valid invocations of other engines reset them.

No existing entry point was repurposed. Explicit BYTECODE/THREADED calls still
select the old implementations. `mvm_run` still requires an engine argument;
existing callers passing 0 continue to select BYTECODE.

```c
mvm_error error;
mvm_value result;
mvm_vm *vm = mvm_create(NULL, &error);
/* module is caller-owned and outlives execution. */
mvm_status status = mvm_run_default(vm, module, 0, NULL, 0, &result, &error);
mvm_xvm_statistics stats = mvm_xvm_stats(vm);
/* stats.native distinguishes native blocks from portable compiled XVM. */
mvm_destroy(vm);
```

```sh
metavm-run factorial.mvm              # XVM, native when available
metavm-run factorial.mvm --xvm
metavm-run factorial.mvm --bytecode
metavm-run factorial.mvm --threaded
```

`dvi-run` accepts the same engine selectors for Machinery event replay. Its DVI
decoder and native DVI semantics are unchanged.

## Compiler and native tier

`Machinery/XVM.inc` compiles every verified opcode to a specialized runtime
operation, resolving branches into basic blocks and splitting straight-line runs
at 32 instructions. Calls, branches and returns terminate blocks. All instructions,
including unreachable ones, pass the original verifier first; unknown opcodes
are never replaced by NOP or generic arithmetic.

Native code contains real sequences of helper calls: **subroutine-threaded block
compilation**, not a single trampoline into the old dispatcher. The same plan can
execute portably without executable memory. Arithmetic, tags, objects, GC, calls
and effects share the established semantic handlers. This is not an optimizing
register allocator or inline arithmetic compiler; no speedup is promised.

AMD64, Aarch64 and RISC-V64 emitters preserve their ABIs and stack alignment.
RISC-V literal loads are aligned. Code is RW during construction, then RX, never
RWX; instruction caches are synchronized. Entries have readable padding for
sanitizer metadata probes. Guest-provided native pointers are not accepted.
Native execution was tested on Linux amd64; other architectures/OSs remain
execution-unvalidated. Big-endian and arm64e ABIs use portable XVM.

Public C11 headers and the C++11 wrapper remain extension-free. Platform memory
APIs/cache-flush intrinsics are private and optional.
`-DMETAVM_XVM_NATIVE=OFF` excludes them; portable XVM needs only the C standard
library. There is no mandatory C++, MetaML, LLVM, MetaArch or other library.

## Runtime guarantees

Each guest instruction costs one step, including every instruction inside a
native block. Fuel exhaustion attaches the same function/PC as the old engines
and never executes later instructions in that block. Frames remain iterative
heap frames, not recursive native calls. Wrapping i64 arithmetic, shift/division
traps and **truthy integer zero** are unchanged. Definition still normalizes its
conditions. MetaML's XVM has a different typed value model; neither compiler
reinterprets the other's instruction semantics.

Registers remain GC roots during native code and host callbacks. Callbacks can
allocate/collect; reentrant runs/configuration/registration are rejected. Errors
unwind frames and leave the VM reusable. Result rooting and aliased arguments
retain their existing rules.

Plans belong to one invocation and borrow instructions only until it returns.
All buffers/pages are released, including on failures. Immutable modules acquire
no caches or hidden writes, so independent VMs can share them. Idle module
optimization/replacement before another run cannot leave stale native pointers.

## Limits and fallback

Original fuel, frame, register, heap and module limits remain. XVM independently
caps plan allocations and native code at 64 MiB each by default. Overflow-safe
accounting charges every plan table allocation, including overprovisioned
block/leader/index tables. Native bytes include alignment and sanitizer padding.
Compiler staging storage is separately bounded by the native ceiling. These
logical ceilings are not allocator-overhead/RSS or process-wide memory quotas.

Ceiling exhaustion returns MVM_LIMIT; allocation failure returns MVM_OOM. Both
byte ceilings must be positive. Partial plans are released on every failure.

| Native policy | Behavior |
| --- | --- |
| `MVM_XVM_NATIVE_AUTO` | Default: native if supported and executable mappings succeed; otherwise portable compiled blocks. |
| `MVM_XVM_NATIVE_OFF` | No executable allocation; native capacity is unused. |
| `MVM_XVM_NATIVE_REQUIRED` | Mapping/ABI unavailability returns MVM_NOT_FOUND, not fallback. |

Configured ceiling violations never bypass their ceilings by falling back.
`stats.native` reports the actual tier. Callback time and arbitrary trusted
MetaML authoring are not bounded by guest fuel.

## Optional MetaML integration

VMWeave now defaults to current workspace MetaML, whose default executor compiles
all expression kinds to bytecode/native blocks. Set `METAVM_DSLTK_INCLUDE_DIR`
to that SDK when enabling `METAVM_ENABLE_METAML`. The supplied
`third_party/MetaML.hpp` remains unchanged; `METAVM_LEGACY_METAML=ON` explicitly
selects that tree-only dependency. External header-only consumers may define
the same macro for the legacy adapter. Machinery's JIT needs neither dependency.
See DSLtk's `XVM.md` for the separate MetaML compiler, modes and cache semantics.

## Scope and tests

This changes the execution substrate, **not catalog guest compatibility**.
CHIP-8 profiles/ROMs, SSAVM simultaneous PHIs/immutable arrays, SECD captures and
strict application, Krivine call-by-name WHNF and DVI-2 decoding remain separate
native implementations. Catalog counts/generated identities are unchanged. No
historical JVM, Python, WASM or other full runtime is newly claimed.

Tests retain both old engines and add XVM to arithmetic/traps, effects, GC,
budgets, binary mutation differentials, 49 catalog factories, generated C,
Definition and DVI replay. Dedicated tests cover native/portable policy,
compiled-block counters, ceilings, exact fuel boundaries, aliased arguments,
callback collection/reentrancy, diagnostic PCs and reuse after failure.
See [XVM_BASELINE.md](XVM_BASELINE.md) for baseline/reproduction notes.
