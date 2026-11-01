# MuLIB pipeline

**Header/switch:** `include/mulib-pipeline.h` / `MULIB_IMPL_PIPELINE`  
**Required implementations:** `MULIB_IMPL_PARALLEL`, `MULIB_IMPL_TASKPOOL`  
**Build:** C11, `-pthread`; C++17 declarations supported. Dependencies are never auto-enabled.

## Execution model

An item is `{ void *data; size_t size; }`. A stage is a callback/context pair with
signature `int stage(mulib_pipeline_item_t *, void *)`. `mulib_pipeline_run`
executes stages in array order for each item, using parallel-for with grain one
to process **different items concurrently**. A stage can update bytes, resize
the item or replace its buffer. No worker waits for another stage's worker.

This is a focused **item-parallel pipeline**, not a streaming network of
per-stage queues: a single item traverses all its stages on one worker, and
different invocations of a stage can run simultaneously. Stages sharing mutable
context need their own synchronization. The caller must not be a callback on the
same pool, and must exclusively control pool lifecycle during run. Dependencies
use pool-global waits for bounded-queue backpressure.

## Ownership is explicit

```c
int mulib_pipeline_run(mulib_taskpool_t *pool,
    const mulib_pipeline_stage_t *stages, size_t stage_count,
    mulib_pipeline_item_t *items, size_t count,
    mulib_pipeline_release_fn release, void *release_context,
    const mulib_task_cancel_t *cancel, size_t *completed, int *error);
```

1. Validate non-NULL pool/release callback, every stage function, item array, and
   each item (`size > 0` requires data). NULL stages/items are allowed with zero
   respective counts. Counts whose array byte sizes overflow `size_t` are rejected
   before inspecting array elements. Invalid input returns `MULIB_TASK_INVALID` **without taking
   ownership or invoking release**. Output counters are initialized to zero.
2. After validation, the pipeline consumes **every** item, even on allocation,
   submission, callback failure, or pre-requested cancellation. The caller must
   not concurrently access the array or buffers. Item buffers must be separately
   owned; aliases that would cause racing updates/double release are forbidden.
3. A replacing stage owns its current item and must dispose of the **old** buffer
   itself before losing that pointer. On return, even on failure, leave the item
   in a state understood by `release`; do not free a buffer and leave a dangling
   pointer. Non-NULL data with size zero is allowed. A stage returning success
   with NULL data and nonzero size is treated as a callback failure.
4. After all workers stop, `release(item, release_context)` runs **exactly once
   for every item, in input order, on the calling thread**. This includes items
   that never began a stage. It can collect output and free final buffers. The
   library then clears data to NULL and size to zero. Array storage, stage arrays
   and contexts themselves are borrowed, never freed. Release must return normally.

Because release is serialized, a collection callback may accumulate results into
an ordinary caller-owned container without locking. Release is mandatory even
for an empty run, so the ownership contract is unambiguous. Zero stages is an
identity pipeline followed by release; zero items succeeds unless token-cancelled.

## Failure and cancellation

Zero means success. Other returns use taskpool's status enum. A nonzero stage
return stops that item immediately, causes unstarted iterations to stop via
parallel's atomic error flag, and returns `MULIB_TASK_FAILED`. `error` receives one
original stage code. In-flight stages/items may finish; no transactional rollback
is attempted. `completed` counts items that traversed every stage successfully;
it is not the number of released items. Stage failure takes precedence over token
cancellation unless an infrastructure error occurred.

Tokens are checked between iterations and between stages. Running stages are not
interrupted and should inspect an application-provided token if long-running.
Cancellation still disposes of every item. The library cannot recover ownership
from callbacks that throw, longjmp, terminate a worker or never return.

## Runnable example and tests

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -pthread -Iinclude \
  examples/modules/pipeline.c -o /tmp/pipeline
/tmp/pipeline
```

The example doubles two separately allocated integers and collects their sum in
the release callback. Tests exercise buffer replacement, final release on the
caller thread, original error preservation, cancellation before any work,
validation failure retaining ownership, empty inputs, and cleared output handles.
