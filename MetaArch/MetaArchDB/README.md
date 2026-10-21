# MetaArch DSL database

Each architecture has its own directory and specification:
`MetaArchDB/<architecture>/<architecture>.march`.
These are generated from the 28 `ArchDB/*.dsymspec.yaml` files using MetaArch's
schema reader and the [architecture DSL](../docs/DSL.md).

All architecture model fields and encoder bodies are retained. Each directory
contains a `backend/<architecture>.json` relative symlink to the existing file
in `ArchDB/backend/`, preserving the schema-required backend path without
duplicating large JSON files. Keep `ArchDB` alongside `MetaArchDB` when moving
the database, and preserve symlinks.
Source comments remain in the original YAML files. Schema serialization
normalizes field and flag ordering. Assembly uses the same named-operand
interface as the YAML specs; no positional operand bindings are inferred.

## Regenerate or verify

Build the `metaarch` tool with `ARCHDB_BUILD_DSL=ON`, then run from the project root
(Python 3.9 or newer; standard library only):

```sh
python3 tools/compile_archdb.py
python3 tools/compile_archdb.py --check
```

Use `--tool build-asan/metaarch` (or another build path) to select a different
binary. Both modes load each generated DSL file through MetaArch and compare
its serialized model with the YAML model. `--check` also checks that generated
text is up to date and backend symlinks resolve to the original files.

## Use

```sh
build/metaarch convert MetaArchDB/riscv64/riscv64.march -f json
echo 'addi rd=a0, rs1=a0, imm=-1' | \
  build/metaarch assemble MetaArchDB/riscv64/riscv64.march -
# 13 05 f5 ff
```
