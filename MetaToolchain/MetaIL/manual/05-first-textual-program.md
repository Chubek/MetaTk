# 5. Your first textual program

Save this program as `scale.mi`:

```text
module demo;
fn scale(x) {
entry:
  return (x + 0) * 8;
}
```

Check it, emit canonical formatting, then select two transformations:

```sh
build/metail check scale.mi
build/metail emit scale.mi
build/metail optimize scale.mi \
  --passes AddZeroRight,MultiplyByPowerOfTwoRight03
```

The selected rules remove the addition and replace multiplication by eight with
a left shift by three. The result contains `return (x << 3);`. These rewrites are
valid for the signed-64 wraparound semantics, including values that overflow
mathematical integer arithmetic.

Use `--out optimized.mi` to save the emitted program. Use `--stats changes.json`
with `optimize` to inspect per-pass change counts. Analyze the original independently:

```sh
build/metail analyze scale.mi --passes ControlFlowGraph,CountMulOperations
```

The CLI reports analyses as JSON. It does not execute `scale` or produce machine
code. `check` verifies the implemented structural and CFG rules; it is not a proof
of definite assignment or call-signature correctness. For broader examples see
`examples/arithmetic.mi` and `examples/dataflow.mi`.

[Contents](README.md) · [Next: First native program](06-first-native-program.md)
