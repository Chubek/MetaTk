# Affine backend capstone

See [chapter 36](../../36-capstone.md) for the complete walkthrough. This project
is a **consumer** of an installed Juniper package, not a second Juniper build.

```sh
cmake -S "$MANUAL_EXAMPLES/capstone" -B "$WORK/capstone-build" \
  -DCMAKE_PREFIX_PATH="$WORK/install" -DCAPSTONE_USE_FUSION=ON
cmake --build "$WORK/capstone-build" -j4
python3 "$MANUAL_EXAMPLES/inspect_schedule.py" \
  "$WORK/capstone-build/artifacts/scheduled.json" --expect-opcode MADD
```

Configure another build with `-DCAPSTONE_USE_FUSION=OFF` to see separate multiply
and add operations. The three generated artifacts are expanded canonical source,
optimized canonical source, and scheduled machine-IR JSON. Canonical intermediate
stages are read with `--no-preprocess`.

`MADD` is a virtual selected opcode with wrapping signed-64 semantics asserted
by the profile author. This example supplies no physical register allocation,
native implementation, ABI, or callable machine-code image.

Copy this directory into your own project before making experiments. Its includes
are root-relative, so preserve the `macros/` and `profiles/` directory structure.
