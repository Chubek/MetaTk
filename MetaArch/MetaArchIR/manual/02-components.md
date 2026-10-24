# Chapter 2 — Components and data flow

## The implementation layers

[`MetaArchIR.hpp`](../MetaArchIR.hpp) supplies inline C++20 types and functions in
namespace `mair`. Its parser uses DSLtk's parser combinators. A parsed program is
the same kind of owned `Program` that the fluent C++ builder produces.

Database loading and instruction encoding use the public MetaArch C++ API in
[`MetaArch.hpp`](../../MetaArch.hpp). That API owns the validated architecture
specification and delegates encoding to the underlying C library. The generated
`.march` files in `MetaArchDB` are parsed as architecture documents; mnemonic
discovery is not a textual search through those files.

The CLI consists of [`marchir.c`](../cli/marchir.c), its C entry point, and
[`marchir.cpp`](../cli/marchir.cpp), the C++ implementation that contains
exceptions and selects output modes.

## Two output routes

```text
.mair source --parse--+
                     +--> Program + Target --> render --> GAS-style text
C++ builder ---------+                   |
                                         +--> assemble --> Image
                                                             |
                                                             +--> flat bytes
                                                             +--> CLI JSON
```

`render` accepts native instructions and encoded instructions. Encoded instructions
become exact `.byte` sequences if they contain no relocations. Native operands
remain assembler text.

`assemble` accepts encoder/form instructions and layout/data directives, but
rejects native instruction text. It returns sections, symbols and relocations.
`Image::flat` can then produce bytes only when no section layout or relocation
information would be lost under its single-section contract.

## Where failures occur

Parsing catches malformed statement syntax; building also checks local invariants
such as duplicate labels. Target-dependent checks happen during rendering or
assembly. Loading a target can itself fail if its specification is invalid or
unavailable. A successful parse therefore does not establish that a program can
be encoded for a particular target.

[Previous](01-portable-assembly.md) · [Contents](README.md) · [Next](03-building.md)
