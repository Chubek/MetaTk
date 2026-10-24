# Chapter 33 — amd64: Euclid's GCD algorithm

## Algorithm and register assignment

[`GCD.cpp`](../examples/api/GCD.cpp) emits the subtraction form of Euclid's
algorithm for two unsigned 32-bit inputs. It keeps the first working value in
`%eax` and the second in `%esi`, following the same System V argument convention
as the preceding example.

The algorithm repeatedly subtracts the smaller nonzero value from the larger
until they are equal. It handles zero inputs separately, including `(0, 0)`.

<!-- verify: target=amd64 emit=asm -->
```mair
section .text
global gcd
gcd:
mov %edi, %eax
test %esi, %esi
jz .done
test %eax, %eax
jz .take_b
.loop:
cmp %esi, %eax
je .done
jb .reduce_b
sub %esi, %eax
jmp .loop
.reduce_b:
sub %eax, %esi
jmp .loop
.take_b:
mov %esi, %eax
.done:
ret
```

## Read the branches as assembly

`cmp %esi, %eax` compares the working values in AT&T operand order. `jb` selects
the unsigned-below case, so the next subtraction reduces the larger value.
Using a signed conditional branch would change behavior for inputs with their
high bit set.

These labels and branches are native assembler text. MetaArchIR validates their
names and mnemonic support, while the external assembler computes the branch
displacements. No control-flow analysis or instruction rewriting is performed.

## Build and verify

```sh
METAARCHDB="$PWD/MetaArchDB" build/mair_example_GCD > gcd.s
cc -c gcd.s -o gcd.o
```

The native integration test checks `(48,18)`, `(0,7)`, `(7,0)` and `(0,0)`.
Subtraction-based GCD is intentionally simple, but can take many iterations for
widely differing positive values. An author wanting a division-based routine
must write that alternative assembly algorithm; MetaArchIR does not optimize
this loop into one.

[Previous](32-amd64-compute.md) · [Contents](README.md) · [Next](34-retargeting.md)
