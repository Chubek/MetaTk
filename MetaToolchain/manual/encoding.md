# Encoder language and checked encoding

## Statements

An encoder body contains one statement per line; `#` starts a comment.

```text
EMIT8 expression
EMIT16LE expression       EMIT16BE expression
EMIT32LE expression       EMIT32BE expression
EMIT64LE expression       EMIT64BE expression
EMIT_IMM n, expression
EMIT_MODRM reg, rm
EMIT_RELOCATION symbol, RELOC_NAME
REQUIRE condition
```

Each item above is a separate statement (the paired endian examples are not
two statements on one source line). `EMIT_IMM` uses 1, 2, 4 or 8 bytes in target
byte order. Fixed emitters override the target. `EMIT_MODRM` emits the x86
register-direct byte `0xC0 | ((reg & 7) << 3) | (rm & 7)`. Relocations attach to
the most recently emitted field. `REQUIRE` must evaluate to nonzero.

Expressions use decimal/`0x`/`0b`/`0o` literals, operand identifiers,
parentheses, and the following operators (high to low precedence):

```text
unary - ~ !
* / %
+ -
<< >>
< <= > >=
== !=
&
^
|
&&
||
```

Arithmetic uses wrapping 64-bit two's complement. Division is signed; modulo
is Euclidean (nonnegative for a positive divisor). Right shift is arithmetic.
Operands are integer values, not parsed register names. Constant expressions
are folded, then compiled into stack-based RPN. Parameters are discovered in
first-use order. Do not rely on C-style short-circuit evaluation of RPN boolean
expressions to guard invalid arithmetic. Built-in functions such as `BITS`,
`SEXT`, `ZEXT`, `FIELD`, `ALIGNED` are **not yet implemented**.

For a signed n-bit immediate, `(imm + 2**n) % 2**n` expresses wrapping (write the
constant in actual source; `**` is not a supported operator). Adding `2**(n-1)`
instead creates an incorrect bias for an ordinary two's-complement field.
ISA-specific scaled/split/biased fields still require independent verification.

## Low-level versus checked forms

`dsym_encode`, `dsym_encode_named`, opaque ABI `metaarch_spec_encode`, and Python
`spec.encode` run REQUIRE and field-fit checks; they **do not check form bounds**.
ABI/Python named encoding requires all parameters and forbids extra/duplicate
bindings. The native named API separately offers `allow_unbound`.

`dsym_encode_form`, opaque ABI `metaarch_spec_encode_form`, and Python
`spec.encode_form` enforce form constraints. Walk operands in declaration order,
skip `hidden: true` and `kind: implicit`, and map the rest positionally onto
compiled parameters in first-use order. Count mismatches fail, including complex
memory operands that cannot map one-to-one. The form flag `implicit_ops` alone
does not hide an operand. Error operand positions are original 1-based positions.

- Register values are hardware `enc` values, not register-table indices. At
  least one declared register must match encoding, class and width.
- Scalars obey explicit `imm_min`/`imm_max` and width/signed limits. With width
  present, absent `signed` means unsigned; without width, explicit bounds apply.
- The int64 API cannot represent unsigned values above `INT64_MAX`.
- `sym`/`rel` operands must align with an `EMIT_RELOCATION` parameter; other
  operand kinds must not occupy that parameter.
- Native checked forms do not parse free-form constraints or evaluate feature
  gates. The C++20 assembler has its own explicit mapping/feature checks; do not
  confuse its API with the C++17 ABI wrapper.

Current per-instruction output caps are 64 bytes and 8 relocations. The expression
VM stack is capped at 64 entries. General configurable core resource limits are
not available.

## Relocation layout

The field's `offset` and `size` are bytes within output. `bit_offset` is relative
to the least significant bit of the emitted integer; `bit_width` is the
relocation record width. `endian` (ABI/Python: `byte_order`) is resolved to the
actual field byte order. A 26-bit relocation inside a 32-bit field is valid;
a 32-bit relocation inside a byte is not. Runtime encoding rejects intervals
that extend past the field.

A consumer reads the field integer in its recorded order, inserts the relocated
value into `[bit_offset, bit_offset + bit_width)`, and writes it back. MetaArch
does not infer split-bit relocations, resolve symbols or run a linker.

## Named lint diagnostics

| Code | Condition | Current policy |
|---|---|---|
| `DSYM-W001` | Folded RPN `PARAM c1 ADD c2 MOD`, c2 a positive power of two, c1 neither 0 nor c2 | Lint warning; strict error even without lint |
| `DSYM-W002` | Visible operand/parameter count mismatch | Lint warning; strict + lint error |
| `DSYM-W003` | Symbol/relocation kind disagrees with relocation parameter | Lint warning; strict + lint error |
| `DSYM-W004` | Relocation bit interval exceeds emitted field | Lint warning; strict + lint error; runtime always rejects |

An exact trailing `# biased` comment opts out of W001 for an intentional ISA
bias. It does not suppress other errors. Not every historical diagnostic has a
stable code; individual code suppression is not implemented. Compiler lint
mapping checks are skipped with NO_COMPILE. Known data exceptions and unresolved
sites are in [the encoder audit](../docs/encoder-audit.md).
