# 25. Tree patterns and selection costs

Selection covers expression trees. It is not a textual mnemonic substitution,
greedy root-opcode lookup, or cost-based scheduling pass.

## Three pattern building blocks

* `reg` captures an entire subtree that must be computed into a virtual register.
* `imm` or `imm(lo, hi)` captures a literal directly as an instruction operand.
* `operator(patterns...)` covers that exact operator shape with its canonical arity.

The root cannot be `reg`: that would ask selection to cover the same whole tree
recursively. Literal rules use an `imm` root instead.

<!-- check: juniper mode=select opcodes=ADDI -->
```juniper
backend cost_example extends portable {
  rule small_add : add(reg, imm(-16, 15)) => ADDI cost 1;
}
fn add_seven(x) {
  return add(x, 7);
}
```

For `add(x, 7)`, the portable register-register cover costs `2 + 0 + 1 = 3`:
the add rule costs 2, reading `x` is free, and materializing `7` costs 1. The
immediate cover costs `1 + 0 = 1`; the captured immediate adds no materialization
cost. Source return/control instructions are not choices in this arithmetic
cover calculation.

## Nested covers compare total costs

<!-- check: juniper mode=select opcodes=MADD -->
```juniper
backend fused_cost extends portable {
  rule multiply_add : add(mul(reg, reg), reg) => MADD
    cost 3 latency 4 resource alu;
}
fn combine(x, y, z) {
  return add(mul(x, y), z);
}
```

The separate portable multiply/add costs 4. `MADD` costs 3 and consumes three
already-available parameter values, so it wins even though its root rule cost is
higher than the standalone add rule's cost 2. The latency 4 does not alter that
selection decision; it affects scheduling later.

## Ordering and ties are contracts

Captured leaves become operands in left-to-right depth-first order. The previous
`MADD` captures `x`, then `y`, then `z`. No automatic commutation turns
`add(z, mul(x, y))` into that pattern. Add a separate valid pattern if your backend
supports the other form.

If the appended fused rule instead costs 4, the earlier portable cover wins the
tie. Keep rule order stable in canonical IR and tests. Lower latency alone does
not win an equal-cost selection tie.

Pattern constructors express the same shapes in C++:

```cpp
using P = juniper::Pattern;
juniper::Rule fused{
    "multiply_add",
    P::operation("add", {P::operation("mul", {P::reg(), P::reg()}), P::reg()}),
    "MADD", 3, 4, "alu"
};
```

## Costs are not physical performance measurements

Choose and document a cost policy: code size proxy, target-specific preference,
or another deterministic weight. Do not interpret portable costs as measured
processor cycles. Selection performs no global DAG sharing, common-subexpression
elimination, implicit legalization, physical register allocation, or ABI lowering.
Repeated subexpressions remain repeated work unless a prior valid transformation
or your source makes sharing explicit.

[Previous](24-backend-profiles.md) · [Index](README.md) · [Next](26-immediates-and-coverage.md)
