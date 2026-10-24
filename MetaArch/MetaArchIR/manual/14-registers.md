# Chapter 14 — Physical registers and operand constraints

## Names refer to database registers

Checked instructions use physical register names declared in the target
specification. In RISC-V examples, `x10` identifies a real architectural register;
it is not a compiler-generated temporary. MetaArchIR does not allocate a new
register when it encounters an unfamiliar name.

```cpp
auto target = mair::target("riscv64", "MetaArchDB");
auto info = target.register_info("x10");
auto encoding = info.enc;
auto width = info.width;
auto operand = mair::reg("x10");
```

`reg` checks name syntax and constructs an operand. Target lookup happens during
encoding, or immediately when you explicitly call `register_info`. Thus creating
`mair::reg("not_a_register")` does not by itself prove that a register exists.

## Native and checked spellings differ

Native amd64 AT&T operands commonly use `%eax`. Checked bindings use database
names, such as `eax`, without the assembler's `%` marker. Likewise, a literal
`$42` belongs to native AT&T text; a checked immediate is `mair::imm(42)` or the
text `42`. Do not pass assembler decorations to the typed operand interface.

## Register class and width

A checked form can distinguish a general-purpose register from an FP or vector
register and compare the declared width. This prevents using a numerically
similar encoding from the wrong class. Low-level encoders primarily consume
numeric encodings and do not imply these form checks.

Passing `mair::imm(10)` is not equivalent to `mair::reg("x10")` for a form that
expects a register name, even if the integer matches its encoding.

Calling-convention choices remain yours: argument registers, callee-saved
registers, stack discipline and result registers are not inferred by the builder.
The examples use explicitly stated ABIs and register assignments so their intent
can be reviewed independently of the encoding machinery.

[Previous](13-checked-forms.md) · [Contents](README.md) · [Next](15-symbols.md)
