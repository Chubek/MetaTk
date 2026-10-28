# 26. Immediates, coverage, and legalization boundaries

An immediate constraint matches a literal node, inclusively between two signed-64
decimal bounds. It is not a theorem prover for arbitrary expressions with a known
range, and it does not insert a target-specific instruction sequence on failure.

## Use a fallback deliberately

<!-- check: juniper mode=select opcodes=ADDI,ADD,CONST -->
```juniper
backend immediate_demo extends portable {
  rule add_byte : add(reg, imm(-128, 127)) => ADDI cost 1;
}
fn fits(x) {
  return add(x, 127);
}
fn outside(x) {
  return add(x, 128);
}
```

`fits` uses the immediate rule. `outside` falls back to the inherited literal
materialization and register-register `ADD`. This fallback exists because the
profile explicitly inherited portable rules, not because the selector invents
an encoding when an immediate is out of range.

An expression such as `add(x, add(3, 4))` is not directly a literal-operand match.
Run `ConstantFolding` first if you want that subtree to become literal `7`:

```sh
"$J" select "$WORK/immediate-expression.jun" --optimize \
  --passes ConstantFolding -o "$WORK/immediate-expression.json"
```

Write the expression into a complete function with the preceding profile before
using that command.

## A minimal profile has no hidden coverage

<!-- check: juniper mode=select opcodes=ADDI -->
```juniper
backend immediate_only {
  rule only_add : add(reg, imm(-8, 7)) => ADDI;
}
fn increment(x) {
  return add(x, 1);
}
```

This works without a literal rule because `1` is directly captured as an
immediate. Return is structural, and the parameter is already a virtual register.
But an out-of-range value in the same profile is deliberately rejected:

<!-- check: juniper mode=select error=JUN-E300 -->
```juniper
backend immediate_only {
  rule only_add : add(reg, imm(-8, 7)) => ADDI;
}
fn unsupported(x) {
  return add(x, 8);
}
```

`check` can still validate its IR: selection coverage is a later property. Use
`select` in backend acceptance tests, not only `check`.

## Design a legalization policy outside selection

For a real target, decide which values can be immediate operands, which require
constant materialization, and which operations need multi-instruction lowering.
Then implement that lowering explicitly or reject the unsupported subset.
Naming a profile after a 32-bit processor does not change Juniper's signed-64
semantics into 32-bit arithmetic.

For a signed `k`-bit immediate, test `-2^(k-1)`, `-1`, `0`, `1`, `2^(k-1)-1`,
and the two adjacent out-of-range values. Also test that optimizing before
selection does not produce uncovered operators. MetaArch form checks provide a
second encoding-time boundary; they do not replace a deliberate selection policy.

[Previous](25-selection-costs.md) · [Index](README.md) · [Next](27-fusion.md)
