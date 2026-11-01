# mulib-isched

Dependency-aware, single-issue, latency-aware list scheduling of a bounded DAG.
This is not a machine-specific pipeline simulator, register allocator, or an
optimal makespan solver.

## Integration and data

Include `mulib-isched.h`; define `MULIB_IMPL_ISCHED` exactly once. There are no
other implementation dependencies. C11, with C-linkage declarations for C++.

Input nodes have positive `size_t latency`. An edge `{before,after}` means:

```
start[after] >= start[before] + nodes[before].latency
```

Node IDs are array indices. Inputs are borrowed for the synchronous call and
never mutated or retained. Duplicate edges are legal and do not change the
constraint. An empty DAG is valid; NULL arrays require zero corresponding
counts. A zero latency, out-of-range edge, or self/cyclic dependency is rejected.
There is no implicit dependency based on array order or instruction names.

`mulib_isched_run(nodes, count, edges, edge_count, limits, &schedule, diagnostic)`
requires an empty, zero-initialized `mulib_isched_schedule_t`. Success owns two
arrays: `order[issue_position]` is a permutation of node IDs and `start[node_id]`
is that node's issue cycle. `makespan` is the largest completion cycle. Empty
output has count/makespan zero and NULL arrays. Release with `mulib_isched_free`,
which accepts NULL and zeroes the object. Do not copy a live owner. Failure
leaves output empty and releases all temporary arrays.

## Algorithm and scheduling model

First, a deterministic topological pass checks the **entire** graph, including
unreachable components. Reverse topological dynamic programming computes the
longest latency-weighted remaining dependency path. Among ready nodes, the
scheduler chooses the largest such priority; ties choose the smallest ID.

One node can issue each cycle. Previously issued operations can overlap; a
dependent operation waits for all predecessor completions. If no node is ready
at the current issue cycle, the scheduler jumps directly to the earliest ready
time rather than iterating through idle cycles. There are no functional-unit
classes, reservation tables, issue bundles, memory-disambiguation rules, or
target-specific hazards beyond supplied edges.

To schedule MIR, construct edges for every value use and assign target latencies.
Also add ordering edges for faults/effects that must retain order. Reordering
independent checked arithmetic without such edges can change which runtime
fault occurs first. After scheduling, clients must remap SSA value IDs and emit
in `order`, not sort by original ID. The scheduler does not mutate typed MIR or
assert that arbitrary input instructions commute.

## Limits and diagnostics

Pass `mulib_isched_limits_t {max_nodes,max_edges,max_cycles}` by value. All limits
are exact, including zero. Node/edge counts and array-byte multiplications are
checked before allocation. The cycle limit bounds the **completion** makespan,
not merely the last issue. `SIZE_MAX` can be a valid single-node completion
cycle; arithmetic overflow beyond it returns OVERFLOW, not wraparound.

Statuses distinguish INVALID, CYCLE, RESOURCE_LIMIT, OVERFLOW, NO_MEMORY and OK.
`diagnostic.node` identifies the offending node; for an invalid edge it identifies
the edge index; for cycles it is an unscheduled member of the blocked graph (not
necessarily the only cycle). Ignore location on success or descriptor failures.
`mulib_isched_status_string` supplies static explanatory text.

Memory is O(nodes): six `size_t` arrays (four temporary, two returned), plus one
byte per node. Allocation uses malloc/calloc/free and is released on all errors.
The straightforward scanning algorithm uses O(nodes² + nodes*edges) time. Limits
bound counts, not individual CPU operations; callers choosing very large DAG
limits accept that complexity. No recursion or per-cycle idle loops are used.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/isched.c -o /tmp/isched
/tmp/isched
```

The example asserts overlapping independent operations, a four-cycle dependency
wait, and cycle rejection. Compiler IR tests also cover impossible edge IDs,
zero latency, makespan limits, overflow, empty graphs, and actual MIR reordering
with SSA remapping followed by equivalent MINIVM execution.
