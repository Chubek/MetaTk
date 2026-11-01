# mulib-rewrite

Bounded e-matching and equality saturation on `mulib-egraph`, with real
backtracking, repeated-variable constraints, RHS construction and congruence
rebuilds. No predefined algebra or semantic soundness is assumed.

## Integration

Include `mulib-rewrite.h`. Explicitly define `MULIB_IMPL_EGRAPH` and
`MULIB_IMPL_REWRITE` exactly once. No dependency implementation is silently
enabled. C11, C++ C-linkage declarations, no third-party dependencies.

## Exact rule representation

A rule borrows LHS and RHS pattern arrays and their counts. Each array is a
postorder **tree** of 1 through `MULIB_REWRITE_PATTERN_LIMIT` (64) records, with
root at the final index. Every used child ID is earlier than its parent. Each
nonroot record must have exactly one parent: no sharing, cycles, unreachable
records or forward references are accepted. A repeated variable is represented
by separate VARIABLE records with the same variable number.

NODE records match `opcode`, `literal`, and arity exactly. Children match
eclasses, not merely their first enode. VARIABLE records have arity zero and
bind any class; their numbers are below `MULIB_REWRITE_VARIABLE_LIMIT` (32).
Unused children/VARIABLE opcode and literal are ignored. RHS variables must
occur on LHS; unbound output variables are invalid. No guards, predicates,
commutative matching, associative flattening, variadic nodes, type inference,
or variable-dependent literal computations are supported. Express commutation
as an explicit equality rule if sound and desired.

All rules are validated before any graph mutation. Rule arrays remain borrowed
and immutable only for the synchronous call. There is no compiled-rule owner.

## Application and saturation

Call `mulib_rewrite_run(g,rules,rule_count,limits,&result)`. NULL result is legal.
The graph can initially be dirty; the API rebuilds it using the same work
budget. Each iteration visits canonical classes and every rule, enumerating
matching alternative enodes with backtracking. Bindings are canonicalized for
repeated-variable comparisons. It explores nested alternatives rather than
committing to a first child and incorrectly missing later compatible bindings.

For every match, the RHS is instantiated in postorder by hash-consing nodes
and substituting bound classes. Its root is unioned with the matched class,
then congruence is rebuilt before any further add. Existing matching nodes are
reused; no extra capacity is charged for duplicate keys. Newly added terms are
eligible in following iterations; same-iteration effects can also be observed.
Return OK only after a **complete iteration** changes neither node count nor
class equalities. An empty rule set returns OK after any required initial rebuild.
There is no guarantee of reaching saturation for an expanding rule system.

Graph mutation is monotonic and **not transactional**. Limit/error exits retain
all previously added nodes/unions, possibly a partial RHS. A failed rebuild
can leave the graph dirty; an explicit later rebuild restores add/extraction.
Rules with invalid syntax never mutate it. A caller wanting rollback should
operate on a separately constructed graph; no graph clone API is provided.

## Explicit limits and diagnostics

`mulib_rewrite_limits_t` contains four independent hard bounds:

* `rules`: maximum supplied rule count (RULE_LIMIT on excess).
* `iterations`: completed/attempted saturation passes. Zero forbids a pass;
  ITERATION_LIMIT means saturation has not been proved, even if the last
  allowed changing pass happened to reach a fixed point.
* `matches`: maximum successful matches admitted for application. Identity
  matches count too, even when they do not change the graph (MATCH_LIMIT).
* `work`: shared matching/RHS/rebuild visit budget (WORK_LIMIT). Matching task
  visits, candidate enode scans, RHS record visits and rebuild enode visits
  consume it. Zero is zero; it is not a default/unlimited sentinel.

The e-graph's own creation capacity independently bounds total terms/classes.
Capacity or other underlying graph errors return EGRAPH_ERROR, with exact
`result.graph_status`. Syntax/descriptor errors return INVALID.
`mulib_rewrite_status_string` returns static explanatory text.

The result also reports `iterations`, `matches`, root-union `applications`,
`work`, and the last/current `rule` and `class_id`. Congruence-induced unions do
not count as applications. Location fields are diagnostic context, not a full
proof trace. Inspect them only when relevant to a failure. No node/rule owns
extra strings, and no error globals are used.

Tree validation uses fixed arrays; matching recursion is bounded by 64 pattern
records (not arbitrary graph depth), with fixed-size task/binding states. RHS
construction uses a fixed 64-entry value table. No rewrite heap allocations
occur beyond the preallocated e-graph; rebuild allocates nothing. E-matching
can be combinatorial, so work and match bounds are mandatory. These counts do
not meter hash collisions, find traversal, rule validation or user wall time;
those are structurally bounded by pattern/rule/graph capacities.

## Semantic responsibility and MIR composition

The rule `x+0=x` is a useful example for signed checked integer expressions
whose x subexpression is still evaluated. The rule `x*0=0` is **not generally
fault preserving** when x could fault; neither is checked-arithmetic
reassociation. Types, effects, faults and evaluation count are outside this
untyped equality container. Apply only proven-valid rules for your language.

Extraction is a separate call to `mulib_egraph_extract`. It yields a postorder
tree that clients can map to common MIR records, supply types, validate, and
execute through CMACHINE/MINIVM. The compiler IR tests perform exactly that
path and assert the returned result after a real rewrite and extraction.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/rewrite.c -o /tmp/rewrite
/tmp/rewrite
```

The example encodes `42+0`, saturates with `x+0=x`, and extracts the one-node
constant 42. Tests cover invalid/unbound/cyclic patterns, repeated variables,
iteration/match/work/graph limits and executable MIR extraction.
