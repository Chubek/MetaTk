# Backend algorithms and QScript

The original MetaChain facade remains header-only. The new algorithm entry
points are compiled extensions:

```cmake
target_link_libraries(my_backend PRIVATE MetaChain::Backend)
target_link_libraries(my_language PRIVATE MetaChain::QScript)
```

Both are built by default. Disable them independently with
`METACHAIN_BUILD_BACKEND=OFF` and `METACHAIN_BUILD_QSCRIPT=OFF`.
No system SAT/SMT library is needed. SAT encodings use the bundled Satie CDCL
solver through `satie/BackendSAT.hpp`; its older DSLtk is namespace-isolated.

## Selection (`metachain/ISel.hpp`)

`backend::Graph` has dense node IDs, opcodes, ordered input IDs and designated
outputs. Rules are recursive structural patterns. A `*` pattern is an operand
boundary, not a consumed node. Costs are nonnegative integers.

```cpp
metachain::backend::Graph graph{
    {{"const", {}, 6, false}, {"const", {}, 7, false},
     {"mul", {0, 1}, 0, false}}, {2}};
std::vector<metachain::isel::Rule> rules{
    {"constant", {"const", {}}, 1},
    {"multiply", {"mul", {{"*", {}}, {"*", {}}}}, 3}};
auto selected = metachain::isel::dag_cover(graph, rules);
```

- `tree_cover` and `burs` use bottom-up dynamic cost labeling and top-down
  reduction. This BURS variant has one value class, not a generated
  multi-nonterminal tree automaton. Shared operands are rejected.
- `dag_cover` and `graph_cover` use bounded branch-and-bound exact cover. Shared
  nodes are charged once. Cyclic graphs must first be split into DAG regions.
- `macro_cover` tries low-cost, large macros first, backtracking to complete
  a cover. It does not claim optimality.
- `smt_cover` encodes exact cover and bounded weighted cost in Satie's SAT
  fragment. The unary cost expansion is limited to 4096 literals; SAT formulas
  have at most 200000 variables and 1000000 clauses; each solve has a
  100000-conflict budget.

Internal matched nodes cannot have users outside the tile, be separately
observable outputs, or carry effects. Results are dependency-ordered tiles,
not target machine bytes. Structural patterns do not test immediates or types.
Unmatched nodes, malformed graphs and exhausted searches report diagnostics.
`optimal` is true only when an optimum has been proved; a bounded heuristic
may return a valid cover with `optimal == false`.

## Scheduling (`metachain/ISched.hpp`)

Tasks have predecessor/earliest-issue delays, result latency and a named resource.
The machine specifies issue width and optional per-cycle resource capacities.
Resources have **unit issue occupancy**, not latency-long reservations. Delays
are explicit: use a producer's latency for a normal value dependency.

- `list_based`: critical-path priority; `cycle_driven`: stable ID priority.
- `static_schedule`: serial, stable topological baseline.
- `constraint`: minimum-makespan SAT schedule, limited to 32 tasks and 128
  completion cycles. `valid` independently checks schedules.
- `speculative`: relaxes only caller-designated control edges on explicitly
  speculatable tasks. Never designate a data dependency as a control edge.
- `trace_schedule`: profile-weighted trace formation (no code motion,
  compensation code, or tail duplication).
- `delay_slots`: fills pre-reserved nop slots with safe earlier instructions.
  Its input is a straight-line target-machine region with no interior branch
  targets. Access/barrier/trap metadata is supplied by the target. It is **not**
  a MetaIL transform: MetaIL has no architectural delay slots.

`dependencies` builds conservative RAW/WAR/WAW dependencies for a straight-line
MetaIL block, preserving effects and traps. QScript scheduling never moves
instructions across block boundaries and retains large blocks in input order.

## Allocation (`metachain/Regalloc.hpp`)

`InterferenceGraph` holds symmetric edges, optional fixed colors, spill costs,
and move preferences. `graph_coloring` simplifies low-degree nodes and selects
colors optimistically. `briggs` additionally conservatively coalesces
non-precolored move pairs. `linear_scan` uses half-open intervals and spills
the active interval ending furthest in the future. IDs may be sparse but must
be unique and less than 1000000. Intervals are whole convex live ranges, not
split intervals or lifetime holes.

