# Chapter 22 — Real MetaML authoring

[Previous](21-definition-lowering.md) · [Contents](README.md) · [Next](23-catalog-and-legacy-adapters.md)

## Use the actual evaluator

`vmweave/MetaMLAdapter.hpp` registers primitives on the supplied
`metaml::Session` and evaluates real MetaML source. It does not implement an
alternate parser that merely resembles the language. A fresh Session and owned
authoring state are used per translation. MetaML and matching DSLtk require
C++20 and the optional configuration from chapter 5.

```sh
out-metaml/vmweave-metaml vmweave/factorial.vmweave factorial.mvmb
out-metaml/Machinery/metavm-run factorial.mvmb
```

Expected result: `3628800`. The ordinary `vmweave` CLI cannot read this MetaML
source; use it for Machinery assembly/binary only.

## Module-authoring primitives

| Primitive | Arguments |
|---|---|
| `vm_emit` | Assembly text |
| `vm_function` | Name, arity, register count |
| `vm_const` | Destination register, i64 value |
| `vm_binary` | Operation name, destination, left, right |
| `vm_label` / `vm_jump` | Label name |
| `vm_jump_false` | Register, label name |
| `vm_return` | Register |
| `vm_end` | Unit `()` |

```text
val _ := vm_function "main" 0 1
val _ := vm_const 0 42
val _ := vm_return 0
val _ := vm_end ()
```

The adapter publishes qualified `VMWeave.vm_*` / `VMWeave.vd_*` primitives and
language-level short aliases compatible with bundled authoring files. MetaML
functions can emit whole loops or instruction rules; compilation is evaluation,
not line-by-line string matching.

## Definition-authoring primitives

`compile_definition` exposes the full fixed-width Definition IR:

| Primitive | Arguments / returned value |
|---|---|
| `vd_machine` | Name, word bits, `"big"`/`"little"`, registers, register bits, memory bytes |
| `vd_rule` | Rule name, unsigned mask/value represented as bounded i64 |
| `vd_const` | i64 → node reference |
| `vd_field` | Shift, width, sign boolean → reference |
| `vd_read_register`, `vd_read_byte` | Index reference → value reference |
| `vd_binary` | Operation string, left/right references → reference |
| `vd_write_register`, `vd_write_byte` | Index and value references |
| `vd_jump` | Target reference |
| `vd_branch` | Condition and target references |
| `vd_halt` | Result reference |
| `vd_end` | Unit `()`; close rule |

Definition binary operation strings are `add/sub/mul/div/mod/and/or/xor/shl/shr/eq/lt`.
References are opaque node indices, not actual runtime register values. The
adapter checks open-rule state and reference order, bounds each rule to 4096
nodes, and bounds authored nodes cumulatively to 65,536. General Definition
state/rule checks also apply.

```text
vmweave-metaml --definition INPUT.vmweave GUEST.bin OUTPUT.mvmb
```

The output remains a specialized two-array-argument module. Its CLI compilation
does not make it directly runnable by the zero-argument generic runner.

## C++ entry points, diagnostics, and trust

`vmweave::compile` returns a `metavm::Module`; `compile_definition` returns a
Definition. Overloads accept source-byte limits, source filename, and
`EvaluationLimits`, defaulting to 1,000,000 checkpoints and depth 256.
Source defaults to 16 MiB and rejects NUL. `MetaMLError` retains the complete
language diagnostics vector; rendered messages include source context.

Limits do not sandbox arbitrary MetaML imports or primitive host effects.
The supplied dependency's earlier retained type-checker allocations remain a
separate known sanitizer concern; successful non-MetaML leak checks do not
resolve it. Preserve dependency bytes and record exact compatibility versions.

**References:** [adapter](../vmweave/MetaMLAdapter.hpp), [complete Definition source](../vmweave/examples/ToyR16.vmweave).
