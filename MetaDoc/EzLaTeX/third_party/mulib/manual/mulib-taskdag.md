# MuLIB taskdag

**Header/switch:** `include/mulib-taskdag.h` / `MULIB_IMPL_TASKDAG`  
**Transitive implementation switches:** `MULIB_IMPL_TASKPOOL`  
**Build:** C11, `-pthread`; C++17 declaration-only inclusion supported.

## Model and runnable example

A bounded directed graph associates an `int (*)(void *)` callback with each node.
An edge `before -> after` means the former must **succeed** before the latter is
eligible. Independent nodes in a dependency frontier execute on the supplied
worker pool. The calling thread is the coordinator, never a worker that waits
on other pool jobs. No dependency implementation is enabled implicitly.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -pthread -Iinclude \
  examples/modules/taskdag.c -o /tmp/taskdag
/tmp/taskdag
```

The example runs a producer and consumer over one shared integer with a real
dependency; the consumer asserts the producer's completed value.

## Graph construction and validation

`mulib_taskdag_create(&dag, node_capacity, edge_capacity)` allocates fixed arrays.
Node capacity must be positive; zero edge capacity is valid. Allocation products
are overflow-checked. `mulib_taskdag_add(dag, fn, context, &id)` borrows a non-NULL
callback and arbitrary context, returning a stable zero-based ID. `depend`
requires existing IDs, ignores duplicate edges, and returns `FULL` if the edge
capacity is exhausted. A self-edge is recorded and diagnosed as a cycle, rather
than silently ignored. Nodes/edges cannot be removed.

`mulib_taskdag_order(dag, order, capacity)` writes a topological order using
Kahn's algorithm. Capacity must cover `mulib_taskdag_count(dag)`; NULL order is
allowed only for an empty graph. `CYCLE` means an incomplete prefix may have been
written. `run` performs this validation **before any callback executes**. The
graph is bounded by caller capacities, not by a global maximum. Linear edge scans
favor small/medium dependency graphs over massive build graphs: order is
O(V*E + V), with O(V) temporary storage. Frontier execution may scan nodes and
edges repeatedly; dense/long graphs can approach O(V²*E). Storage is O(V+E).

## Execution, results and cancellation

`mulib_taskdag_run(dag, pool, optional_token)` resets prior node results and runs
the graph. An empty graph succeeds. It borrows the pool and token. **Call from
outside that pool and use an otherwise exclusively controlled pool**: queue-full
backpressure and frontier barriers use the pool's global `wait`. Do not perform
concurrent graph mutation, result queries, run, or destroy. Separate graphs may
use separate pools concurrently.

Callbacks return zero on success, any other integer on failure. After run,
`mulib_taskdag_result(dag, id, optional_state, optional_result)` returns:

| State | Meaning | Stored callback result |
|---|---|---|
| `PENDING` | Not executed (including cycle-validation failure) | 0 |
| `RUNNING` | Internal in-progress state; do not query concurrently | 0 |
| `SUCCEEDED` | Callback returned zero | 0 |
| `FAILED` | Callback returned nonzero | Original code |
| `BLOCKED` | A prerequisite failed/was skipped | 0 |
| `CANCELLED` | Token prevented execution or queue rejected/dropped the job | 0 |

The integer return from `run` is `MULIB_TASK_OK`, `FAILED`, `CANCELLED`, `CYCLE`,
or an allocation/pool/argument error from taskpool's status enum. If multiple
causes occur, the first observed nonzero status is retained; inspect node results
for detail. Failure blocks descendants but **does not stop independent branches**.
Callbacks own cleanup of resources they allocate, including partial failures.

Cancellation is checked before submission eligibility and immediately before
callback invocation. Running callbacks finish normally and may inspect their
own borrowed token for faster cooperation. There is no rollback or preemption;
a token requested after all work has begun need not change successful results.
Subsequent runs are allowed; contexts must themselves support repeated calls.
`mulib_taskdag_destroy` frees graph storage, not callback contexts, and accepts NULL.

## Limits and tests

This is a **frontier/barrier scheduler**, not an optimal dynamic work-stealing
DAG executor: a newly ready descendant waits for all jobs in the current frontier.
It cannot be nested inside callbacks on the same pool. No futures or deadlines
are implied. Tests cover success, failed-prerequisite blocking, an independent
successful branch, repeated runs, duplicates, capacity errors, pre-cancellation,
self-cycles and multi-node cycles with no callback side effects.
