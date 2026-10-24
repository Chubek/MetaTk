# Chapter 15 — Labels and global symbols

## Define a section-relative position

A label records the current offset in the current section. It emits no bytes.
`global` marks a name for global visibility and also emits no bytes:

<!-- verify: target=amd64 emit=json -->
```mair
global entry
section .text
entry:
encode ret
```

The resulting image has a symbol named `entry`, section index zero, offset zero,
and `global=true`. A global declaration may precede or follow the corresponding
label; image assembly applies the visibility markings after layout.

## Duplicate and undefined names

Label names must be unique throughout a Program, including across sections.
Using `.loop` twice does not create two local scopes. A duplicate label is rejected
by the builder or parser before target-dependent emission.

Repeated global declarations are harmless to image assembly's set of global
names. A global name without a defining label, however, makes `assemble` fail.
`render` simply emits assembler declarations and does not enforce that same
defined-global requirement. This difference matters when comparing text-only and
image-output validation.

## Definitions are not references

Writing `callee:` defines a symbol. Writing `sym=@callee` supplies a symbolic
encoder input that must correspond to an emitted relocation. Even when both
appear in one program, MetaArchIR retains the relocation instead of computing
the final linked value. Symbol offsets have no absolute base address yet.

Native branch operands such as `jmp .loop` follow a different path: `render`
passes that operand text to an external assembler, which can resolve it according
to the target dialect. The checked image path does not translate such native
branch syntax into encoder parameters.

[Previous](14-registers.md) · [Contents](README.md) · [Next](16-sections.md)
