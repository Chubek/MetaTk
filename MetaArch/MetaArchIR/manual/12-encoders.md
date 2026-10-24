# Chapter 12 — Named encoder statements

## Invoke an exact database program

`encode` names a database encoder and supplies its input parameters by name:

<!-- verify: target=riscv64 emit=hex -->
```mair
encode addi, rd=x10, rs1=x10, imm=-1
encode ret
```

For the shipped RISC-V database this yields `1305f5ff67800000`. The first
instruction decrements `x10`; the second uses the database's fixed return encoding.

Use `--list-encoders` or `Target::parameters` to discover parameter names.
A mnemonic and an encoder can share a name without being interchangeable API
concepts. An architecture may use a different encoder name for a particular
operand pattern.

## Binding values

Each binding is `parameter=value`. Values may be integers, physical register
names or explicit `@symbol` references with addends. Register values are resolved
through the selected specification to their numeric encoding. Duplicate parameter
names fail during program construction. Missing, extra or unknown inputs are
handled by the underlying encoding API.

```cpp
mair::Program p;
p.encode("addi", {{"rd", mair::reg("x10")},
                  {"rs1", mair::reg("x10")}, {"imm", mair::imm(-1)}});
```

Encoder binding order is not a substitute for parameter names. For this low-level
interface the names establish the mapping, while a `form` statement additionally
uses declared operand order.

## Know which checks you selected

The encoder executes the database's compiled instruction-emission program and its
VM checks. It does **not** automatically inherit all form operand constraints.
An encoder that wraps an immediate can accept a value that a checked form would
reject. Use the form interface when you want register-class, width, feature and
immediate-bound validation.

Relocation-bearing encoders produce image relocation records. They cannot be
flattened or rendered as plain byte directives without losing those records.

[Previous](11-native-assembly.md) · [Contents](README.md) · [Next](13-checked-forms.md)
