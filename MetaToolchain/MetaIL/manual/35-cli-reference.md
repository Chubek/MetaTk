# 35. Command-line reference

The native entry point is `cli/metail.c`; the command implementation is shared
with runtime services in Lua. `cli/MetaIL-CLI.yaml` records the command contract.

| Command | Purpose |
|---|---|
| `check FILE` | Validate a textual program |
| `emit FILE [--out FILE]` | Canonicalize text |
| `optimize FILE` | Transform and emit |
| `analyze FILE` | Print JSON analyses |
| `kernels` | List or query declarations |
| `selftest` | Runtime/inventory smoke test |
| `verify` | Check bundled declarative rewrite semantics |
| `lua SCRIPT` | Run a Lua script with native bindings available |

Optimization accepts `--passes`, `--repeat`, `--out`, and `--stats`. Analysis
accepts `--passes` and `--out`. Both accept `--kernel-dir DIR`. `--out -` writes
stdout; omitting `--out` also uses stdout.

```sh
build/metail kernels --search 'multiply power' --tag strength-reduction --paths
build/metail kernels --tag numeric --tag statistics --kind analysis --json
build/metail kernels --search AddZeroRight --source
```

Kernel discovery supports `--search`, repeated `--tag`, `--use`, `--kind`, and
`--kernel-dir`. Choose at most one of `--json`, `--paths`, or `--source`; otherwise
the default human-readable listing is printed. A search with no matches succeeds.

Exit code 0 means success. Code 1 covers command, input, I/O, and execution errors.
Code 2 covers native entry-argument or runtime-initialization failures. The `lua`
command preserves trailing arguments in the Lua `arg` table.

[Contents](README.md) · [Next: Embedding](36-embedding.md)
