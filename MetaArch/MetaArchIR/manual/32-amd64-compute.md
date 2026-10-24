# Chapter 32 — amd64: sum of squares

## Function contract

[`Compute.cpp`](../examples/api/Compute.cpp) builds a native assembly routine
equivalent to an unsigned 32-bit sum of squares:

```text
compute(x, y) = x*x + y*y, modulo 2^32
```

It assumes the x86-64 System V register convention used by the Linux execution
test: arguments in `%edi` and `%esi`, return value in `%eax`. No stack frame is
needed because the routine uses only the selected argument/result registers and
does not call another function.

## Assembly listing

<!-- verify: target=amd64 emit=asm -->
```mair
section .text
global compute
compute:
mov %edi, %eax
imul %edi, %eax
imul %esi, %esi
add %esi, %eax
ret
```

The two-operand `imul` instructions retain the low 32 bits of each product.
That matches unsigned arithmetic modulo 2^32 for this function. The final add
also retains the low 32 bits. This is not a claim of unbounded mathematical
integer arithmetic.

## Build and inspect

```sh
METAARCHDB="$PWD/MetaArchDB" build/mair_example_Compute > compute.s
cc -c compute.s -o compute.o
```

Run these commands on a compatible x86-64 toolchain. The first executable only
prints assembler text; it does not call the emitted function. The second command
assembles that text into a host object.

The CLI integration test links the generated function with a C driver and checks
that `compute(3, 4)` returns 25 on a supported native host. This validates more
than string emission: the external assembler and C ABI agree with the example.

Because this is a native-text program, asking MetaArchIR for direct binary output
would fail. That is a deliberate interface distinction; a direct-byte version
would need explicitly selected database encoders or checked forms.

[Previous](31-aarch64-walkthrough.md) · [Contents](README.md) · [Next](33-amd64-gcd.md)
