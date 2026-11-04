# Chapter 20 — Definition IR and instruction semantics

[Previous](19-vmweave-builder.md) · [Contents](README.md) · [Next](21-definition-lowering.md)

## What a Definition describes

`vmweave::Definition` specifies a fixed-width guest instruction encoding and
its scalar semantics. Version 1 supports 8-, 16-, or 32-bit words, either byte
order, 1–4096 registers of 1–64 bits, and up to 16,777,216 data bytes. Code and
data are separate. It does not infer an ISA from a catalog name.

The public `state` declares `instruction_bits`, `register_bits`, `registers`,
`memory_bytes`, and `Endian::{big,little}`. Defaults are 16-bit big-endian code,
16 64-bit registers, and 4096 memory bytes. A Definition has a name and a vector
of `Rule`s, each with a name, word `mask/value`, and ordered semantic nodes.

## Decoding and unambiguous rules

A word matches when `(word & rule.mask) == rule.value`. All mask bits must fit
the instruction width, and value may not set bits outside the mask. Rules
must be disjoint; Definition verification rejects any potentially overlapping
pair rather than depending on an arbitrary first-match priority.

Names must be nonempty and at most 255 bytes. There must be 1–4096 rules, each
with 1–4096 nodes. Rule names must be unique. Invalid enum values, fields,
references, and operations after a control transfer are errors.

## Node operations

| Rule helper | Result/effect |
|---|---|
| `constant(i64)` | Scalar node reference |
| `field(shift, width, sign=false)` | Extract/sign-extend a guest-word field |
| `read_register(index_ref)` | Read guest register at scalar index |
| `read_byte(index_ref)` | Read data byte |
| `binary(op, left, right)` | Scalar arithmetic/bit/comparison node |
| `write_register(index_ref, value_ref)` | Mask write to declared register width |
| `write_byte(index_ref, value_ref)` | Mask write to eight bits |
| `jump(target_ref)` | Transfer to an absolute guest byte address |
| `branch(condition_ref, target_ref)` | Transfer when scalar is nonzero |
| `halt(value_ref)` | Return a scalar result |

Value references are zero-based node indices local to a rule and must refer
to earlier value-producing nodes. A write/control node is not a value. Binary
operations include add/sub/mul/div/mod, bitwise AND/OR/XOR, shifts, equality,
and signed less-than. Arithmetic uses Machinery's defined i64 semantics;
comparisons normalize to scalar 0/1.

## A complete minimal definition

```cpp
vmweave::Definition definition("return-byte-v1");
definition.state.instruction_bits = 8;
definition.state.memory_bytes = 0;
vmweave::Definition::Rule rule{"return-byte", 0, 0, {}};
auto value = rule.field(0, 8);
rule.halt(value);
definition.rules.push_back(rule);
definition.verify();
auto result = definition.execute({42});
if (result.value != 42) throw std::runtime_error("Definition result mismatch");
```

The mask accepts every byte because this deliberately specified toy format
defines every word as a return instruction. That is not a valid way to model
unknown historical opcodes. Real definitions should encode only sourced
instructions, leaving unknown words rejected by lowering.

Verification validates an ISA description; it does not yet validate a particular
program's branch addresses, selected words, or dynamic register/memory accesses.
Chapter 21 covers those distinct checks and the specialized generated module.

**References:** [Definition.hpp](../vmweave/Definition.hpp), [ToyR16 specification](../vmweave/examples/ToyR16.hpp).
