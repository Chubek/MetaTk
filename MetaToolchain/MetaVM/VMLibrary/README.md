# VMLibrary generation

Build from `MetaToolchain/MetaVM`, not an individual machine directory:

```sh
cmake -S . -B out-generation -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON
cmake --build out-generation -j4
ctest --test-dir out-generation --output-on-failure
```

The `metavm-generated-examples` target produces
`out-generation/generated/VMLibrary/<name>/<name>.mvmb` and `<name>.c` for each
catalog profile. The C factory is named `generated_<symbol>_example_module`;
it returns an owned verified module, which the caller destroys using
`mvm_module_destroy`. It does not execute that module. Link emitted C with
Machinery and supply any required host imports (notably DRMVM graphics).

These are **portable Machinery examples**, not generated native guest decoders.
Native implementations are separate named APIs in eight profiles; the other 41
are catalog-only. Machine metadata is not an instruction-set specification.
Unknown guest instructions are never substituted with a portable example.

`tools/generate_catalog.py` owns the catalog adapters and companions. Native
implementation files are not overwritten. See [the generation guide](../docs/GENERATION.md)
for metadata regeneration, independent output directories, Definition lowering
and validation details.
