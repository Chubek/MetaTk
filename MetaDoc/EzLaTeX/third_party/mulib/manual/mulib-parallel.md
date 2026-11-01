# MuLIB parallel

**Header/switch:** `include/mulib-parallel.h` / `MULIB_IMPL_PARALLEL`  
**Required implementation:** `MULIB_IMPL_TASKPOOL` (not auto-defined)  
**Build:** C11 implementation, C++17 declarations, POSIX pthread pool and `-pthread`.

## Parallel for

```c
int mulib_parallel_for(mulib_taskpool_t *pool,
    size_t begin, size_t end, size_t grain,
    mulib_parallel_fn fn, void *context,
    const mulib_task_cancel_t *cancel, int *error);
```

Calls `fn(index, context)` for each index in **[begin,end)**, partitioned into
chunks of up to `grain` consecutive indices. Chunks run on actual pool workers;
indices inside a chunk run sequentially. Order across chunks is unspecified.
`begin > end`, zero grain, NULL callback/pool, and allocation overflow are invalid.
Empty ranges succeed without allocating, unless cancellation was already requested.
The exclusive upper bound makes `end == SIZE_MAX` representable without loop
overflow. Temporary storage is O(ceil((end-begin)/grain)); choose grain to avoid
one context allocation per element for very large inputs.

The caller coordinates submission and waits until **all submitted work finishes**
before returning. Context and token are borrowed for that duration. Never call
from the supplied pool's worker or cleanup callbacks; use an otherwise
exclusively controlled pool. Queue-full backpressure drains the current submitted
batch through global pool `wait`, so there can be barriers between batches.

## Errors, cancellation and shared data

Callbacks return zero on success, nonzero on failure. An atomic first-error slot
retains one original error (which one is unspecified for simultaneous failures).
An optional `error` output is initialized to zero, and receives that code.
The API returns taskpool's `MULIB_TASK_FAILED` rather than confusing a callback's
application-specific code with its own status. Other returns include `INVALID`,
`NOMEM`, pool errors, and `CANCELLED`; zero is success. Infrastructure errors take
precedence over callback failures, then callback failures over cancellation.

Each worker checks shared failure/cancellation before each iteration. Already
running callbacks finish normally; **partial writes are not rolled back**.
Pending pool jobs cancelled by pool shutdown set cancellation rather than falsely
reporting completion. Do not independently manipulate pool lifecycle during a
parallel operation. Tokens are the supported concurrent cancellation mechanism.
Callbacks must return normally; shared context mutation requires synchronization.
Disjoint output slots do not require locks. The function never frees application
buffers or user contexts, even on failure.

## Fixed-stride map

`mulib_parallel_map(pool, input, input_stride, output, output_stride, count,
grain, fn, context, token, error)` is built on parallel for. It calls
`fn(index, input_slot, output_slot, context)` for fixed-stride slots. Strides must
be positive. Both `count * stride` products are checked for overflow; caller must
provide buffers of those sizes. Empty maps permit NULL buffers. Nonempty maps
reject NULL buffers and partially overlapping ranges. Exact in-place mapping is
allowed **only with identical input/output pointers and equal strides**. A
callback must not retain a slot pointer past the lifetime of the caller's buffer
or access other slots concurrently without application synchronization.

No implicit element constructors/destructors, reductions, task nesting, rollback,
thread-affinity guarantee or SIMD specialization is provided. Map is a byte-slot
adapter, not a container owner. Each callback exclusively owns its output slot.

## Example and validation

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -pthread -Iinclude \
  examples/modules/parallel.c -o /tmp/parallel
/tmp/parallel
```

The runnable example squares four integers using two workers. The focused test
suite also checks a non-divisible chunk count, in-place map, overlap/overflow
rejection, empty input, original callback error preservation, and cancellation
requested from inside a callback.
