# 34. Custom kernels and metadata

Each `.optk` file must register exactly one uniquely named kernel. Its filename
may differ from the registered identifier. Load a custom directory through
`Context::load_kernels`, `metail_load_kernels`, or CLI `--kernel-dir`.

```lua
local K = lkopt.optimizer.new "ProjectAddZero"
K:rewrite { pattern = "add($x,0)", replace = "$x" }
K:metadata {
  description = "Remove redundant widget additions",
  keywords = { "widget", "identity" },
  tags = { "project", "arithmetic" },
  uses = { "simplify widget expressions" }
}
```

The description overrides its derived description. Keywords, tags, and uses
extend metadata derived from the declaration's kind, name, pattern, and analysis
configuration. Lists must be dense arrays of nonblank, single-line strings without
NUL. Unknown metadata keys are errors.

Directories are scanned recursively in deterministic path order. Symlink cycles
are not followed by recursive scanning. Duplicate registered names, invalid guards,
unknown implementations, or malformed metadata fail the load. A failed file or
directory load rolls back additions made by that load.

Kernel execution receives a restricted environment with the registration API,
numeric facade, and selected standard facilities. Library tables are private to
the file. Kernels remain trusted local Lua programs; the loader is not a general
resource-isolation service.

Declare an analysis with `K:analysis {kind="cfg"}` or select an existing procedure
with `K:builtin "constant-propagation"`. New algorithm names require an actual
runtime implementation, validation, and tests.

[Contents](README.md) · [Next: CLI](35-cli-reference.md)
