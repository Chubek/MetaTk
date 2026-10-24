# Chapter 30 — RISC-V: a checked decrement function

## Establish the target interface

This example targets the shipped riscv64 specification. It takes a value in
`x10` (the conventional `a0` argument/result register), subtracts one with `addi`
and returns using the database's `ret` encoder.

First inspect the form and register listings:

```sh
build/marchir -T riscv64 --db MetaArchDB --list-forms
build/marchir -T riscv64 --db MetaArchDB --list-registers
```

The supplied database has `addi` at form index 11. A different revision can move
that index; the C++ API can discover it with `target.forms("addi")`.

## Source and expected bytes

<!-- verify: target=riscv64 emit=hex -->
```mair
section .text
global decrement
decrement:
form 11, rd=x10, rs1=x10, imm=-1
encode ret
```

```sh
build/marchir -T riscv64 --db MetaArchDB --emit hex \
  MetaArchIR/examples/cli/riscv64/decrement.mair -o -
```

Expected output is `1305f5ff67800000`. The first four bytes encode word
`0xFFF50513`; the last four encode `0x00008067`. The byte strings reflect the
instructions' little-endian emission.

## Why the form path matters

The form's third operand is a signed 12-bit immediate, so −2048 and 2047 are its
boundary values. Replacing −1 with 2048 should fail form validation. Replacing
the destination with `f0` should fail the register-class check. Neither case is
merely a malformed token: these are target-dependent semantic errors.

## Independent cross-check

An external LLVM assembler can check the native equivalents:

```sh
printf 'addi a0, a0, -1\nret\n' | llvm-mc -triple=riscv64 -show-encoding
```

The test suite also compares object-section bytes using `llvm-objcopy` when both
tools are installed. This cross-check is valuable because the database encoder,
not MetaArchIR's surface syntax, defines the actual instruction bit pattern.

[Previous](29-resource-limits.md) · [Contents](README.md) · [Next](31-aarch64-walkthrough.md)
