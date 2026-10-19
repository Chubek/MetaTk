# Architecture DSL and assembler API

The optional `MetaArch::dsl` target adds C++20 ownership, checked results and
DSLtk-based parsers. `MetaArch::metaarch` remains C11 with no C++ dependency.
`MetaArch.hpp` does not expose or require DSLtk headers in consumer code.
DSLtk is needed only when building `metaarch_dsl`.

## Build and run

```sh
cmake -S . -B build -DARCHDB_DSLTK_DIR=/mnt/nvme/metatk/DSLtk
cmake --build build
ctest --test-dir build --output-on-failure
build/metaarch assemble examples/toy.march examples/toy.asm
# 11 ff 10 2a
build/metaarch assemble examples/toy.march examples/toy.asm -o toy.bin
build/metaarch convert examples/toy.march -f yaml -o toy.yaml
```

Disable this layer entirely with `-DARCHDB_BUILD_DSL=OFF`. The legacy `archdb`
tool is unchanged. The new `metaarch` tool accepts `.march` specifications or
any of the four existing schema formats.

## Architecture language

See [`examples/toy.march`](../examples/toy.march) for a complete runnable spec.

```text
architecture {
    schema = 1;
    target { arch = toy; addr_size = 32; byte_order = little; obj_format = ELF32; }
    register r0 { class = gpr; width = 32; enc = 0; }
    encoder byte { body = "EMIT8 value\n"; }
    assembly = { byte = [value]; };
}
```

The grammar uses DSLtk's `Parser`, `satisfy`, `ch`, sequence/choice/repetition
primitives and `run_parser`. Recursive productions build `DsymNode` records;
the **existing C schema binder** validates and compiles them. There is no
second implementation of architecture validation or encoder execution.

```ebnf
document    = "architecture", "{", { declaration | assignment }, "}";
assignment  = identifier, "=", value, ";";
declaration = ("target" | "form"), object
            | ("register" | "feature" | "mnemonic" | "encoder" | "syntax"
               | "section" | "reloc"), identifier, object;
object      = "{", { assignment }, "}";
value       = object | "[", [value, {",", value}], "]"
            | string | integer | identifier;
```

Declarations append to their schema lists; named declarations insert `name`.
All schema fields can also be written directly as assignments, including
`targets`, `forms`, `backend`, `asmtk` and `metachain`. Do not mix list
assignment and declaration syntax for the same field. Duplicate assignments
are errors. Whitespace and `#` line comments are ignored outside strings.
Identifiers start with an ASCII letter or underscore and continue with
letters, digits, `_`, `.` or `-`. Other strings must use double quotes.
Strings use JSON escapes (including Unicode); encoder programs use `\n` for
statement separators. `true`, `false`, `null` are typed values, not strings.
Integers are signed 64-bit decimal, `0x`, `0b`, or `0o` with optional sign.
Floating-point literals and multiline raw strings are not implemented.

`assembly` is an extension, removed before schema binding. Each entry maps an
**encoder name** to its parameters in source operand order. It must cover every
compiled parameter exactly once. When forms exist for that encoder, this order
must also match `form.operands`; the assembler accepts a form only if its
metadata checks pass. It never guesses order from encoder parameter first-use.
Custom constraints, hidden operands and complex operand kinds are rejected by
the checked form path rather than silently ignored. Target features are taken
from the selected target's `features` list.

Schema-1 serialization writes the architecture model only: **assembly bindings
are not exported** to YAML/JSON/XML/S-expressions. Retrieve bindings using
`assembly_bindings()` and supply them as assembler options when necessary.
The DSL is an input language, not a comment-preserving editing format.

## Assembly language

One encoder invocation per line; `#` or `;` starts a comment. Registers are
looked up in the loaded spec (including explicitly listed aliases) and converted
to their `enc` values. Names are case-sensitive. Integers follow the DSL rules.

```asm
addi rd=a0, rs1=a0, imm=1  # works with the existing RISC-V YAML spec
add dst=eax, src=ecx      # works with the existing x86 YAML spec
```

Named operands can be reordered, but duplicate, missing and unknown parameters
are errors. Without explicit bindings, this is a low-level encoder interface:
only encoder `REQUIRE` statements/output checks apply, not form metadata.
With a binding, form checks also apply to named syntax.

Conventional positional syntax requires DSL `assembly` metadata or an override:

```sh
build/metaarch assemble ArchDB/riscv64.dsymspec.yaml program.asm \
  --bind addi=rd,rs1,imm --target 0
```

```asm
addi a0, a0, 1
```

There is **no instruction selection by mnemonic**, mnemonic alias expansion,
register-prefix syntax, directives, memory addressing, labels, symbol resolution
or relaxation yet. Use encoder names, not necessarily mnemonic names. Mixing
named and positional operands is rejected. The library returns owned relocation
records with rebased offsets; the CLI refuses relocations because raw binary
cannot represent them. Assembly is transactional: failure returns no partial
output. The CLI assembles fully before opening its output file.

## C++ API

```cpp
#include <MetaArch.hpp>

auto loaded = metaarch::Spec::load("ArchDB/riscv64.dsymspec.yaml");
if (!loaded) { /* inspect loaded.error()->diagnostics */ return; }
auto &spec = *loaded.get();
auto machine_code = spec.assemble("addi rd=a0, rs1=a0, imm=1\n");
if (!machine_code) { /* inspect error diagnostics */ return; }
const auto &bytes = machine_code.get()->bytes;
```

`Spec` is move-only and owns the C spec. Its `get()` pointer and lookup results
are borrowed and must not outlive it. Diagnostics, serialized strings, machine
bytes and relocation strings own their storage and may outlive it. Load warnings
remain available in `diagnostics()`. Use `encode()` for strict named low-level
encoding, or `encode_form()` with an explicit parameter map for kind/class/width,
immediate-bound and feature checks. `Result<T>` exposes nullable `get()` and
`error()`; test it before accessing either. Ordinary input errors do not throw;
standard C++ allocation failures may still throw `std::bad_alloc`.

DSL source size (default 16 MiB), nesting depth (64), and assembly output size
(16 MiB) are configurable. These are not general-purpose hostile-input memory
or CPU quotas: the C binder/encoder and STL allocations retain their own behavior.
`load_dsl()` enforces its source limit while reading. `assemble()` accepts an
already-owned string view; the CLI currently reads its entire source file.

Read-only operations on a resolved `Spec` use per-call state and can be used
concurrently, provided no caller mutates its underlying C structures. Moving or
destroying it concurrently with an operation is invalid. Mutable catalogues and
the underlying C loading/backend mutation APIs still require external locking.
