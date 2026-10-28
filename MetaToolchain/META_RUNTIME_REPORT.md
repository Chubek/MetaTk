# MetaVM / MetaJIT runtime completion — 2026-10-28

## API additions and compatibility

The user approved a new versioned scalar signed-64 VM baseline, with explicit
host calls, bounded execution, tracing/native compilation, OSR and deoptimization.
The four runtime gaps from the [first audit](META_REFINEMENT.md) are implemented:
`MetaVM/vmspec`, `MetaVM/vmweave`, `TracingJIT.hpp`, and the OSR/Deopt interfaces.
There are no remaining empty source/header files in the seven audited Meta*
directories. This is an implementation of the agreed scalar baseline, not a claim
that every future compiler feature exists.

New public C++17 APIs:

- `MetaVM/vmspec/VM.hpp`: `Op`, `Instruction`, `Limits`, immutable `Program`,
  `Status`, `Trap`, `Stop`, owning `Frame`, and `Host` callback.
  `Program` validates construction, exposes const code/host/count accessors,
  serializes and deserializes bytecode. `make_frame`, `validate_frame`, and `run`
  create/validate/resume execution state.
- `MetaVM/vmweave/Weave.hpp`: `weave(source,function,metail_home,limits)` compiles
  one named MetaIL function using the actual MetaIL parser.
- `MetaJIT/Deopt.hpp`: `NativeExit`, ABI-v1 `NativeFrame`, and owning
  `DeoptSnapshot` with validated restore and const state/reason accessors.
- `MetaJIT/OSR.hpp`: `Transition` and `osr_enter` for exact frame-boundary entry
  and materialized side-exit commit.
- `MetaJIT/TracingJIT.hpp`: `TraceOptions`, `TraceStats`, `TracingJIT`,
  `program`, `stats`, `native_supported`, `run`, and `execute`.
- `MetaJIT/Runtime.hpp`: bounded `CodeBuffer::append(vector<uint8_t>)` and
  recognition of the `int64_t (*)(NativeFrame*)` transition signature.
- Build targets: `MetaVM::VM`, optional `MetaVM::Weave`, `MetaJIT::Tracing`.
  `vmweave` provides compile/run commands when the adapter is enabled.

No existing C11 public signature, C ABI v1 layout or architecture spec data was
changed. MetaIL itself is still an IR/optimizer library; the interpreter and JIT
live in separate components. Existing inline preprocessing and lazy method
compilation remain supported. No new external runtime dependency was added;
the portable VM uses only the C++17 standard library, and the optional adapter
reuses the existing MetaIL dependency configuration.

## Implementation

### Versioned VM and hostile input handling

The bytecode format is explicitly specified in
[`MetaVM/vmspec/FORMAT.md`](MetaVM/vmspec/FORMAT.md): little-endian fields,
version 1, 27 opcodes, instruction-index branches, counted host names and fixed
24-byte instruction records. It is not a dump of native structs. The loader
rejects unknown versions/opcodes, invalid registers/targets/calls, duplicate or
invalid host names, fallthrough, truncation, trailing bytes and excessive sizes.
Serialization has an independent fixed-byte anchor as well as round-trip tests.

The VM implements wrapping arithmetic, signed comparisons, truncating division
and remainder, explicit shift/division traps, and explicit host calls. It avoids
C++ signed-overflow/invalid-shift UB. Frames retain exact slots, PC, result, trap
and cumulative step count. Finite budgets suspend and resume without changing
semantics. Opaque program identities prevent accidentally resuming a frame or
snapshot against different code, even when bytecode and frame shape are equal.
Active-frame reentry and step-counter overflow are rejected.

### MetaIL lowering

The adapter uses the real parser and must-definition CFG analysis, including
back edges and joins, before emitting bytecode. It rejects possibly uninitialized
reads and reachable fallthrough, lowers lazy `select` with branches, preserves
left-to-right call effects and eager logical operations, and stages host argument
values explicitly. Expression temporaries are reused between statements and
checked against the slot limit. There is no ad-hoc source regex parser or
registered no-op pass. Every call expression explicitly uses the host namespace;
there is no implicit linker or source-module recursion contract.

### Tracing, OSR and deoptimization

Static backward destinations identify candidate loop headers. The interpreter
warms headers and records actual paths. The x86-64 compiler emits real arithmetic,
comparisons, bounded slot loads/stores, budget checks and path/trap guards. It
does not call the interpreter once per compiled instruction.

At OSR, an already-running VM frame enters native code at its current header.
All slots are materialized at every instruction boundary. Guard failures side-exit
at the actual branch destination; arithmetic traps retain the faulting PC without
writing the destination. Native budget accounting is bytecode-instruction-exact.
The interpreter can resume directly without replaying a branch or host effect.
Owning deoptimization snapshots retain all values and identity independently of
RX memory lifetime.

Host calls are mandatory interpreter boundaries, never speculated or duplicated.
Trace count, recording length and resident generated code are bounded. Saturated
caches fall back to interpretation. RW staging becomes RX before execution;
there is no RWX mapping. Invalid options, allocation and mapping failures throw;
semantic traps are VM states. Callback failure cannot silently replay a possibly
performed external effect. Ownership, trusted-code boundaries, synchronization
and ABI offsets are detailed in [`MetaJIT/TRACING.md`](MetaJIT/TRACING.md).

## Verification

