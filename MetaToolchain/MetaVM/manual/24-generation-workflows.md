# Chapter 24 — Generation workflows and generated C

[Previous](23-catalog-and-legacy-adapters.md) · [Contents](README.md) · [Next](25-adding-a-machine.md)

## Three different generation operations

1. **Catalog regeneration** writes owned source adapters, metadata, and examples.
2. **Example compilation** converts manifest-listed `.mvm` examples to `.mvmb`
   artifacts and serialized C factories.
3. **Definition lowering** translates one explicit guest program into Machinery
   operations; its state convention differs from ordinary catalog examples.

None automatically generates the 49 historical native runtimes. Native semantics
must be implemented and tested through their separate APIs or a suitable explicit
definition pipeline.

## Catalog source maintenance

```sh
python3 tools/generate_catalog.py --check
python3 tools/generate_catalog.py
python3 tools/generate_catalog.py --output-root out-manual/catalog-source
```

`--check` reports stale/missing owned files and exits 1 without modifying them.
Generation renders and validates the entire planned output before writing.
It rejects invalid names, colliding C symbols/header guards, and missing
referenced native headers. Unchanged files retain bytes and timestamps.
Unrelated files, even empty build artifacts, are ignored. Native implementations
are outside generator ownership. An alternate root receives generated files
only, not a copied complete source release.

## Built binary and C examples

The default root CMake build runs target `metavm-generated-examples`:

```sh
cmake --build out-manual --target metavm-generated-examples
python3 tools/generate_examples.py --vmweave out-manual/vmweave \
  --output-root out-manual/other-generated
```

For every manifest profile it produces
`generated/VMLibrary/<name>/<name>.mvmb` and `<name>.c`, plus
`generated/catalog_factories.hpp`. CMake and the script enumerate the same
manifest inventory; unrelated assembly fixtures are not implicit inputs.

Compilation of the entire set finishes in a temporary directory before
destination publication. Validation/compiler failures preserve existing outputs.
Each changed artifact is then atomically replaced. Publication is not a
multi-file transaction: an I/O failure during that stage can leave a mixed set.
There is no fsync durability promise. The script preserves unchanged artifact
mtimes; CMake deliberately touches declared build outputs after a successful
rule to keep dependency timestamps current. Source-root output is rejected to
protect catalog source `.c` files.

## C emitter API and host contract

`vmwv_compile_assembly` wraps assembly compilation. `vmwv_emit_c` writes a
verified serialized module to a caller-owned `FILE*`; stream failure can leave
a partial stream. `vmwv_emit_c_file` stages output before atomic replacement,
with a 128 MiB emitted-text ceiling. Emission rejects modules exceeding default
factory-deserialization limits before publishing an unloadable factory.

Generated factory names are `generated_<symbol>_example_module` for catalog
artifacts. Application emission chooses its own name: start with an ASCII letter,
continue with letters/digits/underscores, avoid C keywords, `main`, framework
prefixes `mvm_`/`MVM_`/`vmwv_`, and names over 255 bytes. An application-specific
prefix also avoids collisions with headers and other linked factories.

```c
extern mvm_status application_answer(mvm_module **, mvm_error *);
```

Compile emitted `.c` as C11 and link Machinery. On success the caller owns
the returned module and destroys it; the factory does not execute it or bind
imports. Definition factories additionally need their two state arrays.

The generation regression compiles all 49 emitted C files, checks byte parity
against source/catalog modules, exercises stream/file emission, reloads binaries,
and executes both engines. This proves the artifact pipeline, not guest ISA
compatibility.

**References:** [generation details](../docs/GENERATION.md), [emitter header](../vmweave/vmweave.h).
