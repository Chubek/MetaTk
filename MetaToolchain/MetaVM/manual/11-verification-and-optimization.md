# Chapter 11 — Verification, rewriting, and optimization

[Previous](10-assembly-language.md) · [Contents](README.md) · [Next](12-execution-engines.md)

## What structural verification establishes

`mvm_module_verify` checks the entire module, including unreachable instructions.
It rejects unknown opcodes, empty functions, invalid local registers, bad
function/import indices, impossible argument windows, and branches outside their
function. It also rejects a function whose final instruction can fall through
past the end. Adding a return on a reachable path does not excuse malformed
unreachable code after it.

The verifier checks representation and control-flow structure, not a complete
language type system. For example, structurally valid `add r2 r0 r1` may still
receive nil or an object at runtime. Division by zero, array bounds, missing
host imports, fuel exhaustion, and allocation failure remain runtime concerns.
Do not translate “verified” into “cannot trap.”

Compilation, deserialization, serialization, optimization, and execution verify
their inputs at their appropriate boundaries. A compiler constructing modules
directly should nevertheless call verification explicitly before publication;
that localizes errors to its own output stage.

## Safe single-instruction replacement

```c
mvm_status status = mvm_module_replace(module, function_index, pc,
                                     replacement, &error);
```

Replacement verifies the whole resulting module. If validation fails, the old
instruction is restored. The operation does not permit changing an immutable
module concurrently with readers, even when the new instruction looks local.
Borrowed code views should be reacquired after mutation.

Use replacement for intentional compiler rewrites, not for bypassing branch or
register checks. A branch to an arbitrary byte offset is not valid: Machinery
targets are instruction indices within one function.

## Conservative optimization

`mvm_optimize(module, &changes, &error)` performs an instruction-count-preserving
peephole pass. It replaces self-moves and jumps to the next instruction with
NOP and folds supported adjacent constant operations within a basic block.
Branch entry points prevent unsafe folding across alternate predecessors.
Trapping operations are not folded away.

Preserving instruction positions preserves branch targets and step-budget
accounting. A NOP still consumes one instruction step. Do not expect this pass
to remove unreachable blocks, perform global register allocation, inline calls,
prove termination, or generate native code. Failure leaves the module unchanged;
the change-count pointer may be null when the count is unneeded.

## External compiler helpers

`mvm_select_integer_operation` maps operation names to Machinery opcodes;
`mvm_fold_integer_operation` evaluates a selected pure scalar operation using
the same arithmetic contract. Supported selection names are `add`, `sub`, `mul`,
`div`, `mod`, `and`, `or`, `xor`, `shl`, `shr`, `eq`, `lt`, and `le`.

The `MetaIL-ISel.h` and `MetaIL-Rewrite.h` names describe a narrow integration
surface. They do not parse the MetaIL language, link its Lua runtime, or provide
compatibility with an unrelated historical C++ `Program`/`Frame` interface.

**References:** [replacement API](../Machinery/AbstractMachine-Rewrite.h),
[optimizer](../Machinery/Optimization.h), [SSA analysis](30-ssavm-analysis.md).
