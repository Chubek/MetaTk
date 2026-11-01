# mulib-dsymspec

Owned, read-only queries over **the actual supplied `dsymspec/` architecture
files**. The generated snapshot includes all 28 schema-1 YAML documents, their
targets, registers, features, mnemonics, forms, encoder bodies, syntaxes,
sections, linkages, relocations, and optional extensions. Architecture coverage
is metadata coverage, **not executable assembler coverage**.

## Integration

Define `MULIB_IMPL_DSYMSPEC`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_STRBUF`, and `MULIB_IMPL_UTF8` before MuLIB includes in exactly one
C11 translation unit. Other units include `mulib-dsymspec.h` without switches.
Declarations support C++17. No implementation switch enables another module.
Ship `include/mulib-dsymspec-data.inc` beside the header. No runtime filesystem,
Python, YAML library, third-party C library, or architecture database is needed.

## Generation and provenance

The existing YAML module intentionally rejects literal block scalars, which
the supplied encoder bodies use. Rather than introduce a second incomplete
YAML parser, `tools/dsymspec_generate.py` uses **generation-time** PyYAML and
jsonschema. It rejects duplicate mapping keys, validates every document against
`dsymspec/schema/dsymspec.schema.json`, and emits deterministic C string chunks.
It never modifies the supplied files and never executes pseudo-code.

```sh
python3 tools/dsymspec_generate.py          # regenerate checked-in snapshot
python3 tools/dsymspec_generate.py --check # schema validation + freshness check
python3 tools/dsymspec_generate.py --json amd64 > /tmp/opencode/amd64.json
```

Generation requires Python 3.9+, PyYAML and jsonschema; these are not required to
compile/run MuLIB. `_mulib_codegen` records source path and SHA-256 digests of
both the YAML and schema. These are provenance, not runtime authentication.
Integers outside the JSON DOM's exact-double range are preserved as decimal
**strings**, currently the two large immediate bounds in AMD64 and BPF.
No other source metadata is replaced with invented architecture facts.

The supplied AMD64 `asmtk.encodings` extension refers to pseudo-code encoder
templates. The generator recognizes only their audited `EMIT8` constants,
opcode-plus-register, ModRM direction/group, immediate-field, and named
relocation templates. It resolves these into `_mulib_codegen.encodings`.
Unknown templates abort generation. The raw bodies remain queryable strings.
Other architecture bodies are retained as data, not interpreted.

## API and ownership

`mulib_dsymspec_builtin_count/name` enumerate exact architecture identifiers.
`mulib_dsymspec_load_builtin("amd64", &error)` returns an owned independent DOM.
`mulib_dsymspec_load_json(bytes, length, &error)` loads copied JSON, including
the generator's JSON output. It does **not** accept YAML. Raw schema-1 JSON
without `_mulib_codegen` supports queries but not assembly.

`count(spec, group)` and `record(spec, group, index)` query root arrays.
`find(spec, group, name)` uses `name`, except `targets` uses `arch` and `forms`
uses `mnemonic` and returns the first form. Names are exact/case-sensitive;
aliases are not expanded. `document(spec)` exposes the complete borrowed DOM;
`encodings(spec)` exposes the generated executable table, if any. Use JSON DOM
queries for fields and nested arrays. **Never mutate borrowed nodes.** All
borrowed strings/nodes expire at `mulib_dsymspec_destroy(spec)`; source bytes
may be freed immediately after loading. NULL destruction is harmless.

`backend.file` is retained verbatim as a reference to the supplied backend JSON.
Backend files are not opened or merged automatically; no path resolution or
TableGen predicate/scheduling interpretation is claimed.

## Validation, limits, and failures

Loading returns NULL with an optional static error message. JSON parse errors
include a zero-based byte offset; semantic/allocation errors use offset zero.
Success clears the error. No partial owned object escapes.

The default input limit is 4 MiB, configurable with
`MULIB_DSYMSPEC_MAX_BYTES` in the implementation TU. JSON nesting is bounded by
the JSON module (128). Core named groups, targets, forms and resolved encodings
are limited to 4096 records each. Allocation sizes and concatenation are
overflow-checked. Name lookup is linear; duplicate detection is quadratic
within the bounded named groups. There is no mutable global cache.

Runtime validation checks schema version, unique target architecture identifiers,
targets/address size/byte order and
power-of-two alignment; unique printable ASCII names; register class/width/
encoding and parent references; nonempty encoder bodies; relocation widths;
form mnemonic/encoder references; and all fields used in resolved encodings.
It is **not a general JSON Schema implementation**: unused optional fields,
feature relationships, arbitrary extension properties, register parent cycles,
and pseudo-code semantics are not comprehensively validated. Full supplied
schema validation occurs during generation. Treat custom metadata as trusted
code-generation input, not as authenticated ISA definitions.

## Runnable example and tests

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/dsymspec.c -lm -o /tmp/opencode/mulib-dsymspec
/tmp/opencode/mulib-dsymspec
```

`tests/test_codegen.c` loads all supplied snapshots, reads the real AMD64 YAML
and schema, checks records/bodies, tests ownership and JSON roundtrips, and
rejects malformed/invalid metadata. The generator's `--check` verifies the
checked-in snapshot still matches all real source files.
