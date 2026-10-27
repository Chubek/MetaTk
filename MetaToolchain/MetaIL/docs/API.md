# Native API and kernel registry

## Structured IR and the C++ native DSL

The complete guide is the [40-chapter manual](../manual/README.md). `MetaIL.h` now
provides `metail_expr`, `metail_stmt`, `metail_function`, and `metail_module`
descriptors. `metail_module_check`, `metail_module_emit`, `metail_module_analyze`,
and `metail_module_optimize` transfer these structures directly to canonical Lua IR
tables; they do not generate or parse MetaIL text. `metail_parse` imports textual
DSL into an owned descriptor tree. Parse/optimization results are independently
owned, inspectable snapshots freed with `metail_module_free`; caller-built inputs
are borrowed and never modified.

`MetaIL.hpp` uses the real `DSLtk.hpp` AST facility and requires **C++20**. Configure
`DSLTK_ROOT` to the directory containing that header (workspace default
`../../DSLtk`). Installation includes the selected DSLtk header with its embedded
notices. Build `Expr` values with `var`, integer literals, arithmetic/bitwise/
comparison operators, `call`, `select`, `land`, and `lor`. Functions have fluent
`let`, `ret`, `eval`, `assume`, `label`, `jump`, and `branch` methods. Modules and
functions expose their own value-owned vectors for programmatic inspection/editing.

```cpp
metail::Module module("demo");
auto x = metail::var("x");
module.function("scale", {"x"}).ret((x + 0) * 8);
metail::Context context;
auto result = context.optimize(module); // result.module and result.stats
auto text = context.emit(result.module);
auto parsed = context.parse(text);
parsed.functions[0].statements.back().expr = metail::integer(42);
```

`Expr::ast()` exposes a const DSLtk AST; `Expr::from_ast` accepts validated DSLtk
trees. Comparison operators build IR expressions, not host booleans; use
`same_as` for structural equality. Expressions have no bool conversion. C++ `if`
and loops execute while constructing the IR; use `branch`/`jump` for IR control
flow. The C++20 requirement applies to `MetaIL.hpp` and `KernelRegistry.hpp`; the
C API remains C11. Existing text-based Context overloads remain supported.

`Expr::visit(visitor, order)` runs a DSLtk visitor over const AST nodes.
`Expr::substitute(bindings)` returns a simultaneous variable substitution.
`metail::rewrite(expr, rules, limits)` returns a `RewriteResult` containing
`expression`, `passes`, and `applications`. `metail::rewrite_expressions` applies
the same rules to all module expressions and returns `ModuleRewriteResult`
containing `module`, `expressions`, `passes`, and `applications`.
These operations own their results and leave inputs unchanged, with no Lua
context required. `RewriteLimits` defaults to 100 passes per expression and
10,000 applications (shared across a module). Malformed replacement trees and
budget exhaustion throw `metail::Error`; callback exceptions propagate.
The checks enforce structure and termination bounds, not semantic equivalence.
See the [DSLtk chapter](../manual/22-dsltk-integration.md) for traversal order,
convergence, substitution, and effect/trap obligations.

## Build and consume

The C11 implementation in `MetaIL.c` embeds the genuine Lua 5.4 runtime and calls
`lua/metail/api.lua`. That module shares validation, pass selection, optimization,
and analysis with the CLI. `lua/metail/ir.lua` remains the canonical IR. No child
processes or temporary program files are needed for native API calls.

```cmake
project(consumer LANGUAGES C CXX)
find_package(MetaIL CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE MetaIL::MetaIL)
```

Use `-DCMAKE_PREFIX_PATH=/your/install/prefix` for an installed package, or use
`add_subdirectory(/path/to/MetaIL metail-build)` instead of `find_package`.
Headers are installed under `include/metail`, the library under the configured
library directory, and runtime data under `share/metail`. The exported target
propagates the header path, C++20 requirement, and native link dependencies.

Runtime lookup accepts an explicit home path, then `METAIL_HOME`, the current
directory, configured install root, and configured source root. Explicit paths and
`METAIL_HOME` are authoritative: an invalid value is an error. The CLI additionally
checks paths relative to `argv[0]`. For relocated installations set
`METAIL_HOME=<prefix>/share/metail` or pass that directory to the context constructor.
Contexts anchor their runtime paths at creation; subsequent working-directory
changes do not change their runtime root.

## C ownership and errors

```c
#include <MetaIL.h>
#include <stdio.h>

int main(void) {
    char error[2048];
    metail_context *ctx = metail_create(NULL, error, sizeof error);
    if (!ctx) { fprintf(stderr, "%s\n", error); return 1; }
    char *output = NULL, *stats = NULL;
    metail_status status = metail_optimize(ctx,
        "fn f(x) { return x + 0; }", "example.mi", "AddZeroRight", 2,
        &output, &stats);
    if (status == METAIL_OK) fputs(output, stdout);
    else fprintf(stderr, "%s\n", metail_last_error(ctx));
    metail_free(output);
    metail_free(stats);
    metail_destroy(ctx);
    return status == METAIL_OK ? 0 : 1;
}
```

