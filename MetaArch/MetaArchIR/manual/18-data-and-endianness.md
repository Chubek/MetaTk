# Chapter 18 — Data directives and byte order

## Fixed-width directives

| Source directive | Size | Accepted numeric interval |
| --- | --- | --- |
| `byte` | 1 byte | −128 through 255 |
| `word` | 2 bytes | −32768 through 65535 |
| `long` | 4 bytes | −2147483648 through 4294967295 |
| `quad` | 8 bytes | Full signed int64 range |

The smaller directives accept either the signed or unsigned range for their
width. All literal parsing still goes through int64, so `quad` does not accept
every uint64 value. To emit eight all-one bytes, use `quad -1`.

<!-- verify: target=amd64 emit=hex -->
```mair
word 0x1234
long -1
byte 0, 255
```

On a little-endian target this yields `3412ffffffff00ff`. On a big-endian target
the first two bytes become `1234`; single-byte data is unaffected.

## Numeric normalization

The builder validates and normalizes numeric data arguments to decimal strings.
This prevents an external assembler from treating a leading zero differently
from the language's decimal rule. Text rendering uses `.2byte`, `.4byte` and
`.8byte` for multi-byte data instead of relying on an assembler-specific meaning
of `.word`. Those dotted fixed-width spellings are accepted by the parser too.

## Symbolic data and field endianness

A bare identifier may appear as a data value for text rendering, such as
`quad destination`. An external assembler determines its symbolic meaning.
The image assembler requires integer data values and does not manufacture a
relocation for this directive. For a database-described relocation, use an
encoder's symbolic input instead.

Data directive byte order follows the selected target record. An instruction
encoder can explicitly emit a field with a different byte order; its relocation
records preserve that actual field order. Do not apply a blanket target-endian
rewrite to already encoded instruction bytes.

[Previous](17-alignment.md) · [Contents](README.md) · [Next](19-program-api.md)
