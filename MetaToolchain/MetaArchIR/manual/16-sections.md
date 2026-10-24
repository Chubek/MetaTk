# Chapter 16 — Sections and section-relative layout

## Select or create a section

`section .text` switches the current section. Repeating the same section name
resumes its existing byte buffer rather than creating a second section with that
name. Sections appear in the image in first-selection order.

<!-- verify: target=amd64 emit=json -->
```mair
section .data
byte 1
section .text
encode ret
section .data
byte 2
```

This produces `.data` bytes `0102` and `.text` bytes `c3`. The intervening text
section does not affect the data section's next offset.

## The implicit text section

If an emitting or label statement appears before any explicit section selection,
`assemble` creates `.text`. A program containing only comments and no explicit
section can produce an image with no sections. Explicitly selecting a section
creates it even if it remains empty.

`global` and comment statements alone do not select a section. A label before
the first section does, because it needs a section-relative position.

## Layout is not object-format policy

The `Section` API record contains a name, required alignment and bytes. It does
not contain ELF flags, a virtual address, a load address, a symbol table index
in an object file or an executable permission policy. Arbitrary syntactically
valid dot-prefixed section names are accepted; they are not restricted to the
section names in the target specification.

For the same reason, naming a section `.bss` does not create implicit zero-fill
storage without bytes. Only the supported statements determine its contents.
Use an external object writer or application integration layer to assign richer
section semantics.

Multiple sections are preserved by JSON/image output. `Image::flat` rejects an
image with more than one section, even if some are empty, rather than guessing
their order, addresses and inter-section padding.

[Previous](15-symbols.md) · [Contents](README.md) · [Next](17-alignment.md)