- Contexts own their Lua state. Calls on one context must be serialized; separate
  contexts are independent and may be used concurrently.
- Source, filename, pass lists, and paths are NUL-terminated strings. Native APIs
  accept source text in memory. File reading is a separate C++ convenience method.
- `metail_emit`, `metail_optimize`, `metail_analyze`, and `metail_kernels_json` return
  allocated strings. Release them with `metail_free`, including when the library
  was built shared. Output slots must be distinct and start a fresh result; free
  previous values before reusing a slot. Failures set supplied output slots to NULL.
- `metail_last_error` returns borrowed diagnostic text (up to 2047 bytes) until the
  next call on the context. Creation copies errors into the caller's buffer.
- `metail_kernel_get` uses a zero-based index. Its strings are borrowed until the
  next context call. Keywords, tags and uses are newline-delimited lists.
- Invalid programs, unknown passes, wrong pass kinds, and failed kernel loads
  return errors; the context remains reusable. The API never calls `exit`.
- `NULL`/`"all"` selects the default pass pipeline. An empty pass string, empty
  element, or unknown name is an error. Optimization rounds must be 1 through 12.
  Analysis returns JSON. Optimization returns emitted MetaIL and optional JSON
  per-selected-kernel change counts; automatic final cleanup is not counted there.

`MetaIL.hpp` mirrors these operations with RAII strings, `OptimizationResult`,
exceptions (`metail::Error`), and a move-only `Context`. Embedded NUL arguments
are rejected rather than silently truncated. `native_handle()` is borrowed and
must not be destroyed by the caller. Moved-from contexts reject operations.

## Requesting kernels

`KernelRegistry` snapshots declarations loaded by `lkopt`, rather than guessing
registered names from filenames. The registry initially loads the 327 bundled
kernels; invalid declarations cause loading to fail. Additional directories may
be registered using `Context::load_kernels`, followed by `registry.refresh(context)`.
Duplicate names and failed directory loads roll back that directory's additions.

Methods:

| Method | Result |
|---|---|
| `entries()`, `size()` | Full sorted metadata inventory |
| `find(name)` | Exact case-sensitive identifier lookup; nullptr on no match |
| `search(query)` | Matching metadata values |
| `paths(query)` | Matching absolute `.optk` paths |
| `request(name)` | One `KernelFile {kernel, contents}`; error if unknown |
| `request(query)` | Matching `KernelFile` values, empty if no matches |
| `Kernel::source()` | Current file contents; error if missing/unreadable |
| `passes(kernels)` | Comma-separated pass list; rejects empty or mixed-kind groups |

Queries combine keywords, tags, use case, and kind with AND. Each whitespace-
separated keyword must appear literally in the name, description, implementation,
keywords, tags, or uses. Each use-case term must appear in the uses. Every requested
tag must match a complete tag. Matching folds ASCII case, uses no regex, and makes
no semantic applicability guarantee for a particular input program. Ordering is
case-sensitive ascending kernel name. `find` pointers/references into `entries`
remain valid until refresh or registry destruction; returned values own their data.
Const registry searches are thread-safe. Synchronize refresh with readers.

File contents are read only when requested and reflect current disk contents.
Refreshing rebuilds metadata from the context's already loaded declarations; it
does not reexecute changed files. Create a fresh context to reload changed kernels.
Register custom kernels on the context that will execute the selected passes.

## Custom metadata

Metadata is derived for existing kernels from their actual DSL kind, name, pattern,
and analysis configuration. A custom kernel can add explicit search metadata:

```lua
local K = lkopt.optimizer.new "ProjectAddZero"
K:rewrite { pattern = "add($x,0)", replace = "$x" }
K:metadata {
  description = "Remove redundant additions in widget expressions",
  keywords = { "widget", "identity" },
  tags = { "project", "arithmetic" },
  uses = { "simplify widget expressions" }
}
```

`description` overrides the derived description; the three lists extend derived
metadata. Lists must be dense arrays of nonblank single-line strings without NUL.
Unknown metadata keys are errors. Each `.optk` must register exactly one kernel;
filename and registered name may differ. Loading uses the existing restricted Lua
environment and validates the actual declaration. The loader is intended for
trusted local kernels, not as a resource-limited execution service.

The CLI offers equivalent discovery through `kernels --search`, repeated `--tag`,
`--use`, and `--kind`. Output can be the default listing, `--json`, `--paths`, or
`--source`; the last three are mutually exclusive. `--kernel-dir DIR` adds a custom
directory to `kernels`, `optimize`, or `analyze`. An unmatched search succeeds with
zero results. CLI exits are 0 for success, 1 for command/input/I/O/runtime errors,
and 2 for native runtime initialization or invalid native entry arguments.
