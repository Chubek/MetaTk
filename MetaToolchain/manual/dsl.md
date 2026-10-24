# Architecture DSL and assembler

The optional C++20 implementation uses DSLtk parsing and the existing C binder
and VM, not a separate architecture semantics engine. The full grammar,
constraints and C++ API are in [docs/DSL.md](../docs/DSL.md).

```text
architecture {
    schema = 1;
    target { arch = toy; addr_size = 32; byte_order = little; }
    encoder byte { body = "EMIT8 value\n"; }
    assembly = { byte = [value]; };
}
```

Assignments end in `;`. Named declarations append schema records; lists/maps
may also be assigned directly. Do not mix declaration and list assignment for
the same key. Strings use JSON escapes; numbers support signed decimal/hex/
binary/octal, not floating-point literals. `#` introduces comments. `assembly`
is a DSL-only explicit encoder parameter-order mapping, removed before schema
binding and **not exported** through the four schema formats.

Assembly accepts one encoder invocation per line with `#`/`;` comments:

```asm
addi rd=a0, rs1=a0, imm=-1
```

Registers are case-sensitive declared names, converted to hardware encodings.
Without explicit `assembly` bindings, named syntax is low-level. Positional
syntax requires a binding from DSL metadata or `--bind addi=rd,rs1,imm`.
With a binding, available form checks and selected-target features apply. It
never silently guesses parameter order from bytecode or picks a mnemonic alias.

This is not a full system assembler: no labels, directives, expressions, memory
addressing, relaxation or linker symbol resolution. Named and positional syntax
cannot be mixed. Failure is transactional; no partial assembled output is
returned. Library relocation offsets are rebased over assembled instructions;
the raw-binary CLI rejects relocations.

`metaarch::Spec` is move-only. Its methods return `Result<T>`; test the result
before dereferencing `get()` or reading `error()->diagnostics`. Native pointers
from `get()` and register lookups are borrowed. Encoded data and errors own their
storage. Defaults for DSL source size/output size are 16 MiB, nesting depth 64;
these are not general hostile-input memory/CPU quotas.

## Generated database

`MetaArchDB/<architecture>/<architecture>.march` contains all 28 generated
models. Each backend is a relative symlink into `ArchDB/backend`; keep the two
trees together and preserve symlinks. Provenance comments remain in YAML.

```sh
python3 tools/compile_archdb.py --check
python3 tools/compile_archdb.py
```

Build `metaarch` first; `--tool` selects another binary. Generation loads each
DSL result and compares its serialized model with the YAML input. See
[MetaArchDB/README.md](../MetaArchDB/README.md).
