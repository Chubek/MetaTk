# Chapter 21 — Definition lowering and execution

[Previous](20-definition-ir.md) · [Contents](README.md) · [Next](22-metaml-authoring.md)

## Program-specialized translation

`definition.lower(code, max_source)` verifies the Definition, decodes every
fixed-width word in a supplied byte vector, and returns a verified Machinery
module. It specializes that module to this code vector. It does not emit a
reusable decoder for arbitrary future programs, infer self-modifying behavior,
or silently skip words it cannot decode.

Version-1 code must be nonempty, at most 65,536 bytes, and aligned to complete
8-/16-/32-bit words. Unknown words fail even if unreachable. Branch destinations
must resolve statically from constants/fields and supported constant expressions
to aligned absolute byte addresses inside the code. Runtime-computed indirect
targets fail. A final fallthrough or conditional false path past the end fails.

## State and result conventions

The lowered `main` takes two Machinery arrays: guest registers and data bytes.
The result is a scalar halt value. It is not a generic zero-argument module
suitable for `metavm-run`. A C host calling a generated Definition factory must
create, root, and initialize the two arrays before invoking it.

`definition.execute(code, initial_registers, initial_memory, limits, engine)`
provides that host setup. Supplied state must fit declared capacities; missing
cells become integer zero. Initial registers and subsequent register writes
are masked to `register_bits`; byte writes mask to eight bits. The host roots
both arrays across allocation and execution. The returned `Result` owns copied
`registers`, `memory`, scalar `value`, and `steps`.

`steps` counts lowered Machinery instructions, including semantic glue, not
guest words. Use it to enforce execution limits, not to claim hardware cycle
accuracy or compare guest instruction counts across different lowerings.

## ToyR16 worked example

The supplied ToyR16-v1 has 16-bit big-endian words, sixteen byte registers, and
256 data bytes:

| Encoding | Semantics |
|---|---|
| `1xkk` | Load immediate byte `kk` into register `x` |
| `2xy0` | Add register `y` into `x`, wrapping byte result |
| `3xkk` | Store register `x` to byte address `kk` |
| `4xkk` | Branch to byte offset `kk` when register `x` is nonzero |
| `500x` | Halt with register `x` |
| `6xy0` | Subtract register `y` from `x` |

```cpp
auto definition = vmweave::examples::toy_r16();
auto result = definition.execute({0x10,40, 0x11,2, 0x20,0x10,
                                  0x30,3, 0x50,0});
if (result.value != 42 || result.registers[0] != 42 || result.memory[3] != 42)
    throw std::runtime_error("ToyR16 result mismatch");
```

The example ISA is deliberately specified, not assigned to an unresolved
historical catalog identity.

## Emission and independent budgets

`vmweave-definition-demo OUTPUT.c` emits the specialized module's C factory
`toy_r16_factory`. [The factory regression](../tests/definition_factory.c)
demonstrates the exact C host convention under both engines. The existing
16 MiB lowering source budget, Machinery module-byte limit, heap/register/frame
limits, and instruction fuel apply independently. No limit is automatically
raised because the Definition is otherwise valid.

Signed fields are explicitly extended; comparisons become integer 0/1;
branches convert integer-nonzero semantics to Machinery booleans. Preserve
those boundaries when extending the IR: Machinery integer zero is itself truthy.

**References:** [ToyR16.hpp](../vmweave/examples/ToyR16.hpp), [lowering implementation](../vmweave/Definition.hpp).
