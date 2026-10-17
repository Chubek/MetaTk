# MLVM: integer core and architecture backends

MLVM's implemented core lives in `MLVM/source/*.qs`. Parsing, canonical IR,
control-flow verification, CFG/data-flow analysis, execution, optimization, serialization and target
lowering are QScript libraries. `MLVM/tooling/mlvm.py` only handles files,
argument parsing and launching QScript; it does not implement those algorithms.
This is the first integer-core milestone, **not** an implementation of every
dialect, subsystem or optimization named by the placeholder directories.

## Running it

From the repository root, with a built `build/qscript`:

```sh
python3 MLVM/tooling/mlvm.py check MLVM/tests/sum.mlvm
python3 MLVM/tooling/mlvm.py run MLVM/tests/sum.mlvm --optimize
# Prints 15. --jit runs the QScript implementation using MetaJIT.
python3 MLVM/tooling/mlvm.py opt MLVM/tests/sum.mlvm
python3 MLVM/tooling/mlvm.py analyze MLVM/tests/sum.mlvm
python3 MLVM/tooling/mlvm.py opt MLVM/tests/diamond.mlvm --global-optimize
python3 MLVM/tooling/mlvm.py emit-qscript MLVM/tests/sum.mlvm -o /tmp/opencode/sum.qs
python3 MLVM/tooling/mlvm.py build MLVM/tests/sum.mlvm -o /tmp/opencode/sum
# Linux x86-64 executable; exits with 15 and needs no QScript installation.
python3 MLVM/tooling/mlvm.py emit-riscv64 MLVM/tests/answer.mlvm -o /tmp/opencode/answer.bin
```

`--qscript PATH` (or environment variable `QSCRIPT`) selects the QScript driver.
`--steps N` bounds MLVM instructions, including the final `ret`; the default is
100,000. `--host-steps N` independently bounds QScript execution (10,000,000
by default). Verification, optimization and parsing also consume the host
budget. Large modules may require a larger host budget. Input is limited to
1 MiB. `check` verifies without executing; `opt` always applies the optimizer.
`--optimize` enables block-local propagation before the other operations.
`--global-optimize` instead selects global must-constant propagation. These
flags are mutually exclusive; `opt` defaults to the local pass. `analyze`
prints a JSON CFG/data-flow report; with an optimization flag it reports the
optimized module. `object` emits an AMD64
ELF relocatable object via QScript. Commands return 0 on success, 1 on errors,
and 2 on argument errors; `run` prints the result rather than using it as a
process exit code. Output commands accept `-o`; binary/native commands require it.

## Textual IR and semantics

```text
module 1 {
    const r0 20;
    const r1 22;
    add r2 r0 r1;
    ret r2;
}
```

`module 1` identifies the format version. A module is one no-argument integer
entry function, not a collection of named functions. Each instruction ends in
`;`. Whitespace and `#` line comments are accepted. Registers are `r0..r31`.
Immediates are signed decimal 64-bit integers; hexadecimal and symbolic labels
are not accepted. Targets are zero-based instruction indices, not byte offsets.

| Instruction | Meaning |
|---|---|
| `const D IMM` | assign immediate |
| `mov D A` | copy register |
| `add/sub/mul/div/mod D A B` | integer arithmetic |
| `and/or/xor D A B` | bitwise arithmetic |
| `shl/shr D A B` | shift left / logical shift right |
| `eq/lt D A B` | equality / signed less-than, yielding 0 or 1 |
| `jmp TARGET` | unconditional branch |
| `jz A TARGET` | branch if zero, otherwise fall through |
| `ret A` | return register |

Addition, subtraction and multiplication wrap at 64 bits. Division truncates
toward zero; remainder has the dividend's sign. Zero divisors, `INT64_MIN / -1`
and `INT64_MIN % -1`, and shift counts outside 0..63 trap, matching QScript.
Registers have no implicit initial value: every read must be definitely
assigned on every reachable path. There is no implicit return or legal
fallthrough past the final instruction. Infinite loops are permitted but
bounded during execution.

## Library strata and contracts

