# mulib-assembler

A typed, single-instruction AMD64 assembler subset. **Opcode bytes, register
encodings, ModRM direction/groups, widths and REX policies come from the
supplied `dsymspec/amd64.dsymspec.yaml` metadata**, resolved by
`tools/dsymspec_generate.py`. The C code implements encoding mechanisms, not
an unrelated hardcoded mnemonic/opcode map.

## Integration

Define all these switches explicitly before MuLIB includes in one C11 TU:

```
MULIB_IMPL_ASSEMBLER MULIB_IMPL_DSYMSPEC MULIB_IMPL_EMITTER MULIB_IMPL_LINKER
MULIB_IMPL_JSON MULIB_IMPL_PARSEC MULIB_IMPL_STRBUF MULIB_IMPL_UTF8
```

The optional native execution example additionally defines `MULIB_IMPL_JIT`
and `MULIB_IMPL_DYNCALL`. No switch is silently enabled. Declarations support
C++17. Assembly is host-independent and requires no executable memory or OS
API; the separate JIT has its own host restrictions.

## Supported input, precisely

`mulib_assembler_emit(spec, image, linker, mnemonic, operands, count, &error)`
encodes one exact case-sensitive mnemonic with zero, one or two typed operands.
There is **no textual assembly grammar**. Construct borrowed operands using
`mulib_assembler_reg(name)`, `mulib_assembler_imm(value)`, and
`mulib_assembler_symbol(name, byte_addend)`. Register and symbol names are
case-sensitive. For two operands, destination precedes source (Intel order).

Only generated AMD64 `asmtk` metadata is executable; other architectures are
queryable through dsymspec but produce unsupported diagnostics here. The target
must declare a 64-bit address size and little-endian byte order (`little` or `le`);
contradictory big-endian AMD64 metadata is rejected without emission. The
currently generated table enables:

| Forms | Supported operands |
| --- | --- |
| Fixed | `nop`, `ret`, `syscall`, `int3`, `ud2`, `cqo`, `leave`; no operands |
| RegisterRegister | `mov`, `add`, `sub`, `and`, `or`, `xor`, `cmp`, `test`, `imul`; same-width GPRs, 32/64-bit |
| MoveImmediate | `mov` into 32/64-bit GPR, immediate of 4/8 encoded bytes |
| OpcodeRegister | `push`, `pop`; 64-bit GPR |
| Unary | `inc`, `dec`, `neg`, `not`, `div`, `idiv`; 32/64-bit GPR |
| Indirect | `call`, `jmp`; 64-bit GPR |
| ShiftCL | `shl`, `shr`, `sar`; 32/64-bit destination and literal register `cl` |
| ShiftImmediate | `shl`, `shr`, `sar`; 32/64-bit destination and 0..255 immediate |
| ArithmeticImmediate | `add`, `or`, `and`, `sub`, `xor`, `cmp`; 32/64-bit GPR and 32-bit encoded immediate |
| Relative | `call`, `jmp`; named symbol plus byte addend |

This table describes the current snapshot, not an API promise to execute any
future metadata form. Normal and REX-extended GPR encodings 0..15 are supported.
Eight-/16-bit forms, high-byte registers, APX encodings, memory/SIB/displacement
operands, segment/address-size prefixes, condition branches/aliases, vector and
privileged extension forms, labels/directives as text, and pseudo-code beyond
the generator's templates are unsupported. Although `Load`/`Store` and condition
records exist in the source, the public operand API deliberately does not
pretend to implement them.

A 32-bit move/arithmetic immediate accepts `INT32_MIN..UINT32_MAX` as a field
bit pattern. A 64-bit move accepts all `int64_t` values; unsigned bit patterns
above `INT64_MAX` are not directly expressible in this signed operand API.
64-bit arithmetic immediates must fit signed 32 bits because the CPU sign
extends them. Shifts always use the imm8 form, with normal hardware masking of
the shift count; no size optimization/relaxation is attempted.

## Relocations and composition

The image must be an initialized little-endian emitter with caller-owned
storage. Relative branches require a linker; other instructions may pass NULL.
Define labels separately with `mulib_linker_define(linker, name, image.size, 0)`.
The assembler reads the named source relocation's width, signedness, pcrel flag
and expression, then records a checked PC32 field with addend `operand - 4`.

In this **flat-image direct-binding model**, both `S + A - P` and the source
`PLT(S) + A - P` form are supported with the caller-provided symbol address as
the final destination (`PLT(S)=S`). No PLT entry, ELF relocation number, dynamic
symbol interposition, GOT or object file is generated. External calls need a
caller-resolved absolute address within signed rel32 reach; there is no veneer.
The linker validates the final displacement when applying relocations.

Emission first builds a bounded temporary instruction, checks image capacity,
and atomically records any relocation before appending. Every failure preserves
image bytes/size and linker tables. Metadata is borrowed for the call; register
names are borrowed; the linker copies symbol names. There is no assembler
allocation, session state, or automatic label scope. Do not mutate the borrowed
metadata DOM, emitter or linker concurrently.

## Diagnostics and trust

Zero means success. Results distinguish `EINVAL`, `EUNSUPPORTED`, `EREGISTER`,
`ERANGE`, `ENOSPACE`, and `ELINK`. The optional error contains a static message;
`linker_result` preserves the original linker result for `ELINK`. Success clears
both. Unknown mnemonics, unsupported architectures/operand widths/forms,
invalid registers, immediate truncation and addend overflow never silently
emit a placeholder. All instruction scratch storage is 16 bytes and checked
metadata constrains the largest implemented encoding below that bound.

Metadata is trusted code-generation input, not a machine-code safety proof.
Source `forms` and backend descriptions contain additional constraints beyond
this audited `asmtk` subset. No CPU feature discovery, ABI register allocation,
instruction scheduling, stack discipline checking, trap prevention, semantic
pseudo-code execution or general architecture support is claimed. For example,
emitting `syscall`, `ud2`, or an unbalanced `push` does not make it safe to run.

## Runnable end-to-end example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/assembler.c -lm -o /tmp/opencode/mulib-assembler
/tmp/opencode/mulib-assembler
```

The example assembles a forward jump to `mov rax,42; ret`, resolves it, checks
the expected bytes, and returns 42 through the W^X JIT on supported hosts.
`tests/test_codegen.c` checks individual encodings, both relocation directions,
unsupported cases, atomic failure, real source/schema metadata and native
execution. It also changes a cloned resolved opcode and verifies the output
changes, demonstrating the encoder actually consults its metadata.