Uncolored values receive abstract spill slots. **No allocator inserts stack
loads/stores or rewrites a program**; that target-dependent operation belongs
to the caller. `valid` checks a graph allocation. `liveness` derives conservative
live-in/out sets, convex intervals and interference from verified MetaIL,
including loop edges and opaque call/return effects.

## IR (`graph-ir/GraphIR.hpp`, `linear-ir/LinearIR.hpp`)

- `cfg` builds basic blocks with fallthrough, branch and conservative
  call/return edges; `dominators` handles unreachable components separately.
- `dfg` retains RAW/WAR/WAW and ordered effects, including loop-carried self
  edges. It is a dependence graph, not an executable expression DAG.
- `gssa` uses explicit per-register block arguments/phi nodes, even for a
  single predecessor. Initial registers are zero. Call/return are unsupported
  in SSA and produce diagnostics rather than guessing an interprocedural ABI.
- `value_graph` exposes SSA value dependencies; `vsdg` additionally threads
  block state tokens; `sea_of_nodes` additionally exposes control regions.
  These are explicit graph representations, not optimizing regional solvers.
- `pdg` adds postdominator-derived control dependencies. Every block must have
  a path to exit. `pdw` returns undirected dependence-web components.
- `dag` validates acyclicity. `superblocks` forms single-entry traces without
  tail duplication, so it never absorbs a side entrance.
- Typed/untyped three-address code, quadruples and destination-bearing triples
  share a lossless MetaIL adapter. Types are `I64`, `Label` or `None`; there is
  no floating-point or aggregate type system. `pseudo_assembly` emits canonical
  textual MetaIL, not native assembly.

## QScript (`qscript/QScript.h`)

QScript is a signed 64-bit integer scripting language with functions,
recursion, compile-time constants and libraries, parsed by a Raku-style
scannerless grammar on ParseTk and compiled to MetaIL. It has its own manual:
**[manual/qscript](qscript/README.md)**.

```qscript
use math;
fn fact(n) { return n < 2 ? 1 : n * fact(n - 1); }
for (my i = 1; i <= 5; i++) say fact(i) if i % 2;
return gcd(fact(5), 84);
```

| Topic | Chapter |
|-------|---------|
| Syntax, operators, statements, traps | [The language](qscript/02-language.md) |
| `fn`, `const`, scoping, host calls | [Functions and constants](qscript/03-functions.md) |
| `use`, library search, modules | [Libraries and modules](qscript/04-libraries.md) |
| The grammar engine (`QScript-Grammar.h`) | [Grammars](qscript/05-grammars.md) |
| Every C/C++ entry point, including `QScript-Backend.h` and `QScript-Module.h` | [API](qscript/06-api.md) |
| The 25 embedded libraries | [Standard library](qscript/07-stdlib.md) |
| `qscript` and `qscript-repl` | [Command line](qscript/08-cli.md) |
| Pipeline, calling convention, limits | [Internals](qscript/09-internals.md) |

```sh
build/qscript run program.qs
build/qscript build program.qs -o program
build/qscript --repl
```

## Analysis helpers

Every layer ships dumping and statistics for tooling (all header-inline):

- `backend::dump/to_dot/stats/live_from_outputs/find_cycle`
- `isel::pattern_size/depth/operands`, `dump_rule/pattern/tile/result`,
  `index_rules`, `verify_cover`
- `isched::schedule_stats/dump_schedule/critical_path`
- `regalloc::alloc_stats/dump_allocation`
- `graph::dump_cfg/dump_analysis`, `linear::dump_operand/dump_code`

QScript's `native`, `object`, and `build` reuse MetaChain's Linux x86-64
lowering, assembler and static ELF linker, with their existing platform
restrictions.

## Vendored support files

The requested Doxygen/pkg-config templates and SWIG interface/generator are
populated, but this does not enable or repair unrelated vendored build systems.
The `moosepaw.pc.in` template describes an optional Moosepaw runtime; that
runtime is not supplied by this source snapshot. `.gitmodules` documents that
dependencies are source-vendored, rather than inventing unverified submodule
URLs. `.nojekyll` remains a marker (its contents are ignored by Pages), and
xxHash's `.tipi/opts` is an empty JSON options object.
