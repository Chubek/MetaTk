# MetaArch — architecture database

C11 library + `archdb` tool for the DomSymAsCC architecture specifications in
`ArchDB/*.dsymspec.yaml` (schema: `ArchDB/schema/dsymspec.schema.json`).

See the [project manual](manual/README.md) for build instructions, architecture,
specifications, CLI, APIs, encoding, maintenance and known limitations.

An optional **C++20 API, DSLtk-based architecture DSL and spec-driven assembler**
are available through `MetaArch.hpp`, `MetaArch::dsl` and the `metaarch` tool.
See [the DSL/API guide](docs/DSL.md) and [runnable example](examples/toy.march).
The [generated DSL database](MetaArchDB/README.md) contains all 28 architectures
at `MetaArchDB/<architecture>/<architecture>.march`.
The C library and `archdb` remain available without C++ (`ARCHDB_BUILD_DSL=OFF`).

For cross-language use, see [the bindings guide](docs/BINDINGS.md):
`MetaArchABI.h` provides opaque C ABI v1 handles, `MetaArchAPI.hpp` provides an
independent C++17 RAII API, `bindings/d/metaarch.di` provides D declarations,
`bindings/go` provides a Go/cgo package, and `bindings/python` provides
[Python 3.9+ bindings](bindings/python/README.md) implemented with `nanobind`.
These coexist with the native C model API and existing C++20 DSL API without
exposing internal struct layouts.

## Layout

| File | Role |
|---|---|
| `DsymspecIR.h` | Shared IRs: arena, buffer, diagnostics, the format-neutral **document tree** (`DsymNode`), codec entry points, and the **encoder program IR** (`DsymProgram`) |
| `DSymspec.h` | Typed, validated model of a spec (`DsymSpec`, `DsymRegister`, `DsymForm`, …), load / write / lookup / encode API |
| `ArchDB.h` | Catalogue of specs (`ArchDBCatalogue.yaml` or a directory), lazy loading |
| `Dsymspec.c` | Infrastructure, descriptor-driven schema binder (tree ⇄ struct), cross-reference resolution, encoder compiler + VM |
| `ToYAML.c` | YAML codec — **libyaml** event parser/emitter, YAML 1.2 core-schema resolution |
| `ToJSON.c` | JSON codec — **yyjson** |
| `ToXML.c` | XML codec — **libxml2** (`xmlTextWriter` + DOM), W3C XPath 3.1 `json-to-xml` vocabulary |
| `ToSExpr.c` | S-expression codec — built-in reader / pretty printer |
| `ArchDB.c` | Catalogue implementation |
| `tools/archdb.c` | CLI |
| `tests/test_archdb.c` | Round-trip, encoding, diagnostics and catalogue tests |

### Design

Every format converts to and from one ordered, typed tree (`DsymNode`). A single
table of field descriptors per record type drives **both** binding (tree →
struct, with full schema validation: required keys, unknown keys, types,
enums, minimum/maximum, name patterns, `uniqueItems`, `minItems`, `const`) and
unbinding (struct → tree, in schema key order). Reader and writer therefore
cannot drift apart, and adding a format means writing only a tree codec.

After binding, references are resolved into `*_index` fields and checked:
form → mnemonic / encoder / features / syntaxes, register → parent (with
cycle and sub-range checks), target → features, `EMIT_RELOCATION` → relocs,
asmtk → encoders / registers, metachain → registers / mnemonics, duplicate
names, `align` power of two, and `required` ∩ `forbidden`. Diagnostics carry
file, line, column and a JSON-pointer path:

```
bad.yaml:5:5: error: 24 is not one of [8, 16, 32, 64, 80, 128, 256, 512] [at /registers/0/width]
```

## Formats

