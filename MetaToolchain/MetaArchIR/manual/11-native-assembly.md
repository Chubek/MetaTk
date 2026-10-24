# Chapter 11 — Native assembly statements

## Let an external assembler interpret operands

A statement whose leading name is not a directive becomes a native instruction.
Its operands remain target-assembler text. This amd64 example uses AT&T syntax:

<!-- verify: target=amd64 emit=asm -->
```mair
section .text
global answer
answer:
mov $42, %eax
ret
```

`render` checks that the target declares the mnemonic or an accepted alias, then
prints the operands in their original order. It does not reverse operand order,
translate register spellings or infer an assembler dialect.

The C++ equivalents are `p.ins("mov", "$42", "%eax")` and
`p.emit("mov", {"$42", "%eax"})`. Both create native instruction statements;
neither invokes a database encoder.

## Operand boundaries

Balanced `()`, `[]` and `{}` protect internal commas. For example,
`8(%rax,%rcx,4), %rdx` is a two-operand list, not four operands. Quotes also protect
commas during splitting. Empty operands, mismatched delimiters and instruction
separators are rejected before rendering.

## Validation boundaries

Mnemonic acceptance does not establish register class, immediate range, enabled
features or machine-code encodability. The external assembler still performs
those tasks. Native x86 size suffixes `b/w/l/q` are accepted against declared base
mnemonics as a spelling convenience; this is not validation of every suffixed
instruction combination. Conditional branch spellings come from condition metadata.

`assemble` rejects native instructions, including a native `ret` with no operands.
Write `encode ret` to obtain bytes directly. This distinction avoids guessing
which encoder or form implements an arbitrary assembler spelling. For native
addressing expressions and local branch labels, render a `.s` file and use the
appropriate external assembler explicitly.

[Previous](10-byte-strings.md) · [Contents](README.md) · [Next](12-encoders.md)