Load libraries with `-L MLVM/source`; native target libraries additionally use
`-L MLVM/targets -L MLVM/generated`. Imports are explicit and non-transitive.

| Library | Public operations | Dependencies |
|---|---|---|
| `ir` | opcode constants, `names`, `opcode`, `writes`, `reads_left`, `binary`, `zeros`, `evaluate` | none |
| `parser` | `parse(str): list<list<int>>`, `Text` grammar | `ir` |
| `control_flow` | `verify`, `successors`, `reachable`, `predecessors`, `reverse_postorder`, `blocks`, `immediate_dominators` | `ir` |
| `dataflow` | `liveness(code)`, `report(code): str` | `ir`, `control_flow` |
| `register` | `interference(code): list<int>`, `color(code, capacity): list<int>` | `ir`, `control_flow`, `dataflow` |
| `compute` | `run(code, max_steps): int` | `ir`, `control_flow` |
| `optimization` | `fold(code)`, `constants(code)`, `propagate(code)` | `ir`, `control_flow` |
| `serialize` | `text(code): str` | `ir`, `control_flow` |
| `bytecode` | `encode(code): list<int>`, `decode(words): list<list<int>>` | `ir`, `control_flow` |
| `metair` | `import_mcil(bytes: str)`, `export_mcil(code): str` | `ir`, `control_flow` |
| `backend` | `qscript(code, max_steps): str` | `ir`, `control_flow` |
| `pipeline` | `compile(text, optimize)`, `run(text, max_steps, optimize)`; mode 0: none, 1: local, 2: global | parser, verifier, optimizer, executor |
| `amd64::amd64` | `lower(code, max_steps): str` | `backend` |
| `riscv64::riscv64` | `emit(code): list<int>` | IR, verifier, allocator, generated Arch encoder |

Canonical IR is a typed `list<list<int>>`. Each instruction has exactly five
fields `[opcode, destination, left, right, immediate]`; unused fields must be
zero. Opcode numbers are 0..16 in the instruction-table order, with arithmetic
and bitwise operations expanded in their displayed order. Modules contain
1..4,096 instructions. Register operands are checked even in unreachable code;
all branches and fallthroughs must be in range. `successors` requires a
structurally valid instruction; use `verify` on untrusted modules. Other public
consumer entry points verify before doing their work.

The verifier propagates defined-register bitsets to a fixed point, intersects
at joins and checks reads only after convergence. Entry always has the empty
definition set, including when a backedge targets instruction zero. The
local optimizer propagates constants within basic blocks, resetting at every branch
target and after terminators. It preserves instruction indices and step counts,
does not mutate its input or alias its rows, and leaves trapping expressions
unfolded. The global pass described below crosses basic-block boundaries with
a conservative fixed-point analysis. Neither pass performs DCE, SSA conversion
or the other planned passes in `MLVM/optimize/MANIFEST.json`.

Bytecode is a **word stream**, not a dump of host memory: magic `0x4d4c564d`,
version `1`, instruction count, then five integer words per instruction.
Decoding bounds the count before arithmetic/allocation, requires exact length,
copies the input and verifies the result. A binary file container and external
byte ordering are not defined by this version.

### CFG and data-flow analyses

All analysis entry points verify the module first, except the low-level
`successors` primitive. Returned lists are owning analysis results, not aliases
of input instructions. CFG reachability is **syntactic**: both `jz` edges are
included even if the condition is provably constant. A conditional branch whose
target equals its fallthrough has one successor, not two duplicate edges.

* `reachable(code): list<int>` gives one 0/1 flag per instruction from entry 0.
* `predecessors(code): list<list<int>>` gives incoming instruction indices in
  ascending source order. This structural graph includes unreachable edges;
  dominance and data-flow passes exclude unreachable predecessors.
* `reverse_postorder(code): list<int>` returns reachable instructions in reverse
  DFS postorder, following the branch-target edge before the fallthrough edge.
  DFS uses an explicit stack, not recursive QScript calls.