Baseline before this completion: root suite **34/34**, 12.48 s; standalone
MetaJIT/MetaIL suite **19/19**, 4.52 s. The integrated root build now includes
the earlier MetaJIT/MetaIL cases plus four new runtime test cases, for **57**.
These are not 57 new tests; overlapping standalone counts must not be summed.

Full integrated builds used `ARCHDB_BUILD_METAJIT=ON` and
`-Wall -Wextra -Wpedantic -Werror` for C and C++:

| Configuration | Full suite | Wall time |
| --- | ---: | ---: |
| GCC 16.2.1 Release (`build-archas`) | 57/57 passed | 22.24 s |
| Clang 22.1.8 Release, shared libraries (`build-archas-clang`) | 57/57 passed | 19.23 s |
| GCC Debug ASan+UBSan (`build-archas-asan`) | 57/57 passed | 100.70 s |
| Clang Debug ASan+UBSan (`build-archas-clang-asan`) | 57/57 passed | 104.41 s |

Final additional identity/wire-anchor/self-loop assertions were rebuilt and
checked in the affected tests after the full matrix. No warnings or sanitizer
findings were reported. Timings include competing builds/tests and are not
performance benchmarks or a regression-budget claim.

Additional verification:

- **41,603 interpreter/native differential assertions:** arithmetic and comparison
  boundary values, aliasing destinations, warmed native trap exits, native
  return paths, terminal frame behavior, alternating branch deoptimization,
  every tested suspension boundary, one-step resume, infinite-loop budget
  enforcement, host ordering/reentry, snapshot identity, cache limits and explicit
  interpreter-only mode. Complete slot state, PC, step count, status, result,
  trap and diagnostic are compared, not just final answers.
- VM tests: fixed wire anchor, truncation at every byte, invalid versions/counts/
  targets/registers/opcodes, host failures, active-frame/counter checks and
  **2000 deterministic hostile-wire mutations**. Accepted mutations must
  reserialize identically and execute within a finite budget.
- Adapter tests: loops, lazy/trapping arms, eager logical calls, nested host
  arguments, conditional expressions in branches, exact limits, uninitialized
  values, fallthrough and rejected assumptions.
- CLI tests: compile/load/execute, budget failure, input/output alias protection,
  usage and I/O exit codes. The checked-in sum example produces
  `result=55 steps=116` for argument 10.
- Dependency-free C++17 VM-only build/test passes (`build-metavm-core`).
- Optional-adapter standalone build/test passes (`build-metavm`).
- Pure-C root configuration builds warning-free with no C++ compiler configured
  (`build-meta-c-only`); runtime options default OFF.
- Standalone MetaIL suite remains 18/18; rewrite verification remains 230 rules,
  183473 signed-64 cases, including 2470 effect/trap cases.
- All 28 generated MetaArchDB models still verify against their YAML originals.
  No generated backend or spec file was edited.

Ninja is unavailable, so new trees use Unix Makefiles. The first pure-C configure
attempt pointed at a nonexistent assumed yyjson cache directory; it was corrected
to the pre-existing source override used by the established root builds, then
built successfully. This was an environment-path error, not a missing new
dependency. Mutation tests are bounded regression coverage, not a claim of a
ten-minute libFuzzer campaign.

## Reproduce

Full integrated verification, from the toolchain root with its existing sibling
dependencies installed or configured as documented in the component READMEs:

```sh
cmake -S . -B build-runtime-gcc -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DARCHDB_BUILD_METAJIT=ON \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build build-runtime-gcc --parallel 4
ctest --test-dir build-runtime-gcc --output-on-failure -j 4
```

Use a different directory with `-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++` for Clang. For each compiler's sanitizer job use
Debug and append `-fsanitize=address,undefined -fno-omit-frame-pointer` to **both**
language flag strings. For the shared variant add `-DBUILD_SHARED_LIBS=ON`.
If yyjson is provided by a source checkout, use the existing
`FETCHCONTENT_SOURCE_DIR_YYJSON` override; otherwise normal package/fetch
discovery applies.

Focused standalone and external checks:

```sh
cmake -S MetaVM -B build-vm-check -DCMAKE_BUILD_TYPE=Release
cmake --build build-vm-check --parallel 4
ctest --test-dir build-vm-check --output-on-failure

cmake -S MetaVM -B build-weave-check -DCMAKE_BUILD_TYPE=Release -DMETAVM_BUILD_WEAVE=ON
cmake --build build-weave-check --parallel 4
ctest --test-dir build-weave-check --output-on-failure -j 4
build-weave-check/vmweave compile sum MetaVM/examples/sum.mi /tmp/opencode/sum.mvm
build-weave-check/vmweave run /tmp/opencode/sum.mvm 1000 10

MetaIL/build/metail verify
python3 tools/compile_archdb.py --check --tool build-archas/metaarch
git diff --check
```

## Scope boundaries, not remaining stubs

Native tracing remains Linux x86-64 System V as approved; other platforms use
the interpreter (native-disabled mode is tested; no macOS/Windows execution run
is claimed). Version 1 deliberately has no heap/memory instructions, floating
point, implicit recursive function linker, SSA, register allocator or speculative
host-call ABI. Assumptions remain rejected rather than invented. These exclusions
are explicit contracts, not registered empty implementations.

The earlier broad MetaArch codec/schema/packaging maintenance brief is separate
from this runtime completion and is not claimed complete by these results.
No issue from the four agreed remaining runtime areas is left as a placeholder.
