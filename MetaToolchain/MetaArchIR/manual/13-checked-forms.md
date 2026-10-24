# Chapter 13 — Checked instruction forms

## Select the form, then map its operands

A form associates a mnemonic and encoder with operand descriptions and feature
requirements. MetaArchIR addresses forms by their zero-based index in the loaded
specification. In the shipped riscv64 database, form 11 is the three-operand
`addi` form:

<!-- verify: target=riscv64 emit=hex -->
```mair
form 11, rd=x10, rs1=x10, imm=-1
```

The bindings have two simultaneous meanings. Their **positions** correspond to
the form's declared operands; their **names** identify the encoder parameters.
Here those positions are destination register, source register and immediate.
Reordering named bindings is therefore not generally safe for a form statement.

## Discovering forms in C++

```cpp
auto target = mair::target("riscv64", "MetaArchDB");
auto forms = target.forms("addi");
if (forms.size() != 1) throw mair::Error("select the intended addi form explicitly");
mair::Program p;
p.form(forms.front(), {{"rd", mair::reg("x10")},
                       {"rs1", mair::reg("x10")}, {"imm", mair::imm(-1)}});
auto image = mair::assemble(p, target);
```

This example requires an unambiguous match instead of treating the first of many
forms as a universal default. For overloaded mnemonics, inspect their operand
records through `target.spec()` and choose deliberately.

## What is checked

The underlying `Spec::encode_form` validates the selected form, operand count,
required/forbidden target features, supported operand kinds, physical register
class/width and immediate constraints before encoding. For this `addi`, `2048`
is outside the signed 12-bit range and an FP register is not a GPR.

Current form limitations are explicit: symbolic inputs require `encode`; hidden
operands, custom constraints, missing operand metadata and unsupported operand
kinds can be rejected by the underlying API. A database form's existence is not
a promise that every form kind is supported by this interface. Form indices are
revision-dependent and should be rediscovered after database updates.

[Previous](12-encoders.md) · [Contents](README.md) · [Next](14-registers.md)
