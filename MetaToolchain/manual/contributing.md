# Maintaining and extending MetaArch

## Add an architecture

1. Read `ArchDB/schema/dsymspec.schema.json` and a small existing spec such as
   `ArchDB/lanai.dsymspec.yaml`. Choose a unique catalogue filename/name.
2. Add schema 1 and target records: address size, byte order, architecture,
   subarchitecture, object format and ABI where known. Do not infer hardware
   semantics from another ISA's spelling.
3. Add register classes, data widths and actual hardware encodings. Declare
   parent/subranges and aliases only when supported by authoritative references.
4. Add mnemonics, encoders and forms. Record visible operand order explicitly in
   your design: checked C/ABI encoding maps to encoder **first-use** parameter
   order. Verify register/immediate/relocation constraints and target features.
5. For each encoding, cross-check boundary bytes against an ISA manual and an
   independent assembler (`llvm-mc --show-encoding` or GNU as/objdump). Put cases
   in `tests/encodings.tsv`: arch, encoder, operand=value pairs, expected hex.
   Cover most negative, -1, 0, 1 and most positive signed values where applicable.
6. If a backend is needed, regenerate it through the external extractor, retain
   provenance/licence notices and use the schema's `backend/<name>.json` path.
   Never hand-edit `ArchDB/backend/`.
7. Validate the spec, review lint, regenerate catalogue and DSL data, run the
   full suite and independently inspect representative output.

```sh
build/archdb validate --lint ArchDB/newarch.dsymspec.yaml
build/archdb catalogue ArchDB --root ArchDB -o /tmp/ArchDBCatalogue.yaml
python3 tools/compile_archdb.py
ctest --test-dir build --output-on-failure
```

Compare generated catalogue output before updating the committed file. The
current Python test deliberately asserts the shipped count (28); an intentional
new architecture requires updating that expectation with an explanation, not
silently dropping coverage. Resolve spec defects from references, never by
blindly replacing similar-looking immediate expressions.

## Code changes

- Keep public C headers portable C11. Keep codec dependencies optional/fetchable
  through the established system-package-first approach.
- Add a codec by implementing tree read/write; reuse schema binding/resolution.
- When changing schema fields, update both JSON Schema and C descriptors.
  Automated bidirectional reflection/drift checks are not implemented yet.
- Freeze ABI-v1 view layouts; new fields need new ABI view types/functions.
- Test failures and negative paths, ownership, boundaries and all four formats.
- Record data changes separately in `CHANGELOG.md`, including the manual or
  TableGen definition and independent expected-byte source.
- Follow `AGENTS.md` for baseline, GCC/Clang warning policy and sanitizer checks.
  Preserve unrelated work and generated-source boundaries.

Use [the existing encoder audit](../docs/encoder-audit.md), [changelog](../CHANGELOG.md),
[baseline](../BASELINE.md) and [prior phase report](../REPORT.md) for historical
decisions. The nanobind implementation does not change architecture data,
schema or native ABI.
