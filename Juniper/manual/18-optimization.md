# 18. Using the real MetaIL optimizer

Juniper does not maintain a competing arithmetic optimizer. Its `optimize`
operation passes owned structured IR through the actual MetaIL runtime and
returns a new owned module, preserving the Juniper backend profile.

## Begin with a small, observable change

<!-- check: juniper -->
```juniper
@module(optimize_me)
@fn(scale, (x))
  @let(value, add(x, 0))
  @return(mul(value, add(2, 2)))
@end
```

After saving it as `$WORK/optimize.jun`:

```sh
"$J" metail "$WORK/optimize.jun" -o "$WORK/before.mi"
"$J" optimize "$WORK/optimize.jun" \
  --passes ConstantFolding,AddZeroRight --rounds 2 -o "$WORK/after.junir"
"$J" metail "$WORK/after.junir" --no-preprocess -o "$WORK/after.mi"
diff -u "$WORK/before.mi" "$WORK/after.mi"
```

`AddZeroRight` removes the `add(x, 0)` expression; `ConstantFolding` simplifies
`add(2, 2)`. An explicit pass list need not perform copy propagation or every
other cleanup you might expect. Compare the actual output rather than promising
an unrelated transformation.

## Select passes and rounds deliberately

The default list is `all`; the default round count is `2`. Allowed rounds are
`1..12`. Names are exact and case-sensitive, separated by commas. Do not use
descriptive lower-case names in place of registered kernel names.

```sh
"$J" optimize "$WORK/optimize.jun" --passes all --rounds 2 \
  -o "$WORK/default-pipeline.junir"
"$J" select "$WORK/optimize.jun" --optimize \
  --passes ConstantFolding,AddZeroRight -o "$WORK/optimized-selection.json"
```

Juniper has no `kernels` command. If you built MetaIL as a source dependency, its
separate executable can discover the installed runtime's catalog:

```sh
"$JUNIPER_ROOT/build/metail/metail" kernels --search 'multiply power' --json
"$JUNIPER_ROOT/build/metail/metail" verify
```

Use the appropriate installed `metail` binary if there is no source-dependency
executable in that path. Kernel counts are versioned runtime inventory, not a
hard-coded guarantee of the Juniper language.

## Preserve the effect contract

<!-- check: juniper -->
```juniper
@fn(not_just_zero, ())
  @let(unused, tick())
  @return(mul(tick(), 0))
@end
```

Correct optimization must preserve the call evaluations. MetaIL guards rewrites
that would drop, duplicate, or reorder observable work. Do not emulate a desired
optimization with a macro that bypasses those checks.

Optimization can change pattern shapes. A profile that matches `mul(x, 8)` may
not match an optimized `shl(x, 3)` expression. Test **both** the selected pass
pipeline and the profile's coverage; do not assume adding more passes can never
make a deliberately narrow backend fail.

Weighstone integer passes provide conservative block-local facts and proof-guided
simplifications. They are not SSA construction, a general SMT engine, or complete
fixed-point range analysis across a CFG. See the upstream
MetaIL documentation (`MetaToolchain/MetaIL/README.md` in a source workspace)
for its precise scope.

[Previous](17-canonical-ir.md) · [Index](README.md) · [Next](19-analysis.md)
