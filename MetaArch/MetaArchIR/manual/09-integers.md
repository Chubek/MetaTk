# Chapter 9 — Integers and numeric boundaries

## Literal syntax

Checked integer literals are signed 64-bit values. They allow an optional `+`
or `-`, followed by digits in one of these forms:

| Form | Base | Example |
| --- | --- | --- |
| Unprefixed | Decimal | `42`, `-17`, `+3` |
| `0x` | Hexadecimal | `0x2a`, `-0x10` |
| `0b` | Binary | `0b101010` |
| `0o` | Octal | `0o52` |

The prefixes are lowercase. Hexadecimal digits may use either letter case.
Leading zero by itself does not mean octal: `010` is decimal ten. The parser
requires the entire literal to be valid, so `4junk` is not the number four.

<!-- verify: target=amd64 emit=hex -->
```mair
byte 42, 0x2a, 0b101010, 0o52
quad -9223372036854775808
```

The first four bytes are all `2a`. The full literal range is
−9223372036854775808 through 9223372036854775807; an unsigned-looking hexadecimal
literal does not expand that range.

## Literal range versus operand range

A successfully parsed number may still be outside an instruction operand's
range. RISC-V `addi` forms use a signed 12-bit immediate: −2048 through 2047.
`2048` is a valid language integer but not a valid immediate for that form.
The `form` path checks those bounds; low-level `encode` only applies checks
actually implemented by its encoder/VM.

Data directives have their own width checks, described in chapter 18. Alignment
and form indices also impose narrower constraints after parsing. Expressions
such as `1+2` are not checked integer literals. Symbol references have a separate,
limited addend syntax; they are not general arithmetic expressions either.

[Previous](08-names-and-comments.md) · [Contents](README.md) · [Next](10-byte-strings.md)
