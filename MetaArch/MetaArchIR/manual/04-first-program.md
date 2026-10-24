# Chapter 4 — Your first program

## Write a minimal function

Save the following as `return.mair`:

<!-- verify: target=amd64 emit=hex -->
```mair
section .text
global return_to_caller
return_to_caller:
encode ret
```

`section` selects the output section. `global` marks a symbol for visibility;
the following label defines that symbol at the current position. `encode ret`
invokes the selected database's parameterless return encoder.

## Request different representations

```sh
build/marchir -T amd64 --db MetaArchDB --emit hex return.mair -o -
build/marchir -T amd64 --db MetaArchDB --emit asm return.mair -o return.s
build/marchir -T amd64 --db MetaArchDB --emit json return.mair -o return.json
```

Hex output is `c3` followed by a newline. Assembly output contains a `.byte 195`
directive beneath the section and symbol declarations. The JSON image retains
the `.text` section and `return_to_caller` symbol, whose offset is zero.

Changing the first command to `-T aarch64` yields `c0035fd6`; `-T riscv64` yields
`67800000`. Each target must actually declare the encoder you request.

## Validate without writing

```sh
build/marchir -T amd64 --db MetaArchDB --emit bin --check return.mair
```

This runs the binary-output path, including its restrictions, but writes no
output file. It is more specific than merely checking source syntax.

The repository already contains this exercise as
[`06_retarget.mair`](../examples/cli/06_retarget.mair). A raw `.bin` result is just
machine-code bytes, not an executable file with an entry point or an object-file
symbol table. Use JSON when you need to retain the program's section and symbol
structure for another component.

[Previous](03-building.md) · [Contents](README.md) · [Next](05-database-loading.md)
