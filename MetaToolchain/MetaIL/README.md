# MetaIL / Optikern

MetaIL is a **scaffolded compiler IR and text language** driven by executable **Optikern Lua 5.4 analysis and rewriting kernels**. The upload contained only four empty files and one kernel declaration; this tree fills those files and adds a C host, parser, AST, CFG analyses, transformations, CLI, tests, examples, documentation, and generated kernel definitions.

**Kernel inventory:** 321 `.optk` files: the original `ConstantFolding.optk`, 200 base kernels, and **120 numeric kernels** named `optimize-kernels/Numeric*.optk`. Totals: 84 analyses, 230 expression rewrites, and 7 procedural optimizations. The numeric additions comprise 96 signed-64 rewrites and 24 analyses backed by MetaNum's executable statistics APIs. `metail verify` checks every rewrite for boundary-value, effect, and trap equivalence and requires every rule to fire.

## Build and test

Install **Lua 5.4** (real upstream Lua runtime, as a shared library), C11 and C++20 toolchains, CMake, and Python 3 for tests. MetaNum headers default to `../../MetaNum`; override with `-DMETANUM_ROOT=/path/to/MetaNum`. DSLtk defaults to `../../DSLtk` (the workspace's `/mnt/nvme/metatk/DSLtk/DSLtk.hpp`); override with `-DDSLTK_ROOT=/path/to/DSLtk`. The archive deliberately **does not vendor Lua itself**. On Debian/Ubuntu, `liblua5.4-dev` supplies the required shared library.

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/metail selftest
./build/metail verify
python3 tools/generate_kernels.py --check
```

The `lua/lua_compat.h` file contains only the subset of the public Lua 5.4 ABI declarations needed by this host; it is not a reimplementation of the Lua interpreter or a vendored third-party header. The host links the actual Lua library. For ordinary installations, `cmake --install build` installs runtime files to `<prefix>/share/metail` and the CLI under `<prefix>/bin`.

## Use

```sh
./build/metail check examples/arithmetic.mi
./build/metail emit examples/arithmetic.mi
./build/metail kernels
./build/metail kernels --json
./build/metail optimize examples/arithmetic.mi --out optimized.mi --stats changes.json
./build/metail analyze examples/dataflow.mi --passes ControlFlowGraph,Dominators,LiveVariables,ReachingDefinitions
./build/metail analyze examples/dataflow.mi --passes all --out analysis.json
./build/metail optimize examples/arithmetic.mi --passes ConstantFolding,MultiplyByPowerOfTwoRight03
```

By default, `optimize` runs all transformations in up to two rounds, with a final constant-fold, branch, dead-assignment and unreachable-block cleanup in each round. `analyze` runs all analyzers or selected analyzers, outputting stable JSON. The interpreter looks for `METAIL_HOME`, the source directory, an installed share directory, or the configured build source root.

## C/C++ API and kernel discovery

`MetaIL.h` exposes structured expression, statement, function, and module descriptors,
direct IR validation/emission/optimization/analysis, textual parsing, and kernel metadata.
`MetaIL.hpp` supplies a C++20 DSL backed by **DSLtk AST nodes**, editable `Module` and
`Function` values, and the move-only `metail::Context` runtime wrapper:

```cpp
#include <MetaIL.hpp>
metail::Context context;
auto x = metail::var("x");
metail::Module module("demo");
module.function("scale", {"x"}).ret((x + 0) * 8);
auto optimized = context.optimize(module); // Structured IR in and out; no text parser.
auto text = context.emit(optimized.module);
auto imported = context.parse(text);       // Text and native DSL interoperate.
```

The native DSL also supports const DSLtk visitors, simultaneous variable
substitution, and checked DSLtk rule sets through `metail::rewrite` and
`metail::rewrite_expressions`. Rewrites validate intermediate results, enforce
pass/application budgets, and return independent values with change counts.
See [DSLtk integration](manual/22-dsltk-integration.md) for the API and semantic
requirements on custom rules.

The [40-chapter manual](manual/README.md) covers all three interfaces, ownership,
semantics, analyses, kernels, deployment, and complete examples.
`KernelRegistry.hpp` offers validated `.optk` declarations
by keywords, tags, and use case, returning paths or reading file contents on demand:

```cpp
#include <KernelRegistry.hpp>

metail::Context context; // Or pass an explicit runtime root.
metail::KernelRegistry registry(context);
metail::KernelQuery query;
query.keywords = "multiply power";
query.tags = {"strength-reduction"};
query.use = "shifts";
query.kind = "rewrite";

auto files = registry.request(query); // Kernel metadata plus .optk contents.
auto passes = registry.passes(registry.search(query));
auto result = context.optimize("fn scale(x) { return x * 8; }", passes);
// result.source contains the rewritten MetaIL; result.stats contains JSON.
```

Search combines all filters with AND and returns results sorted by kernel name.
Keywords/use-case terms are case-insensitive literal substrings; tags match exactly
(case-insensitive). Empty queries return all kernels; unmatched queries return an
empty list. Registries own their metadata and can outlive their originating context.
Built-in tags include `analysis`, `rewrite`, `builtin`, `signed-64`, `scalar`,
`numeric`, `statistics`, `metanum`, `strength-reduction`, `control-flow`, and `dataflow`.

```sh
build/metail kernels --search 'multiply power' --tag strength-reduction --use shifts --paths
build/metail kernels --tag numeric --tag statistics --kind analysis --json
build/metail kernels --search AddZeroRight --source
build/metail_example_controlflow
build/metail_example_gcd
build/metail_example_kernels
```

CMake consumers use `add_subdirectory(...)` or, after installation,
`find_package(MetaIL CONFIG REQUIRED)` and link `MetaIL::MetaIL`. The static library
includes the C++ MetaNum binding, so enable CXX in CMake consumers (even when their
application source is C), or use a C++ linker when linking manually. Runtime Lua
files and kernels must remain available. See [docs/API.md](docs/API.md) for ownership,
custom metadata, installation, and a C example. The CLI contract is recorded in
[`cli/MetaIL-CLI.yaml`](cli/MetaIL-CLI.yaml).

## MetaIL syntax

```text
module sample;
fn main(x) {
entry:
  let y = ((x + 0) * 8);
  br y == 0, zero, nonzero;
zero:
  return 0;
nonzero:
  return (y >> 1);
}
```

All values are presently **signed 64-bit integers**. Addition, subtraction, negation, multiplication, and bitwise operators use two's-complement wraparound. Signed division and remainder truncate toward zero; division by zero and signed minimum divided by -1 cannot be constant-folded. Shifts with counts outside 0..63 are not folded. Boolean results are integer 0 or 1; `select(condition,yes,no)` has conditional (lazy) arm evaluation. Calls such as `tick()` are conservatively impure.

Statements: `let`, `return`, `eval`, `assume`, `goto`, `br`, and basic-block labels. Expressions: integer literals, local variables, unary `-`, `~`, `!`, infix arithmetic/bitwise/comparisons/logical operators, `select`, and function calls. Every operator may also be written in prefix form: `add(x, 7)`, `band(x, 255)`, `select(c, a, b)`, etc. Refer to [docs/Language.md](docs/Language.md).

## Optikern DSL

Each `.optk` file runs in a restricted Lua environment with `lkopt`, `math`, `string`, `table`, and elementary conversion functions. A rule may capture metavariables, optionally require constant or pure expressions, and emit replacements:

```lua
local K = lkopt.optimizer.new "AddZeroRight"
K:rewrite {
  pattern = "add($x,0)",
  replace = "$x"
}
```

Other kinds:

```lua
local CFG = lkopt.optimizer.new "ControlFlowGraph"
CFG:analysis { kind = "cfg" }

local CP = lkopt.optimizer.new "ConstantPropagation"
CP:builtin "constant-propagation"
```

Guards consist of clauses such as `"pure:x,y;const:c;nonzero:d"`. A guard is needed when a rewrite could discard side effects or change the number/order of evaluations. Details and the registered analyses are in [docs/Optikern.md](docs/Optikern.md).

## MetaNum / Lua binding

Run Lua scripts with `build/metail lua script.lua`. Both `require('metanum')` and
`require('lkopt').numeric` expose the same API:

```lua
local num = require('metanum')
print(num.mean({1, 2, 3, 4}))                  -- 2.5
print(num.variance({1, 2, 3, 4}, false))       -- 1.25 (population)
print(num.quantile({1, 2, 3, 4}, 0.25))        -- 1.75
print(num.correlation({1, 2, 3}, {6, 4, 2}))   -- -1
```

`cmake --install build` also installs `metanum_native` for a standard Lua 5.4
interpreter and the `metanum.lua` facade. No MetaNum runtime library is needed.
The 40 GSL/ISL/PPL categories and 298 operations are exposed by `num.catalog()`
as **metadata**, with `callable=false`. See [docs/MetaNum.md](docs/MetaNum.md) for
the full API, input/error contract, numeric kernel inventory, and examples.

```sh
build/metail analyze examples/arithmetic.mi --passes NumericLiteralsMean,NumericExpressionSizesQuantile90
build/metail optimize examples/arithmetic.mi --passes NumericMultiplyMinusOneRight
```

## Current scope and limits

This is a **working, deliberately bounded foundation**, not a complete backend compiler or a full language runtime. It does not yet perform SSA construction, type inference, memory SSA, global interprocedural optimization, native code emission, exceptions, vectorization, alias analysis, or sophisticated lattice-based range analysis. Some optimizations are intrablock only. Rewrites are exact structural patterns with simple guards, not an SMT prover. Calls are modeled as potentially effectful and therefore block unsafe eliminations. Some analyses (e.g. literal range) report syntactic facts rather than proving ranges for arbitrary expressions.

The `tools/generate_kernels.py` script reproducibly regenerates the 320 added kernel definitions, preserves `ConstantFolding.optk`, and rejects unexpected files without deleting them. See [AGENTS.md](AGENTS.md) for contributor integration points and quality requirements.

Build configurations, regression coverage, and results are recorded in [docs/Verification.md](docs/Verification.md).