| Format | Library | Notes |
|---|---|---|
| YAML | libyaml ≥ 0.2.5 | Flow style for small records, `|` blocks for bodies, quoting when a string would read back as another type (incl. YAML 1.1 `yes`/`no`). Comments are not preserved. |
| JSON | yyjson ≥ 0.9 | The backend document is **embedded** as `backend` by default (per the schema); `--ref-backend` keeps `{file: …}`. Reading accepts both forms. |
| XML | libxml2 ≥ 2.9 | `<map>/<array>/<string>/<number>/<boolean>/<null>` with `key=""`, namespace `http://www.w3.org/2005/xpath-functions`; XSLT 3.0 `xml-to-json()` converts it. Control characters use `escaped="true"`. |
| S-expr | built in | `(:key value …)` maps, `(a b c)` lists, `"strings"`, `#t #f #nil`, `+inf.0`. `(:)` is the empty map. |

All four round-trip losslessly (the test suite checks every spec in every
format, including embedded backends). Flag lists are sets and are written in
schema order.

## Encoder pseudo-code

One statement per line; `#` starts a comment.

```
EMIT8 e   EMIT16LE e  EMIT16BE e  EMIT32LE e  EMIT32BE e  EMIT64LE e  EMIT64BE e
EMIT_IMM n, e          # n ∈ {1,2,4,8}, target byte order
EMIT_MODRM reg, rm     # 0xC0 | (reg&7)<<3 | (rm&7)
EMIT_RELOCATION sym, R_NAME   # relocation on the most recently emitted field
REQUIRE cond
```

Expressions: literals (`12`, `0x1F`, `0b101`, `0o17`), operand names, unary
`- ~ !`, `* / %`, `+ -`, `<< >>`, comparisons, `== !=`, `& ^ |`, `&& ||`,
parentheses. 64-bit two's-complement arithmetic; `%` is Euclidean. Bodies are
compiled once to RPN bytecode with constant folding; operands are numbered in
first-use order (`program->params`). Emitted values must fit their field.

Under `DSYM_LOAD_LINT`, compiled (constant-folded) expressions containing
`PARAM c1 ADD c2 MOD` warn with `DSYM-W001` when `c2` is a positive power of
two and `c1` is neither zero nor `c2`. `DSYM_LOAD_STRICT` rejects this pattern
even without lint. Diagnostics name the encoder and body statement number.
Intentional bias must be documented with an exact trailing `# biased` comment
on that statement. This suppresses only this check, not other diagnostics.
See [the encoder audit](docs/encoder-audit.md) for outstanding ISA data issues.

### Checked form encoding and operand mapping

`dsym_encode` / `dsym_encode_named` are low-level APIs: they execute `REQUIRE`
and check emitted field sizes, but **do not enforce form operand constraints**.
Use `dsym_encode_form(spec, form_index, target_index, args, n, &out)` when a
form is known. Both indices are zero-based and must exist. On failure it returns
false and sets `out.error`; discard the output. Encoding reads but never mutates
the resolved spec, so callers may encode concurrently with separate output objects.

The mapping is positional: visit form operands in declaration order, skip
`hidden: true` and `kind: implicit`, and bind the remaining operands to encoder
parameters in **first-use order** (`program->params`). `args` must contain exactly
one integer per visible operand. Operand error positions are 1-based positions
in the original list, including skipped entries. No schema keys were added.
The `implicit_ops` form flag alone does not hide an otherwise visible operand.

- Register integers are hardware `enc` values, not indices into `registers`.
  At least one register with that encoding must match the operand's declared
  class and width. Aliases sharing an encoding are allowed; an integer cannot
  distinguish their names. Width here is register data width, not encoding width.
- Other scalar integers obey `imm_min`, `imm_max`, and `width`/`signed` together.
  With a width, absent `signed` means unsigned; `signed: true` means the usual
  two's-complement interval. Without a width, explicit bounds still apply and
  `signed: false` rejects negative values. Widths of 64 or more are handled
  without shifting by 64. The `int64_t` API cannot express unsigned values above
  `INT64_MAX`. Symbol/relocation arguments are numeric values under the same
  declared scalar constraints, not string symbol names.
