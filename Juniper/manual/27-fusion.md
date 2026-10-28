# 27. Fused instructions without changing semantics

A fused rule asserts that one backend opcode implements a whole operator tree.
Juniper checks the pattern's shape and safety restrictions; it does **not** prove
an arbitrary custom opcode's arithmetic implementation correct.

## Start with a wrapping multiply-add

<!-- check: juniper mode=select opcodes=MADD -->
```juniper
backend multiply_add extends portable {
  rule fused : add(mul(reg, reg), reg) => MADD cost 3 latency 4;
}
fn compute(x, y, bias) {
  return add(mul(x, y), bias);
}
```

`MADD` must compute the same signed-64 bit pattern as wrapping multiplication
followed by wrapping addition. It must not unexpectedly saturate, trap on signed
overflow, widen its exposed result, or use a different evaluation order for
captured effectful arguments.

The pattern's leaves are `x`, `y`, and `bias` in that order. For calls in those
positions, selection evaluates the captured subtrees in source order before
emitting the fused instruction. It neither duplicates the calls nor commutes
arguments to obtain a cheaper match.

<!-- check: juniper mode=select opcodes=MADD,CALL -->
```juniper
backend observed_fusion extends portable {
  rule fused : add(mul(reg, reg), reg) => MADD cost 3 latency 4;
}
fn observe() {
  return add(mul(first(), second()), third());
}
```

For this specific arithmetic fusion, postponing the pure wrapping multiply until
the fused instruction does not introduce a trap. The ordered calls remain full
barriers. Do not extrapolate that reasoning to a nested operation that can trap.

## Why some patterns are forbidden

The following **deliberately invalid** profile tries to cover a nested division:

<!-- check: juniper error=JUN-E102 -->
```juniper
backend unsafe_fusion extends portable {
  rule nested_division : add(div(reg, reg), reg) => DIV_ADD;
}
fn example(x, y, z) {
  return add(div(x, y), z);
}
```

If the third captured operand were a call, a single fused instruction could
postpone the division trap until after that call. Juniper rejects nested
`div`, `mod`, `shl`, and `shr` in patterns. Such operators may appear at the rule
root, where ordered operand evaluation remains separate.

`select` is prohibited anywhere in a rule because its arms are lazy. Calls are
structural operations, not pattern operators. Structural opcode names `COPY`,
`CALL`, `RET`, `ASSUME`, `BR`, `JUMP`, and `LABEL` cannot be user rule opcodes.

## Build a semantic test, not just an opcode-name test

A backend test should compare separate and fused arithmetic over boundary inputs,
record effect traces where calls are captured, and compare trap outcomes. Also
assert the selected opcode and leaf operand order. A test that merely finds
`MADD` in JSON proves the rule matched, not that its eventual implementation is
correct.

Cost/latency tuning comes after those checks. An attractive schedule is not
evidence that a fused instruction preserves the program's semantics.

[Previous](26-immediates-and-coverage.md) · [Index](README.md) · [Next](28-machine-ir.md)
