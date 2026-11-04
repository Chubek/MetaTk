# Chapter 1 — Purpose, scope, and compatibility

[Contents](README.md) · [Next](02-architecture-and-layout.md)

## What MetaVM provides

MetaVM combines a small execution substrate, authoring tools, a machine catalog,
and selected guest interpreters. Use it to embed a bounded register machine,
generate verified modules from another compiler, experiment with explicit
instruction definitions, or execute one of the documented native subsets.

Its execution substrate is **Machinery**, an ISO C11 register VM. VMWeave builds
Machinery modules through a C assembly bootstrap, a typed C++ Builder, Definition
lowering, and an optional real MetaML embedding. VMDesc extends an optional
MetaTeX engine with machine-documentation commands. VMLibrary contains catalog
metadata, portable examples, compatibility adapters, and native C++ interpreters.

These layers can be used separately. An application embedding only Machinery
does not need MetaML, MetaTeX, libdrm, the catalog, or a C++ compiler. An
application using `Chip8` does not need to translate its ROM through VMWeave.

## Four different meanings of support

| Term | What it establishes |
|---|---|
| Catalog identity | A named profile with metadata and declared limitations |
| Portable example | A Machinery program demonstrating build/execution plumbing |
| Native executable subset | A separate API implementing specified guest semantics |
| Full platform compatibility | A substantially stronger claim, not made here |

The catalog has 49 entries. Eight have native executable subsets: CHIP-8,
SSAVM, DRMVM, JVM, FORTH, SECD, Krivine, and DVI. DVI provides a standard
version-2 binary decoder. The other native APIs use raw instructions, source,
or explicitly defined in-memory formats rather than complete platform loaders.
The remaining 41 entries are catalog-only.

For example, running the JVM catalog `.mvm` and obtaining `42` does not load a
Java class. `jvm_integer_method` separately interprets a bounded integer Code
array; it still does not supply classes, objects, a verifier for the full JVM,
exceptions, or a Java standard library.

## Who should read this manual

Embedding developers should concentrate on ownership, rooting, callbacks, and
resource budgets before exposing guest execution. Compiler authors should read
the assembly, verification, and Definition chapters before emitting artifacts.
Native-profile users should read both the API chapter and its limitations; a
familiar machine name is not a substitute for choosing a supported format.

No component should silently replace an unknown opcode with a generic example,
NOP, zero, or success. Unsupported input must fail visibly. This is a central
compatibility and safety rule throughout the project.

## Version discipline

Framework package version 0.2.0, Machinery API version 0.1.0, the Machinery
binary's wire version 1, and closure-profile version 1 describe different
contracts. Do not compare them as one version stream. Rebuild C++ consumers
when headers or object layouts change; there is no stable C++ ABI promise.

**References:** [coverage](../docs/COVERAGE.md), [manifest](../MANIFEST.yaml),
[compatibility roadmap](40-compatibility-and-roadmap.md).
