# 27. Optimization pipelines

Optimization accepts a pass filter and a repeat count. The filter is `"all"`
(also NULL in C) or an explicit comma-separated list of registered transformation
names. Explicit lists run in the requested order. Analysis kernels are rejected
in transformation lists, and conversely.

```cpp
auto result = context.optimize(module,
    "ConstantFolding,AddZeroRight,MultiplyByPowerOfTwoRight03", 2);
```

The default pipeline selects every registered transformation in loader order.
After each round it performs final constant folding, branch simplification,
dead-assignment cleanup, and unreachable-code removal. An explicit pass list does
not silently add those cleanup passes.

Rounds are bounded to 1–12; the C++ and CLI defaults are two. Processing stops early
when a round reports no changes. This is bounded iterative optimization, not a
promise of a global canonical normal form or equality saturation.

Statistics use the JSON shape `{"kernel_changes": {name: count, ...}}`. Counts
aggregate selected-pass changes across rounds. Automatic final cleanup is not
included in those per-kernel statistics, and different procedural passes may
measure a change at different granularities.

To debug a surprising result, run a single explicit pass, compare the structured
input/output, and inspect its statistics. Then add passes in order. Preserve
observable calls and traps when deciding whether a visually simpler expression
is actually equivalent.

[Contents](README.md) · [Next: Rewrites](28-rewrites-and-guards.md)
