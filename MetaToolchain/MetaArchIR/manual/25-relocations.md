# Chapter 25 — Relocations and linker handoff

## Bind a symbolic encoder input

<!-- verify: target=amd64 emit=json -->
```mair
section .text
encode call, sym=@external_function-4
encode ret
```

For the shipped amd64 database, the call emits placeholder bytes plus an
`R_X86_64_PLT32` relocation. The symbol name is `external_function` and the bound
value/addend is −4. This addend is specific to the chosen encoding/relocation
convention; do not copy it indiscriminately to another ISA.

The C++ spelling is `mair::symbol("external_function", -4)`. A symbolic binding
must correspond to an emitted relocation parameter. Supplying `@name` to an
ordinary arithmetic input does not ask MetaArchIR to infer a relocation type.

## Field reference

| Field | Meaning |
| --- | --- |
| `section` | Index into `Image::sections` |
| `offset` | Byte offset of the emitted field within that section |
| `size` | Size of the emitted field in bytes |
| `name` | Database relocation name |
| `parameter` | Encoder input that supplied the symbol value |
| `symbol` | Referenced name, or empty for a numeric relocation input |
| `value` | Bound numeric value; the addend for a `SymbolRef` |
| `bit_offset` | LSB-relative bit position within the emitted integer |
| `bit_width` | Relocation bit width |
| `endian` | Actual field byte order |
| `pc_relative` | PC-relative flag from the relocation description |

A 26-bit relocation can occupy part of a four-byte field. Use the bit metadata
instead of assuming `bit_width == size * 8`. The actual field byte order can
differ from the target's default byte order.

## Handoff responsibilities

MetaArchIR preserves relocation records, including references to locally defined
labels. It does not evaluate database relocation expressions, assign final
addresses, apply overflow policy or write object files. A linker integration must
consult the corresponding target relocation semantics, not derive a universal
formula from the PC-relative flag alone.

Consequently, `render` rejects relocation-bearing encodings and `flat` rejects
images containing relocations. JSON/image output is the supported loss-preserving
handoff for this assembly route.

[Previous](24-images.md) · [Contents](README.md) · [Next](26-json-images.md)
