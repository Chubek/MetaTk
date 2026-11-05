# MetaMIR — Meta Machine IR

**Status: design proposal, not an implemented format or API.** This document
describes a future output boundary for [MetaBURG](MetaBURG/README.md), which
currently emits assembly. No MetaMIR parser, writer, or machine passes are
implemented.

## Purpose and pipeline

MetaMIR is a target-specific representation of selected machine instructions,
between MetaIL's scalar operations and final assembly. It preserves structured
operands and compiler metadata instead of flattening everything into assembly
text immediately.

```text
MetaIL AST ────────────────────┐
                              v
MetaIL text → existing parser → MetaBURG + target MetaArchDB + .mburg rules
                                  ├─ assembly output
                                  └─ MetaMIR → future machine passes → assembly
```

MetaBURG is an optional consumer: MetaIL's existing parsing, optimization,
analysis, and emission paths remain usable without it. Generated MetaIL files
can be parsed into the same AST representation before selection; they are not
another instruction-selection language. The canonical current Lua AST is
[`MetaIL/lua/metail/ir.lua`](MetaIL/lua/metail/ir.lua).

An `.mburg` file is an executable Lua program, not a separate BURG grammar.
The implemented `MetaBURG/lburg/lburg.cpp`, `lburg.hpp`, and `lburg.lua` provide
matching, cost calculation, selection, and target instruction facilities using
the MetaIL and MetaArchDB Lua interfaces. The intended native dependencies are
the workspace's `/mnt/nvme/metatk/third_party/lua` and
`/mnt/nvme/metatk/third_party/kaguya`, with configurable roots and pinned download
fallbacks. Executing rules requires trusting their Lua
code; neither a file extension nor this design implies sandboxing.

## What MetaMIR records

The proposed model has these layers:

| Record | Required information |
| --- | --- |
| Module | Format version, exact target identity and architecture-database revision, functions, external symbols, linkage requirements |
| Function | Name, calling convention, parameters/results, virtual registers, basic blocks, frame objects |
| Basic block | Stable label, ordered instructions, explicit terminator and control-flow successors |
| Instruction | Target instruction/form identity or explicit pseudo-opcode, named operands, explicit and implicit uses/definitions, effects, source origin |
| Operand | Tagged value: virtual register, physical register, immediate, block reference, symbol reference with addend, or frame reference |

Virtual registers carry a machine width and a target register-class constraint.
Physical registers use target identities rather than host register numbers.
Immediate values retain their full integer value: serialization must not lose
64-bit precision through floating-point numbers. Symbols and frame references
remain symbolic until the appropriate later lowering stage.

Instruction identity includes the selected form, not just its printed mnemonic:
a mnemonic may have multiple operand layouts and encodings. Named encoder
operands must come from explicit target/form bindings; do not infer positional
bindings. [MetaArchDB](MetaArchDB/README.md) supplies target descriptions, but
not a blanket guarantee that every form has sufficient semantics for selection.
Rules must supply missing legality or effect information, or reject the form.

## Selection and Lua-assisted dynamic programming

For tree selection, MetaBURG labels each AST node with the cheapest legal cover
for each goal (for example, a value in a register class or a legal immediate).
A candidate's cost is its local cost plus the costs of its required child goals.
Illegal candidates are excluded, and an uncovered required goal is an error.
Equal-cost choices need a stable rule order for reproducible output.

Lua predicates can consult target features and immediate constraints; Lua cost
functions can express target-dependent preferences. Costs must be comparable,
deterministic, and free of side effects that change later matching. A future
implementation must define the cost domain, tie-breaking, and termination rules
for goal-to-goal conversions explicitly.

After labeling, selection reconstructs the chosen cover and emits structured
instructions. Assembly and MetaMIR output should consume that same selected
sequence, so choosing an output format does not silently choose different rules.
Tree-local minimum cost is not a claim of globally optimal register allocation,
scheduling, or instruction selection across shared subexpressions and blocks.

## Illustrative selected function

The following is **notation only**, not an accepted `.mir` syntax or a Lua API.
Assume RV64 with an ABI that passes and returns this scalar in `a0`, and MetaIL
input `fn increment(x) { return x + 1; }`:

```text
function increment [target=riscv64, abi=chosen-rv64-abi]
  virtual %x, %result : i64, class=GPR
entry:
  %x = COPY_FROM_PHYS a0
  %result = riscv64:addi { rd=%result, rs1=%x, imm=1 }
  COPY_TO_PHYS a0, %result
  RETURN
```

`COPY_FROM_PHYS`, `COPY_TO_PHYS`, and `RETURN` stand for proposed pseudos whose
uses, definitions, and ABI effects must be specified. They are not claimed to
be MetaArchDB instructions. A later pass would assign physical registers,
resolve copies, lower the return, and validate the concrete target operands.
The source variable name is not itself a physical register assignment.

## Semantic requirements

MetaMIR lowering must preserve MetaIL's current scalar signed-64 semantics,
including wraparound arithmetic, division/remainder and shift trap contracts,
and observable effects. A narrower target needs multi-instruction sequences or
helpers when one instruction cannot implement an operation correctly.

Calls, traps, implicit flags, and implicit register clobbers must remain visible
to subsequent passes. Never discard, duplicate, or reorder an effectful or
potentially trapping expression merely because a cheaper tree cover exists.
Lazy `select` arms must remain conditional; evaluating both arms speculatively
requires proof that doing so is safe. Calls require explicit ABI lowering,
including argument/result locations, preserved registers, and stack alignment.

MetaIL currently permits variable reassignment. Virtual-register naming alone
does not establish SSA. This proposal neither requires SSA nor claims that
MetaIL already implements it. Memory SSA, general alias analysis, a complete
type system, and native execution are outside the current MetaIL scope.

## Validation and eventual consumers

A future verifier should reject unresolved instruction identities, illegal
operand kinds/ranges/classes, undefined registers, missing labels, inconsistent
terminators, and unlowered pseudos at final emission. Diagnostics should retain
the originating MetaIL node and `.mburg` rule where available. Unsupported
operations must fail explicitly rather than emit plausible but incorrect code.

MetaMIR would permit later register allocation, spill/frame lowering, scheduling,
ABI expansion, and assembly printing without rerunning tree matching. Those
passes are future work. Assembly output is not an object file; symbol resolution,
relocations, and final encoding still require an assembler/linker or a separately
implemented encoding pipeline.

Before implementing a persistent format, settle the extension and versioning,
serialization grammar, stable target/form identifiers, pseudo-instruction set,
effect model, and pre-/post-allocation verification rules. The implemented
[MetaBURG manual](MetaBURG/manual/README.md) and
[twelve examples](MetaBURG/examples/README.md) describe the current selector and
assembly path, not an implemented MetaMIR backend.
