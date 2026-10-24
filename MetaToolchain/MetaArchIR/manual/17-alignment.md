# Chapter 17 — Alignment and padding

## Alignment is measured in bytes

`align N` and `balign N` mean an alignment of **N bytes**. N must be a positive
power of two no larger than 65536. It is not a logarithm: `align 4` means four-byte
alignment, not sixteen-byte alignment.

<!-- verify: target=amd64 emit=hex -->
```mair
section .data
byte 1
align 4
word 0x1234
```

On amd64 the bytes are `010000003412`: the first byte is followed by three zero
padding bytes, then the little-endian word. The current section's alignment
metadata becomes at least four.

## Layout rule

For section size `s` and requested alignment `n`, padding is:

```text
(n - (s % n)) % n
```

An already aligned offset receives zero padding. Alignment at the start of an
empty section raises its alignment requirement without adding bytes. Returning
to a section uses that section's own size, independent of other sections.

If several alignments are requested, the image retains their maximum. Because
the accepted alignments are powers of two, this is sufficient to represent their
combined section-placement requirement for a downstream consumer.

## Text versus image behavior

`render` writes `.balign N`, leaving the external assembler to apply its own
padding behavior. `assemble` inserts zero bytes itself. These routes need not
produce identical padding bytes in executable sections if an external assembler
chooses instruction NOPs.

Zero-filled padding is not guaranteed to be safe to execute on the target ISA.
Place alignment where control flow cannot accidentally fall through it, or use
an explicitly designed native assembly sequence. The image API records layout;
it does not select target-specific NOP instructions or perform branch relaxation.

[Previous](16-sections.md) · [Contents](README.md) · [Next](18-data-and-endianness.md)
