> [ROUND4.md](ROUND4.md) adds CHIP-8 profiles/ROM regressions and SDK packaging.
> Its validation record supersedes the corresponding open items below.

# Third implementation pass

This pass implements two refinement priorities: the real MetaML authoring and
Definition embedding, and expansion/resource accounting of the SSAVM subset.
It preserves the first- and second-pass records and `history/round1.zip`.

## Real MetaML integration

A compatible DSLtk is now available at `/mnt/nvme/metatk/DSLtk/DSLtk.hpp` in this
workspace. Its SHA-256 is
`01ce1fe69e1e09f88662946bf9dca19baa7cd7ada9c0ae633d730685f529bfa3`.
The supplied MetaML header remains byte-for-byte unchanged. Configure its actual
optional C++20 adapter with:

```sh
cmake -S . -B out-meta -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON \
  -DMETAVM_ENABLE_METAML=ON -DMETAVM_DSLTK_INCLUDE_DIR=/path/to/matching/DSLtk
cmake --build out-meta -j4
ctest --test-dir out-meta --output-on-failure
out-meta/vmweave-metaml vmweave/factorial.vmweave factorial.mvmb
out-meta/Machinery/metavm-run factorial.mvmb
# 3628800
```

Execution revealed two previously untested integration defects. MetaML uses `--`
or `{- ... -}` comments, not `(* ... *)`; the catalog generator and typed factorial
source now use its actual syntax. Host primitives resolve through qualified
paths, not bare identifiers. The adapter registers qualified `VMWeave.vm_*` and
`VMWeave.vd_*` primitives and evaluates language-level aliases, preserving the
existing `vm_*` surface. It does not replace or modify MetaML's parser.

All 49 catalog `.vmweave` files are evaluated, serialized byte-for-byte against
their `.mvm` companions, and executed under both Machinery engines (including
DRMVM's software surface effect). Typed factorial has a matching assembly fixture.
A MetaML function emitting its loop is checked against the same serialized
module. These are authoring/factory tests, not historical guest ISA support.

`MetaMLError` retains the original diagnostic objects (codes, primary/secondary
spans, fixes and suggestions); `what()` includes MetaML's rendered source excerpt.
The original `compile(source, max_bytes)` remains; its new overload additionally
accepts a filename and `EvaluationLimits`. `compile_definition` accepts the same
source budget, filename and evaluation limits.

Default evaluation limits are 1,000,000 evaluation/call cancellation checkpoints
and depth 256, enforced through MetaML's own interpreter hooks. Checkpoint
exhaustion retains MetaML's `M0410` cancellation diagnostic. Zero checkpoints
cancel at the first evaluation checkpoint; depth must be positive. These limits
are independent of guest fuel. They do not bound parsing/typechecking, imported
source size or time spent inside host calls, and are not a sandbox for arbitrary
MetaML imports or host effects. Authoring remains trusted build-time code.
The compiler CLI bounds file reads to 16 MiB and atomically publishes only after
successful evaluation and verification, preserving previous output on failure.

## Definition authoring

`compile_definition` returns an owned, verified `Definition`. MetaML functions can
construct the entire fixed-width version-1 IR. `vmweave/examples/ToyR16.vmweave`
authors the same rules as `ToyR16.hpp`, including a reusable arithmetic-rule
function. Values returned by primitives are integer node references scoped to
the current rule. Operands must name earlier value-producing nodes in that rule.

| Primitive | Parameters/result |
|---|---|
| `vd_machine` | name, instruction bits, `"big"`/`"little"`, register count, register bits, memory bytes → Unit |
| `vd_rule` | name, mask, value → Unit; opens one rule |
| `vd_const` | i64 → reference |
| `vd_field` | bit shift, width, signed Bool → reference |
| `vd_read_register`, `vd_read_byte` | index reference → reference |
| `vd_binary` | operation string, two references → reference |
| `vd_write_register`, `vd_write_byte` | index reference, value reference → Unit |
| `vd_jump` | target reference → Unit |
| `vd_branch` | condition reference, target reference → Unit |
| `vd_halt` | result reference → Unit |
| `vd_end` | Unit → Unit; closes the current rule |

Binary names: `add sub mul div mod and or xor shl shr eq lt`. Here `and/or/xor`
are Definition's **bitwise** operations. Comparisons normalize to i64 0/1 and
branches test integer nonzero, explicitly converting at the Machinery truthiness
boundary. Field, state and reference arguments are checked before narrowing.
Register and byte writes wrap to their declared widths.

Existing Definition limits apply (4,096 rules, 4,096 nodes/rule, up to 65,536
guest code bytes). This embedding additionally caps total emitted nodes at
65,536, including nodes emitted by MetaML loops. Rules must be closed; nodes
after control transfers fail immediately. Final verification rejects duplicate
names and ambiguous masks. Lowering rejects unknown guest instructions and
dynamic, misaligned or out-of-range targets.

```sh
out-meta/vmweave-metaml --definition vmweave/examples/ToyR16.vmweave \
  guest.bin guest.mvmb
out-meta/vmweave emit-c guest.mvmb guest.c application_guest_factory
```

The lowered entry takes two Machinery arrays (registers and data bytes); use the
host setup in `tests/definition_factory.c`, rather than the scalar-only example
runner. Optional build tests exercise this complete MetaML → Definition → module
→ emitted C → C factory pipeline under both engines. This remains
**program-specialized lowering**, not a reusable generated native decoder.

## VMLibrary / SSAVM

See [SSAVM.md](SSAVM.md) for the complete current text/API/limit contract. Added
operations are logical AND/OR, wrapping negation and `bne/blt/bge/bgt`, with the
paper's array spelling aliases. The structural verifier now rejects references
to instructions that produce no result, negative argument indices and final
fallthrough. PHI/PFE simultaneous assignment and source-line diagnostics remain.

`ISSA::Limits` and `run_limited` add independent instruction, operand, source,
array-size, cumulative-element, allocation-count and execution ceilings. Empty
arrays consume allocation count. The original APIs and opcode identities remain.
New default operand/allocation ceilings reject resource-heavy programs that the
old implementation admitted. Cumulative budgets do not claim exact heap/RSS
accounting. Full SSA dominance and static type analysis remain future work.

## Validation and remaining work

See [VALIDATION.md](VALIDATION.md) for observed commands/results. Both dependency-
free and real MetaML tests retain original Machinery and catalog regressions.
Clang exposed a pre-existing misleading-indentation warning in CHIP-8 drawing;
explicit loop braces fix it without changing semantics. A narrowly scoped Clang
`unused-private-field` suppression surrounds only the unchanged supplied MetaML
header's unused parser bookkeeping field.

MetaTeX and libdrm hardware integration are still unvalidated. CHIP-8 quirk presets
and external ROM conformance, full guest loaders, reusable decoder generation,
package expansion and non-Linux validation remain open. Forty-four catalog
profiles still have no native guest execution.
