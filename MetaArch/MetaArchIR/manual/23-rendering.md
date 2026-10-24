# Chapter 23 — Rendering assembler text

## The rendering contract

`mair::render(program, target, limits)` returns GAS-style assembly text. It begins
with a target comment, emits statements in order and uses a classic numeric
locale. The target is validated even for an otherwise empty program.

<!-- verify: target=amd64 emit=asm -->
```mair
section .text
global entry
entry:
encode ret
```

For amd64, the emitted instruction line is `.byte 195`. This is intentionally
exact encoded data rather than a reconstructed native `ret` spelling.

## Statement rendering

| Input kind | Output |
| --- | --- |
| Native instruction | Indented mnemonic and comma-separated operands |
| Encoder/form | Decimal `.byte` values from the database encoder |
| Label | `name:` |
| Global | `.globl name` |
| Section | `.section name` |
| Alignment | `.balign N` |
| Byte string | `.ascii` with escaped bytes |
| Multi-byte data | `.2byte`, `.4byte`, `.8byte` |
| Explicit comment | A `#` comment line |

Parser-discarded comments cannot reappear; only explicitly built comment
statements are rendered. Source indentation and numeric spellings are not a
format-preservation contract.

## What rendering does not establish

Native operand validity belongs to the external assembler. Rendering also does
not resolve labels or require every global declaration to have a local definition.
Use the emitted file with an assembler for the selected architecture and dialect.

An encoder/form with relocations is rejected by `render`, even if its symbol is
locally defined. Printing just its bytes would omit necessary relocation work.
Use image/JSON output for that path, or native assembler syntax when you intend
an external assembler to own symbol resolution.

`Limits::output_bytes` bounds the rendered text, including the initial comment
and all escaping. It is not merely a limit on the eventual instruction bytes.

[Previous](22-ownership.md) · [Contents](README.md) · [Next](24-images.md)
