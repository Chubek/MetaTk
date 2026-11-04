# Chapter 9 — Modules, functions, and instructions

[Previous](08-values-and-arithmetic.md) · [Contents](README.md) · [Next](10-assembly-language.md)

## Modules are owned instruction containers

A module owns copied function/import names and instruction arrays. Functions
declare an arity and local register count. Function/import indices begin at
zero and occupy separate namespaces. Forward references may exist during
construction; verification resolves whether the completed module is usable.

Core construction API:

| Function | Purpose |
|---|---|
| `mvm_module_create` / `mvm_module_destroy` | Allocate/release a module |
| `mvm_module_add_function` | Add name, arity, register count; obtain index |
| `mvm_module_add_import` | Add name and arity; obtain import index |
| `mvm_module_emit` | Append a copied instruction to a function |
| `mvm_module_verify` | Check the completed structure |
| `mvm_module_clone` | Return an independently owned verified copy |
| `mvm_module_find` | Look up a function by name |

Builder failures do not partially mutate the module. Treat a published module
as immutable while any thread or VM reads it. Destruction is null-safe.

## Instruction representation

`mvm_instruction` holds `op`, unsigned 32-bit `a/b/c`, and signed-64 `immediate`.
The meaning depends on the opcode:

| Family | Fields |
|---|---|
| `CONST`, `BOOL` | Destination `a`, literal `immediate` |
| `NIL`, `RET` | Register `a` |
| `MOV`, `NOT`, `ARRAY`, `LEN` | Destination `a`, source `b` |
| Arithmetic/comparison/`GET` | Destination `a`, operands `b/c` |
| `SET` | Array `a`, index `b`, stored value `c` |
| `JMP` | Absolute instruction-index `immediate` |
| `JZ` | Test register `a`, absolute target `immediate` |
| `CALL`, `HOST` | Result `a`, target index `b`, first argument register `c` |

The target's arity determines a call's contiguous argument window. For a
zero-argument call, the first-argument field may equal the register count.
Unused instruction fields are ignored; do not infer meaning from them.
`mvm_opcode_name(op)` exposes the instruction's mnemonic for diagnostics and
external tooling; use the enum/wire version, not mnemonic discovery, to define
a persistent opcode encoding.

## Register and call behavior

Each call creates a fresh register window. Arguments populate the first `arity`
registers; the remainder start nil. Registers do not survive between independent
invocations. `CALL` copies arguments to the callee window and eventually stores
the return value in the caller's destination; overlap between argument and
destination registers is supported by the execution machinery.

A function returns a single tagged value. There is no implicit value stack,
variadic calling convention, exception instruction, closure instruction, or
native object ABI. Build such language features explicitly above this substrate.

## Inspection without dangling views

`mvm_module_function_count`, `mvm_module_import_count`, `mvm_module_function`,
and `mvm_module_import` inspect contents. `mvm_function_info` borrows its name
and code pointers. All views can be invalidated by module mutation or destruction.
Copy needed information before appending instructions, replacing code, or
optimizing. Do not retain borrowed views as independent ownership.

**References:** [Module.h](../Machinery/Module.h), [verification](11-verification-and-optimization.md).