- `sym`/`rel` operands must map to parameters used by `EMIT_RELOCATION`, and
  other operand kinds must not map to those parameters.
- A complex memory operand is not silently expanded into multiple parameters:
  a count mismatch is rejected. Use the low-level API for such existing forms
  until their mapping is explicitly representable. This API does not parse
  free-form operand `constraint` strings or evaluate target feature gates.

Resolver lint reports mapping issues without changing the spec data:

| Code | Condition | Severity |
|---|---|---|
| `DSYM-W001` | Suspicious biased modulo | Warning under lint; error under strict, even without lint |
| `DSYM-W002` | Visible operand count differs from encoder parameter count | Lint warning; strict + lint error |
| `DSYM-W003` | Symbol/relocation kind and relocation parameter disagree | Lint warning; strict + lint error |
| `DSYM-W004` | Relocation bit interval does not fit its emitted field | Lint warning; strict + lint error |

Mapping checks need compiled encoders and are skipped with `DSYM_LOAD_NO_COMPILE`.
The checked API enforces the mapping regardless of whether lint was enabled.

### Relocation bit placement

`EMIT_RELOCATION` describes the most recently emitted integer field, even if
`REQUIRE` or another relocation intervenes. `DsymEncodedReloc.offset` and `size`
remain the field's byte offset and byte length. New fields describe placement:

- `bit_offset` is the relocation record's optional `offset` in bits (default 0),
  counted from the **least significant bit of the integer field**, not from the
  first byte in memory. It is field-relative, not output-relative.
- `bit_width` is the relocation record's `width`, not the emitted field width.
- `endian` is the actual emitted field byte order (`LITTLE` or `BIG`), resolving
  `EMIT_IMM` through the selected target. Explicit endian emitters override it.

A linker reads the field using `endian`, inserts the value into
`[bit_offset, bit_offset + bit_width)`, and writes it in the same byte order.
This describes a contiguous bit interval; it does not invent split-bit ISA
relocations, symbol resolution, scaling or PC-relative expression evaluation.
Those still use the relocation record's other semantics.

A 26-bit relocation in a 32-bit field is valid (including offset 6). A 32-bit
relocation in a byte, or an offset that pushes any bits past the field, is not.
Lint reports `DSYM-W004`; strict + lint rejects the spec. All encoder entry
points reject non-fitting relocations at runtime even if lint was not requested,
instead of returning misleading linker metadata. Discard partial output on failure.

**ABI note:** adding these fields enlarges `DsymEncodedReloc` and consequently
`DsymEncoded`. Rebuild the library and all consumers together; this is not a
binary-compatible structure change. Existing source field names are unchanged.

```c
DsymSpec *s = dsym_spec_load_file("ArchDB/riscv64.dsymspec.yaml", 0, 0, &diags);
const char *n[] = {"rd", "rs1", "imm"};
int64_t v[] = {10, 10, 1};
DsymEncoded out;
dsym_encode_named(s, "addi", 0, n, v, 3, false, &out);   /* 13 05 15 00 */
```

## Build

```
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

System packages are used when present (`libyaml-dev libyyjson-dev libxml2-dev`);
otherwise, with `-DARCHDB_FETCH_DEPS=ON` (default), they are fetched and built
statically.

## CLI

```
archdb validate [--strict] [--lint] [--backend] FILE...
archdb convert -f yaml|json|xml|sexpr [-o OUT] [--embed-backend|--ref-backend] FILE
archdb list [ArchDBCatalogue.yaml|DIR]
archdb catalogue ArchDB --root ArchDB -o ArchDBCatalogue.yaml
archdb encode ArchDB/x86.dsymspec.yaml add dst=0 src=1      # 01 c8
archdb dump-ir ArchDB/riscv64.dsymspec.yaml addi
archdb info ArchDB/amd64.dsymspec.yaml
```
