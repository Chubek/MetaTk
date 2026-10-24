# Chapter 24 — Assembling section images and flat bytes

## Build an owned image

```cpp
auto target = mair::target("amd64", "MetaArchDB");
mair::Program p;
p.section(".text").global("entry").label("entry").encode("ret");
mair::Image image = mair::assemble(p, target);
auto bytes = image.flat();
```

`assemble` walks statements in order and tracks the current section. It emits
database-encoded instructions, literal byte strings, numeric data and zero-filled
alignment padding. Native instructions are an error on this route.

## Image records

- `Section`: `name`, `alignment`, and owned `bytes`.
- `Symbol`: `name`, zero-based `section`, section-relative byte `offset`, `global`.
- `Relocation`: section/field positions, name, parameter, symbol/addend and field
  metadata described in chapter 25.

Section indices refer to the image's section vector. They are not addresses or
section identifiers from a future object file. Symbols are collected in label
encounter order; global marking is applied after layout.

## Flattening restrictions

`Image::flat` returns a copy of the sole section's bytes when there is one section
and no relocations. An empty image returns an empty byte vector. It rejects any
relocation and any image with more than one section, including an extra empty
section.

Flattening discards symbol names and alignment metadata even in the accepted
single-section case. Its result has no entry point, load address or executable
container. A caller that needs those properties must retain the Image and supply
an appropriate placement/object-writing layer.

## Output budgets and mutation

The assembly byte budget covers the sum of emitted bytes across all sections,
including padding. It is not a complete cap on all object and string allocations.
Image records are public value containers; if an application modifies them after
assembly, it assumes responsibility for keeping indices and offsets consistent.
`flat` is a narrow extraction check, not a general verifier for arbitrary mutated
images.

[Previous](23-rendering.md) · [Contents](README.md) · [Next](25-relocations.md)
