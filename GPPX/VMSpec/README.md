# VM reference catalogue

These `.vmwv` files are **GPPX source documents that expand to JSON**. There
was no pre-existing `.vmwv` grammar or consumer in this repository. Format
`gppx-vmwv`, version 1, is a local descriptive catalogue format, not an external
VM generator language. It does not generate interpreters or binary bytecode.

Every entry specifies a chosen machine/revision, named state components,
invariants, selected symbolic operations, and a worked example. The scope is
explicitly `reference-subset`: these are not complete ISA definitions, runtime
implementations, or conformance suites. Some abstract-machine operations are
normalized semantic rules rather than historical instruction mnemonics. Each
entry identifies such choices in `notes` and includes references.

## Build and use

The top-level build enables `GPPX_BUILD_VMSPECS` by default. It builds target
`gppx_vmspec` and writes one JSON file per source to `build/VMSpec/json/`.

```sh
cmake -S . -B build
cmake --build build --target gppx_vmspec
python3 VMSpec/validate.py --self-test build/VMSpec/json/*.json
build/gppx 'VMSpec/CPython Lua.vmwv' -o comparison.json
```

The subdirectory can also build independently using an installed `gppx`:

```sh
cmake -S VMSpec -B build-vmspec -DGPPX_VMSPEC_PROCESSOR=/path/to/gppx
cmake --build build-vmspec
ctest --test-dir build-vmspec --output-on-failure
```

Installation places source files, the validator and generated JSON under
`${CMAKE_INSTALL_DATADIR}/gppx/vmspec`. Python is optional for building; when
available, CTest runs the validator and its rejection tests.

## Format contract

Each source is processed independently with a fresh, default GPPX state.
`VMWV_VERSION` is a temporary preprocessing macro and is undefined afterwards.
JSON strings remain literal under the default syntax configuration.

The expanded root object has exactly these fields:

| Field | Meaning |
| --- | --- |
| `format`, `version` | `gppx-vmwv` and integer `1` |
| `id` | Lowercase filename stem with spaces replaced by hyphens |
| `name`, `revision` | Display name and selected historical/version scope |
| `scope` | `reference-subset` |
| `model` | `stack`, `register`, `transition`, `closure`, `graph`, `logic`, `tape`, or `comparison` |
| `state` | Nonempty mapping of state component names to descriptions |
| `invariants` | Nonempty array of semantic constraints |
| `operations` | Objects with unique `name`, ordered `operands` names and textual `effect` |
| `example` | Textual `initial` state, ordered `steps`, and textual `expected` outcome |
| `references` | Primary documentation URLs or bibliographic references |
| `notes` | Encoding choices, omissions and interpretation assumptions |

Each example step contains an `op` and an `args` array of scalar symbolic
operands. References to registers, labels and terms are strings; they are not
resolved or evaluated by the validator. Validation checks JSON keys, schema,
unique IDs and operation names, and example operand counts. It **does not**
prove the stated effects or execute examples. No opcode numbers are invented.

## Filename interpretations

- `P4`: Pascal-P4, not the packet-processing P4 language.
- `CMCM`: Thompson and Lins' Categorical Multi-Combinator Machine. Its entry
  presents categorical semantic operations, not the complete sharing machine.
- `B-Machine`: Wang's non-erasing tape machine. The name is ambiguous; this
  entry does not describe Kluge's lambda-calculus B-machine or the B-method.
- `CPython Lua`: an explicitly separated comparison of CPython 3.11 and Lua
  5.4. No hybrid implementation or cross-language bytecode ABI is implied.
- `BCPL`: the Cintcode execution model, rather than a specification of the
  BCPL source language.
- `V8`: the Ignition interpreter tier, not all optimizing execution tiers.
- `LispMachine`: a normalized tagged Lisp model inspired by CADR; it is not
  a CADR hardware or microcode description.

The catalogue deliberately pins historical subsets where possible. A machine
entry must be revised, and its references checked, before using it to design
an assembler, verifier, or emulator for a particular real implementation.
