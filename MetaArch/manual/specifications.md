# Specifications and formats

## Schema 1 model

Each document declares `schema: 1` and one or more targets. Refer to the
[JSON Schema](../ArchDB/schema/dsymspec.schema.json) for exact required keys,
enums, patterns and numerical bounds; the C types live in `DSymspec.h`.

| Section | Meaning |
|---|---|
| `targets` | Architecture/subarchitecture, address and operand sizes, byte order, object format, ABI and enabled features |
| `features` | Named capabilities and descriptions |
| `registers` | Names, classes, widths, encodings, flags and optional parent/subranges |
| `mnemonics` | Instruction spellings, aliases and case behavior |
| `encoders` | Named emission programs and declared byte/relocation budgets |
| `forms` | Mnemonic/encoder combinations, operand constraints and feature/syntax references |
| `syntaxes` | Assembly spelling/format records |
| `sections`, `linkages`, `relocs` | Object-file and symbol/relocation metadata |
| `debug` | References connecting debug records to architecture entities |
| `asmtk`, `metachain` | Consumer-specific instruction/lowering/allocation metadata |
| `backend` | External generated JSON reference or an already embedded backend tree |

Optional scalar fields in C use `has_*`; strings use NULL for absence; lists
track presence separately from empty lists. Names resolve to `*_index` fields.
Unknown references, duplicate names, cycles and schema violations are errors.

Minimal encoder-bearing example:

```yaml
schema: 1
targets:
  - {arch: toy, addr_size: 32, byte_order: little}
encoders:
  - {name: byte, body: "EMIT8 value"}
```

## Codecs

| Format | Representation and caveats |
|---|---|
| YAML | libyaml events; YAML 1.2 core scalar resolution; block encoder bodies; ambiguous strings quoted on write. Comments are discarded. |
| JSON | yyjson; ordinary schema-shaped objects. A loaded backend embeds by default unless reference output is requested. |
| XML | libxml2; W3C XPath-functions namespace `http://www.w3.org/2005/xpath-functions`, `<map>`, `<array>`, `<string>`, `<number>`, `<boolean>`, `<null>`; map keys use `key`. |
| S-expression | Built-in reader/writer; keyword plists `(:key value ...)`, sequences `(a b ...)`, empty map `(:)`, strings, integers/reals, `#t`, `#f`, `#nil`. |

Conversion preserves the typed model, not source text. Field/flag order is
normalized; optional empty lists survive. Compact mode reduces whitespace.
`NO_HEADER` omits generated comments where applicable; `SORT_KEYS` is for generic
trees, not a promise to alphabetize typed spec records.

## Backend handling

The schema-style reference is `backend: {file: backend/<name>.json}`. Resolve it
relative to the spec's source directory. Explicit load calls or the BACKEND
load flag populate an arena-owned tree. Writers do not load it behind the
caller's back. JSON embeds a loaded document by default; other formats require
the EMBED_BACKEND write flag. REF_BACKEND preserves the file reference instead.

`archdb convert` explicitly loads the backend when it will embed it; native API
callers choose when to load. Backends can be much larger than the hand-authored
spec. `ArchDB/backend/NOTICE.md` describes the extracted provenance and notices.
Do not edit extracted JSON to correct an instruction in a hand-authored spec.

## Compatibility boundaries

Only schema 1 is supported. Embedded backend output is accepted by the library
but the reference-only JSON Schema does not describe arbitrary embedded backend
objects. YAML comments/source order are not preserved, and no comment-preserving
`fmt` command exists. JSON validation diagnostics may lack line/column positions.
The XML invalid-UTF-8 escape compromise remains in the existing codec; central
strict/lossy UTF-8 handling is not implemented. Do not treat successful internal
round trips as independent validation by other tools.
