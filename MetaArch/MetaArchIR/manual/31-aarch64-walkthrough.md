# Chapter 31 — AArch64: addition and return

## State the register convention

The following routine treats `x0` and `x1` as input registers and returns their
64-bit sum in `x0`, consistent with a simple AArch64 calling-convention use. Its
register assignments are explicit; MetaArchIR does not derive them from a C type
signature.

<!-- verify: target=aarch64 emit=hex -->
```mair
section .text
global add2
add2:
encode add, rd=x0, rn=x0, rm=x1
encode ret
```

Run the repository example:

```sh
build/marchir -T aarch64 --db MetaArchDB --emit hex \
  MetaArchIR/examples/cli/aarch64/add.mair -o -
```

Expected bytes are `0000018bc0035fd6`: `add x0, x0, x1` followed by the database's
return sequence. Changing the parameter names to RISC-V-style `rs1` and `rs2`
would not retarget the instruction; it would request inputs this encoder does
not define.

## Low-level versus checked use

This example deliberately exercises the named encoder interface. Register names
are resolved to their encodings, but the statement is not automatically associated
with a checked form. For stronger operand constraints, inspect `target.forms("add")`
and select the intended form with its declared operand-to-parameter mapping.

For a native text workflow, the equivalent instruction is `add x0, x0, x1` and
must be rendered for an external AArch64 assembler. Native immediates such as
`#42` are preserved by the parser rather than mistaken for trailing comments.

## Verify independently

```sh
printf 'add x0, x0, x1\nret\n' | llvm-mc -triple=aarch64 -show-encoding
```

Comparing this output with the byte image checks the target encoding independently
of MetaArch's reader and VM. Keep ABI assumptions, selected target record and
database revision alongside any golden byte sequence used by an application.

[Previous](30-riscv-walkthrough.md) · [Contents](README.md) · [Next](32-amd64-compute.md)
