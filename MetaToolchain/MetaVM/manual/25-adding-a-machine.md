# Chapter 25 — Adding or extending a machine

[Previous](24-generation-workflows.md) · [Contents](README.md) · [Next](26-chip8-machine.md)

## Start with a compatibility decision

Write down the intended format/version, word widths, address units, execution
model, and authoritative references before implementing a decoder. A language,
engine, or runtime name may not identify any stable public ISA. Examples include
version-sensitive Python/Ruby bytecode, runtime names such as ART/GraalVM, and
unspecified identities such as ALEX or FUL.

If these questions are unresolved, add or preserve a catalog identity with
honest limitations. Do not invent bytes or map it to a convenient unrelated VM.
A catalog-only profile is a useful tracked requirement, not a failed native
implementation that should silently return success.

## Choose the execution path

- **Machinery program authoring:** use Builder or the C module builder.
- **Fixed-width, statically targeted guest subset:** consider Definition IR.
- **Variable-length formats, language runtimes, closures, graphics, or rich
  host integration:** design a separate native API with explicit contracts.

Definition version 1 cannot express every runtime or dynamic decoder. Do not
shoehorn indirect branches, self-modifying code, full object models, or an
unbounded scheduler through assumptions the lowerer cannot verify.

## Integrate the catalog deliberately

The generator discovers machine directories and maintains the generated
registry/companions. Extend its architecture/coverage data and native-header
mapping as appropriate, then regenerate and review the manifest, profile,
metadata headers, and coverage output. Add a new coverage enum to the schema
when necessary. Keep `native_format_loader` false unless a real specified
format loader is supplied; an in-memory instruction vector is not a historical
binary loader.

Place native implementation files under separate names outside the generator's
owned `<machine>.c/.cpp` adapters. Preserve existing symbols and the historical
`Smaltalk-80` spelling. Add legacy aliases only through an explicit sourced
mapping, not a similarity heuristic. Update legacy documentation if relevant.

## Required semantic and failure tests

For each supported instruction/transition, retain exact input and expected
effects. Include:

1. Ordinary execution and both outcomes of conditional control flow.
2. Width overflow, division extremes, shifts, signed fields, and endian edges.
3. Unknown and truncated instructions, including unreachable malformed code.
4. Branch bounds/alignment, stack underflow/overflow, and bad types.
5. Independent code, stack, frame, heap/environment, and fuel ceilings.
6. Zero/exact/exceeded boundaries using that API's documented zero convention.
7. Ownership after input destruction and failure cleanup.
8. Deterministic independent oracles or differential checks where available.

Do not derive all expected outputs from the implementation under test. Keep
external fixtures pinned, offline, licensed, and accompanied by their complete
required source. Never weaken an existing test solely to admit a new profile.

## Delivery checklist

Compile public headers independently; run optimized and sanitizer builds; retain
core/factory tests; exercise an installed relocated consumer. Document input
format, API ownership, error behavior, limits, concurrency, host requirements,
and unsupported features. Update coverage only after tests establish execution,
not merely because a `.cpp` file exists. Optional integrations need real
dependency/device tests before they are described as validated.

**References:** [project contract](../AGENTS.md), [refinement requirements](../docs/REFINEMENT.md), [validation](39-validation-and-troubleshooting.md).
