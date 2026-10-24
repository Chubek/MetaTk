# Chapter 1 — The portable-assembly model

## What a program represents

A MetaArchIR program is an ordered list of assembly statements. It can describe
instructions, labels, visibility declarations, sections, alignment and data.
An instruction uses physical registers or immediate values chosen by its author.
No compiler pass chooses a register, schedules an instruction or rewrites an
algorithm on the author's behalf.

Portability comes from a common assembly API, common directives and a target
database. Selecting a target changes which instruction encoders and operand
constraints are available. It does not make every instruction name meaningful
on every architecture.

## Three instruction interfaces

| Interface | Meaning | Validation |
| --- | --- | --- |
| Native instruction | Target assembler mnemonic and operand text | Text structure and mnemonic support |
| `encode` | An exact database encoder with named inputs | Encoder/VM checks |
| `form` | An exact database form with explicit input mapping | Form constraints followed by encoding |

Choose `form` when the database describes the required operand constraints.
Choose `encode` when you deliberately want the lower-level encoder interface.
Choose native statements when an external assembler should handle target syntax,
including addressing expressions and branch labels.

## A small portability example

<!-- verify: target=amd64 emit=hex -->
```mair
encode ret
```

The shipped amd64, AArch64 and RISC-V databases all have a parameterless `ret`
encoder. This source can therefore be reused with those three targets. Its bytes
differ: `c3`, `c0035fd6`, and `67800000`, respectively. The reuse is possible
because the database interfaces agree, not because MetaArchIR translates an
abstract return operation.

## Keep the boundaries explicit

Calling conventions, instruction availability and application semantics remain
part of target-specific assembly design. The library does not supply an optimizer,
register allocator, linker or object-file writer. Its section image preserves
information needed by downstream tools instead of silently inventing those tools'
decisions.

[Contents](README.md) · [Next: Components](02-components.md)
