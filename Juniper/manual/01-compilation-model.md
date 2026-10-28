# 1. The compilation model and its boundaries

Juniper is a macro-based intermediate language, not a general-purpose application
runtime. Use it when you want a readable, generated compiler input with a clear
path from scalar expressions to selected and scheduled virtual instructions.

## Follow one program through the stages

<!-- check: juniper -->
```juniper
@module(model)
@fn(scale, (x))
  @return(add(mul(x, 8), 3))
@end
```

The stages have different jobs:

```text
trusted macro source
    | GPPX: @macro expansion, #include, conditionals
expanded Juniper source
    | metadata reader + the actual MetaIL parser/validator
owned scalar IR + backend profile
    | optional real MetaIL optimization/analysis
selected virtual instructions
    | dependency/latency/resource-aware scheduling
machine-IR JSON for inspection or a C++ MachineModule
```

The arithmetic program means `x * 8 + 3` under signed-64 wraparound semantics.
There is no implicit promise that it becomes an x86 instruction, a RISC-V
instruction, or an executable function. The portable profile initially names
operations such as `MUL` and `ADD`; these are selected opcodes, not assembler text.

Run these commands after saving the program as `$WORK/model.jun`:

```sh
"$J" preprocess "$WORK/model.jun" -o "$WORK/model.expanded"
"$J" emit "$WORK/model.jun" -o "$WORK/model.junir"
"$J" metail "$WORK/model.jun" -o "$WORK/model.mi"
"$J" select "$WORK/model.jun" -o "$WORK/model.selected.json"
"$J" schedule "$WORK/model.jun" -o "$WORK/model.scheduled.json"
```

`preprocess` performs no IR validation. `emit` validates and writes a complete
canonical program, including its backend rules. `metail` deliberately omits the
backend, so use it for interchange with MetaIL rather than lossless Juniper
storage. `select` chooses tree covers; `schedule` also computes dependencies and
issue cycles. Neither command runs the function.

## A Juniperfile is also IR

Macro-free input is already useful:

<!-- check: juniper no-preprocess -->
```juniper
juniper 1;
module model;
fn scale(x) {
  return add(mul(x, 8), 3);
}
```

Omitting a backend chooses the portable profile. A later `emit` makes that profile
explicit. This is why you can generate IR once, inspect it, edit it, and continue
the pipeline without replaying the macro configuration.

## Know what remains your responsibility

Juniper currently has scalar signed-64 values and mutable locals. It does not
construct SSA, infer types, resolve calls, allocate registers, lower an ABI,
perform memory/alias analysis, write object files, or link an executable. The
MetaArch adapter encodes explicitly mapped instructions only. These boundaries
are useful: unsupported work fails or remains explicit rather than being guessed.

**Practice:** produce all five artifacts above. Find the multiplication in the
MetaIL text and the selected `MUL` instruction in JSON. Observe that the JSON
result name is a virtual register, not a physical register assignment.

[Manual index](README.md) · [Next: building](02-building.md)