* `blocks(code): list<list<int>>` partitions the entire instruction sequence
  into `[start, end_exclusive]` ranges, including unreachable blocks. Entry,
  branch targets, and instructions following terminators are block leaders.
* `immediate_dominators(code): list<int>` uses a reverse-postorder fixed point
  with linear graph storage. The entry dominates itself (`idom[0] == 0`),
  unreachable instructions have `-1`, and every other element names its
  immediate dominator. Self-loops, backedges to entry and irreducible CFGs
  are supported; no assumption of a structured/reducible CFG is made.
* `liveness(code): list<list<int>>` returns `[live_in, live_out]` register-bitmask
  pairs, one per instruction. Bit `r` represents virtual `r0..r31`, so masks
  fit in the low 32 bits of a QScript integer. Unreachable pairs are `[0,0]`.
  The backward fixed point includes uses before killing a destination, so
  `add r0 r0 r1` correctly keeps both incoming operands live.

`dataflow::report(code)` serializes these results to JSON in QScript. Fields
are `instruction_count`, `reachable_count`, `block_count`, `edge_count`,
`reachable`, `predecessors`, `reverse_postorder`, `blocks`,
`immediate_dominators`, and `liveness`. Block and edge counts describe the whole
structural graph; reachability and liveness distinguish its dead parts.

### Global constant propagation

`optimization::constants(code)` returns the **incoming** state at each
instruction: `[reachable, known_mask, value_r0, ..., value_r31]` (34 integers).
A set mask bit means every syntactically reachable predecessor agrees on that
constant. Unequal constants, or an unknown value on any predecessor, produce
an unknown value at the join. Unknown slots are canonical zero; an unreachable
state is 34 zeros. The entry has no known registers, even with a backedge to
instruction 0. States are independent lists, not repeated aliases.

The transfer function evaluates known, non-trapping arithmetic with QScript's
wrapping integer semantics. Division/remainder traps and invalid shifts are
never evaluated during compilation. Both edges of a conditional participate;
this is a must-constant analysis, not sparse conditional constant propagation
or path-sensitive reasoning. A changing loop induction variable becomes
unknown while loop-invariant constants can survive the join.

`optimization::propagate(code)` rewrites known writes to `const` **only after
convergence**. It returns a fresh module with independent instruction rows,
leaves unreachable instructions copied unchanged, and verifies its output.
It never deletes, inserts or redirects instructions: target indices, trap
locations and MLVM instruction budgets are preserved. It is safe to use before
any existing backend. The analysis uses 34 words per instruction plus graph
storage; execution/heap limits still apply to fixed-point work.

### Register interference and allocation

`register::interference(code)` returns 32 adjacency bitmasks. Live-in values
interfere pairwise; each write additionally interferes with other live-out
values. This includes **dead destination writes**, which must not clobber a
different live value if the instruction remains emitted. The graph is
symmetric with no self edges, and unreachable instructions contribute nothing.

`register::color(code, capacity)` assigns virtual registers to abstract colors
`0..capacity-1`, with `-1` for unused or unreachable-only registers. Capacity
must be 1..32. It greedily chooses highest-degree uncolored vertices, breaks
ties by lowest virtual index, then chooses the lowest free color. Results are
deterministic. Disjoint live ranges may share a color, including a destination
and a dying input. Allocation has no architecture names or ABI assumptions;
the target maps colors to its Arch-provided physical register pool.

This allocator does **not** split live ranges, spill or guarantee an optimal
coloring. Insufficient capacity (or a greedy failure) is an explicit
`register allocation exhausted` error. It does not silently overwrite live
values or fall back to the VM.

## Architecture backends

### AMD64: delegate encoding and ABI ownership

Portable lowering emits QScript source with the same register state, branches,
arithmetic and instruction budget. QScript's VM, JIT and C99 backends can run
that source. `qscript build` and `object` select the existing Linux AMD64
ASMtk/LDtk path. MLVM adds no handwritten AMD64 register or opcode tables.
This is a correctness-first state-machine lowering, not a new optimizing
native instruction selector. It retains the existing QScript limits.

### RV64: bounded Arch-generated leaf code

