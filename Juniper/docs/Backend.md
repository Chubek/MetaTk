# Backend contracts

## Selection

`juniper::Backend` is an ordinary C++20 value. Profiles can be hard-coded in C++,
embedded as declarations, or generated with GPPX. No configuration parser runtime
is needed. Portable selection covers every MetaIL scalar operation; calls and
lazy selects are structural operations, not user-overridable arithmetic rules.

Tree covering computes minimum total cost for each expression. A rule's cost is
added to the selected cost of each `reg` capture. A `reg` captures one complete
subtree; `imm(lo,hi)` captures only an in-range literal at zero extra cost. Nested
operator patterns permit fusion. Captures become instruction operands in
left-to-right, depth-first order. Literal selection uses an `imm` root and emits
`CONST`. Equal-cost ties use declaration order, making output reproducible.
There is no operand commutation, algebraic rewrite, hidden constant materializer,
or fallback when a profile does not cover a node.

`select(module, metail_home)` first validates through the real MetaIL runtime;
the optional runtime root follows the same rules as `Compiler(metail_home)`.
Use it when embedding with a runtime outside the standard installation layout.

**A rule is a backend semantic assertion:** its opcode must implement exactly its
pattern under signed-64 semantics. Juniper does not prove arbitrary opcodes correct.
Every capture is evaluated once in source order. Lazy selects and nested trapping
patterns are forbidden, preventing fusion from moving traps past captured calls.

Variables and temporary results are virtual registers. User identifiers cannot
collide with generated `$vN` registers or `$selectN_*` labels. `COPY`, `CALL`,
`RET`, `ASSUME`, `BR`, `JUMP` and `LABEL` are reserved structural opcodes. Calls,
assumptions, control instructions and potentially trapping operations are full
barriers. `select` lowers to `BR`, labels, arm-local code and a shared result.

## Scheduling

The scheduler constructs explicit RAW, WAR and WAW dependencies, plus full fences
around effects, control and traps. This is important because MetaIL locals are
mutable. The dependency graph uses original instruction IDs, even after reordering.
Critical-path priorities choose among ready instructions; ties use original IDs.
Latency controls result availability; issue width caps total launches per cycle;
each named resource caps launches per cycle. Resources are modeled as pipelined,
not reserved for the full latency. Structural control pseudos conservatively use
one cycle and an issue slot. Scheduling never crosses a fence or a basic block.
It is a bounded, deterministic local model, not real out-of-order simulation.

`select` and `schedule` emit JSON schema 1 with module/backend, issue width,
resource capacities, functions, parameters, and instructions. Each instruction
contains original ID, source statement index (zero-based), opcode, result,
typed operands (`reg`, `imm`, `label`, `symbol`), resource, latency, barrier,
dependencies and cycle (`null` before scheduling). JSON is an inspection artifact;
the round-trippable source IR is produced by `emit`, not by `select`.

## MetaArch boundary

Enable `-DJUNIPER_WITH_METAARCH=ON` to build/test `Juniper::MetaArch` in a source
build. This optional adapter adds no mandatory runtime dependency. MetaArch uses
its existing system-package/FetchContent dependency policy. Its current DSL target
does not provide a standalone exported CMake package; installed adapter users
must supply/link MetaArch themselves. The ordinary Juniper package remains
independent of it.

```cpp
#include <juniper/MetaArch.hpp>
juniper::FormBinding mapping{form_index, {"rd","rs1","imm"}, {-1,0,1}};
juniper::RegisterBindings registers{{"$v0","x10"}, {"x","x11"}};
auto encoded = juniper::encode_instruction(spec, instruction, mapping, registers);
```

`-1` means the instruction result; nonnegative values select instruction operand
indices. `parameters` explicitly names encoder parameters in **form operand
declaration order**, not encoder bytecode first-use order. The adapter calls the
real `metaarch::Spec::encode_form`, preserving architecture kind/class/width,
immediate bounds, target feature checks and diagnostics. Returned bytes and
relocations are owned values. Unknown physical assignments, mapping errors,
labels and symbols fail explicitly.

The caller must provide valid physical register assignments and any ABI/control
lowering. This API encodes selected instructions; it does not silently turn a
virtual-register function into executable machine code. No architecture database
files, generated backends, encoders or ISA semantics are modified by Juniper.
