# Chapter 34 — Retargeting and application integration

## Reuse only interfaces that agree

```cpp
mair::Program p;
p.section(".text").global("return_to_caller")
 .label("return_to_caller").encode("ret");
for (const auto *name : {"amd64", "aarch64", "riscv64"}) {
    auto target = mair::target(name, "MetaArchDB");
    auto image = mair::assemble(p, target);
    auto bytes = image.flat();
    (void)bytes; // Hand each target's bytes to your chosen consumer.
}
```

This works because all three selected databases provide compatible parameterless
`ret` encoders. A general addition routine requires different register choices
and parameter names, and may require different instruction sequences. Retargeting
is explicit assembly adaptation, not a guarantee that an unchanged Program works
on every target.

## A practical application boundary

Keep target-specific assembly recipes in your application alongside their ABI
requirements. Load each Target once, inspect its forms/registers, then construct
Programs for that target. Record the database revision and selected target index
with generated artifacts.

If multiple forms match a mnemonic, use a deliberate selection policy based on
the records and the application's operand requirements. Do not reuse a form
index from one architecture in another or infer mappings from encoder parameter
enumeration.

## Embedding and custom specifications

Applications can load a `metaarch::Spec` through its public API and move it into
`mair::Target`. This supports controlled test specifications or alternate database
sources without scanning `.march` declarations manually. Inspect the MetaArch
`Result` before moving out its successful value; a failed load is not a usable
target.

Choose an output boundary suited to the application: assembler text for an
external assembler, flat bytes for a single-section nonrelocatable payload, or an
Image/JSON handoff when section and relocation information matters. Object writing
and relocation application remain explicit downstream responsibilities. This
keeps the portable-assembly API useful without turning it into a high-level
compiler IR or hiding linker decisions.

[Previous](33-amd64-gcd.md) · [Contents](README.md) · [Next](35-verification.md)
