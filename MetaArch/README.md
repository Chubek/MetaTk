# MetaArch — architecture database

C11 library + `archdb` tool for the DomSymAsCC architecture specifications in
`ArchDB/*.dsymspec.yaml` (schema: `ArchDB/schema/dsymspec.schema.json`).

An optional **C++20 API, DSLtk-based architecture DSL and spec-driven assembler**
are available through `MetaArch.hpp`, `MetaArch::dsl` and the `metaarch` tool.
See [the DSL/API guide](docs/DSL.md) and [runnable example](examples/toy.march).
The C library and `archdb` remain available without C++ (`ARCHDB_BUILD_DSL=OFF`).

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