`Arch/dsymspec/tools/generate_mlvm.py` validates
`Arch/dsymspec/riscv64.dsymspec.yaml`, reads its backend ABI policy, and generates
`MLVM/generated/arch/riscv64.qs`. The generator copies audited single-word
`EMIT32LE` encoder expressions, obtains immediate bounds from forms, and
derives physical/argument/result registers from the specification and its ABI
policy. It rejects unknown mechanisms rather than executing arbitrary encoder
programs. Python `pyyaml` and `jsonschema` are regeneration dependencies only.

```sh
python3 Arch/dsymspec/tools/generate_mlvm.py
python3 Arch/dsymspec/tools/generate_mlvm.py --check
```

The target emits a no-argument RV64IM leaf function returning in the Arch ABI
result register. The interference allocator maps **any virtual `r0..r31`** to
colors, then the backend maps those colors to argument registers from the
Arch policy (currently eight). Physical registers are checked against reserved
and callee-saved registers; there is no spilling. High virtual indices are
allowed, and disjoint lifetimes reuse physical registers. Supported operations
are `const` (signed 12-bit immediate), `mov`,
`add`, `sub`, `mul`, `and`, `or`, `xor`, `lt`, and `ret`. Emission ends at the
first return; later unreachable instructions are structurally verified but not
emitted. This produces raw little-endian instruction bytes, **not ELF**, a
Linux process, or a bounded interpreter. There is no VM fallback. Branches,
division, remainder, shifts, equality, large constants and exhausted allocation
are explicit errors. Control flow and traps need a proper target ABI before
support can be added. RV64 bytes are reference-assembled with LLVM when
available; they are not executed on the x86-64 test host.

The [MetaIR bridge](mlvm-bridge.md) embeds these QScript libraries behind the
C99 `MLVM-Bridge.h` API and exchanges the shared integer subset through MCIL.
Its optional C++ adapter imports/exports real `metail::Program` objects.
Other target/dialect directories, placeholder C headers, LTO, memory, FFI, GPU and plugin
subsystems remain unimplemented. Do not treat an empty `.qs` file that passes
`qscript check` as an implementation.

## Tests and current checkout limitations

```sh
python3 tests/mlvm.py build/qscript cc
python3 tests/mlvm_dataflow.py build/qscript
python3 tests/mlvm_runtime.py build/qscript cc build/libqscript_generated_fixture.so ON OFF
python3 tests/mlvm_arch.py
python3 tests/qscript_strata.py
python3 Arch/dsymspec/tools/generate_mlvm.py --check
```

CTest registers these as `metachain.mlvm`, `metachain.mlvm.dataflow`,
`metachain.mlvm.runtime`, `metachain.mlvm.bridge` and
`metachain.mlvm.arch.generated` (the latter when YAML/schema Python modules
are available). `metachain.qscript.strata` checks the ownership audit's positive
and negative configure paths. Tests cover VM/JIT parity, loop and join verification, malformed
input, arithmetic traps, optimization aliasing, text/word round trips, generated
QScript on VM/JIT/C99/native, launcher I/O and RV64 reference bytes. Independent
small-graph set-based oracles check dominance, CFG, liveness and interference
on 26 deterministic/seeded cases (including irreducible loops), with the analyses
running on VM/JIT. An independent bounded executor checks constant facts and
compares original, optimized and register-allocated programs. The runtime test also compiles the MLVM
QScript implementation itself, including its parser, analyses and optimizer,
to linked C99 and (when enabled) static native executables. It is not limited
to testing the programs MLVM emits.

The current checkout has an unfinished migration from deleted
`Arch/stdspec/` files to `Arch/dsymspec/`. ASMtk still includes
`Arch/stdspec/asmtk/AMD64.hpp`; CMake's backend also references a missing
`MetaIR/satie/BackendSAT.cpp`. Those block a fresh full build. Existing
`build/qscript` can exercise these QScript sources, but results from that
pre-existing binary do not establish a successful clean rebuild. The migration
must be completed before claiming the whole repository builds and passes.
