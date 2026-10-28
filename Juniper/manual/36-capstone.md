# 36. Capstone: a configurable affine backend

This project combines macros, conditional profile selection, canonical IR,
explicit MetaIL passes, CMake dependencies, machine selection/scheduling, and
artifact validation. Everything is supplied under
[examples/capstone](examples/capstone/README.md).

## The project layout

```text
capstone/
  CMakeLists.txt
  Juniperfile
  macros/arithmetic.xpp
  profiles/base.xpp
  profiles/fused.xpp
  README.md
```

The entry file defaults to fusion but can be configured at expansion time:

```text
#include "macros/arithmetic.xpp"
#ifndef USE_FUSION
#define @USE_FUSION 1
#endif
#if @USE_FUSION
#include "profiles/fused.xpp"
#else
#include "profiles/base.xpp"
#endif
@module(affine_capstone)
@fn(affine, (x, factor, bias))
  @return(@affine_expression(x, factor, add(bias, 0)))
@end
```

The macro header defines `@affine_expression` as `add(mul(#1, #2), #3)`. Both
profiles inherit portable coverage and use issue width 2 / `alu` capacity 2.
The fused profile additionally declares:

```text
rule capstone_madd : add(mul(reg,reg),reg) => MADD
  cost 3 latency 4 resource alu;
```

`MADD` remains a profile semantic assertion; this project does not provide a
native hardware implementation.

## Run both variants explicitly

```sh
"$J" emit "$MANUAL_EXAMPLES/capstone/Juniperfile" -DUSE_FUSION=1 \
  -o "$WORK/fused.input.junir" --depfile "$WORK/fused.input.d"
"$J" optimize "$WORK/fused.input.junir" --no-preprocess \
  --passes ConstantFolding,AddZeroRight --rounds 2 -o "$WORK/fused.optimized.junir"
"$J" schedule "$WORK/fused.optimized.junir" --no-preprocess \
  -o "$WORK/fused.machine.json"
python3 "$MANUAL_EXAMPLES/inspect_schedule.py" "$WORK/fused.machine.json"

"$J" emit "$MANUAL_EXAMPLES/capstone/Juniperfile" -DUSE_FUSION=0 \
  -o "$WORK/base.input.junir"
"$J" optimize "$WORK/base.input.junir" --no-preprocess \
  --passes ConstantFolding,AddZeroRight -o "$WORK/base.optimized.junir"
"$J" schedule "$WORK/base.optimized.junir" --no-preprocess \
  -o "$WORK/base.machine.json"
```

The optimizer removes `add(bias, 0)`, exposing the fused shape. Expect `MADD` in
the fused variant, and separate `MUL`/`ADD` in the base variant. Both variants
still have structural `RET`; the result is not executable code.

## Build the same stages with CMake

First install a source-dependency build, then configure the supplied consumer:

```sh
cmake --install "$JUNIPER_ROOT/build" --prefix "$WORK/install"
cmake -S "$MANUAL_EXAMPLES/capstone" -B "$WORK/capstone-build" \
  -DCMAKE_PREFIX_PATH="$WORK/install" -DCAPSTONE_USE_FUSION=ON
cmake --build "$WORK/capstone-build" -j4
python3 "$MANUAL_EXAMPLES/inspect_schedule.py" \
  "$WORK/capstone-build/artifacts/scheduled.json"
```

`juniper_compile` generates the first canonical artifact and its include depfile.
Explicit custom commands then run the chosen optimization passes and schedule
with `--no-preprocess`, each depending on the prior artifact. The project sets
MetaIL runtime discovery through the normal installation/environment contract;
use an explicit `METAIL_HOME` if your runtime is deployed elsewhere.

Edit `macros/arithmetic.xpp` or the active profile and rebuild: the depfile should
cause regeneration. Reconfigure with `CAPSTONE_USE_FUSION=OFF` to change the
compile-time definition. Never edit files in the installed/source example tree
for an experiment without first copying that project to your own working area.

## Extend responsibly

Add analysis artifacts from the optimized canonical input, new selection rules
with boundary tests, or different launch capacities. Keep each change separately
observable. Before adding a MetaArch emission stage, define physical assignments,
form/parameter maps, ABI/control handling, and custom-opcode semantics. Reject
unsupported pieces rather than silently skipping them.

You now have a complete, repeatable **IR compilation** project. Its explicit
stages and boundaries are the foundation for a larger compiler, not evidence
that the unimplemented parts have already been supplied.

[Previous](35-testing.md) · [Manual index](README.md)
