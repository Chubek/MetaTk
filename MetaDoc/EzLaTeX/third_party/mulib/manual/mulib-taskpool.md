# MuLIB taskpool

**Header:** `include/mulib-taskpool.h`  
**Implementation:** `MULIB_IMPL_TASKPOOL`, in exactly one C11 translation unit  
**Dependencies:** C library, C11 atomics, POSIX pthreads; link **`-pthread`**.

## Scope and build

This is a real fixed-size worker pool, not sequential dispatch. A mutex-protected
FIFO ring holds a bounded number of **pending** callbacks; up to `workers`
additional callbacks can be running. Workers sleep on a condition variable when
idle. Creation starts every requested worker or cleans up and reports failure.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -pthread -Iinclude \
  examples/modules/taskpool.c -o /tmp/taskpool
/tmp/taskpool
```

Declarations have C++ linkage guards and can be included from C++17. Compile the
implementation as C11. POSIX Unix/Apple hosts use pthreads. Other platforms, or
builds with `MULIB_TASKPOOL_NO_PTHREAD`, return `MULIB_TASK_UNSUPPORTED` from pool
operations: there is no silent sequential fallback. Cancellation tokens remain
available on those platforms. No other implementation switch is auto-defined.

## API and status codes

* `mulib_taskpool_create(&pool, workers, capacity)` requires nonzero counts and a
  writable output pointer. Checks allocation-size overflow; allocates fixed
  storage and threads. On a valid create attempt, failure sets the output NULL.
* `mulib_taskpool_submit(pool, fn, context, on_cancel)` is **nonblocking**. `fn`
  must be non-NULL; context may be NULL. Returns `FULL` if all pending slots are
  occupied or `CLOSED` after shutdown. These failures do not transfer ownership.
* `mulib_taskpool_wait(pool)` waits for zero pending and zero active callbacks,
  including cancellation cleanup. It does not close the queue. A simultaneous
  producer can submit again after the observation of idleness.
* `mulib_taskpool_shutdown(pool, cancel_pending)` rejects future submissions.
  With zero, workers drain the queue; otherwise pending jobs are cancelled and
  their optional cleanup callbacks execute synchronously in the shutdown caller.
  Running callbacks are never forcibly interrupted. Shutdown is idempotent and
  a draining shutdown can subsequently be escalated to pending cancellation.
* `mulib_taskpool_destroy(pool, cancel_pending)` shuts down, joins **every** worker
  and frees storage. NULL is harmless. A callback that never returns will prevent
  teardown: the pool cannot safely reclaim a running callback's state.

All task runtime operations use `MULIB_TASK_OK == 0`; negative statuses are
`INVALID` (bad arguments/overflow), `NOMEM`, `UNSUPPORTED`, `FULL`, `CLOSED`,
`CANCELLED`, `CYCLE`, `FAILED`, and `SYSTEM` (pthread initialization/creation
failure). Pool callbacks return void; higher-level modules collect callback
errors themselves. pthread error numbers are not exposed as `errno`.

## Ownership and concurrency

On successful submission, exactly one of `fn(context)` and
`on_cancel(context)` is selected. On the successful-execution path, the cleanup
callback is **not** called: `fn` is responsible for releasing owned resources.
If cancellation cleanup is NULL, cancellation simply drops the borrowed context.
The library never frees a context itself. Callback arguments must remain valid
until execution or cleanup has finished; `wait`/destroy provide that boundary.
Callbacks must return normally, not call `pthread_exit`, throw, or longjmp out.

Multiple threads may submit and wait. One external controller must serialize
shutdown/destroy; destroy requires all other API callers to have finished.
Callbacks may submit nonblocking work, handling FULL/CLOSED themselves. Same-pool
worker/cleanup calls to wait or shutdown return `INVALID`; destroy is ignored
and the external controller retains ownership. Nested callbacks on other pools
do not bypass these checks. Parallel and DAG execution reject same-pool reentry
before submitting borrowed job storage. No thread cancellation, priorities, work stealing, timed
waits or recursive fork/join are implemented. Callback shared data still needs
application synchronization. Keep all pools quiescent across application `fork`.

## Cooperative tokens

`mulib_task_cancel_create()` allocates a clear token (NULL on allocation failure).
`mulib_task_cancel_request(token)` atomically sets it; requests are irreversible.
`mulib_task_cancel_requested(token)` is thread-safe; NULL means not cancelled.
`mulib_task_cancel_destroy(token)` is safe only after all users finish. Request
and destroy accept NULL. These APIs are thread-safe, **not** async-signal-safe.
Tokens do not cancel queued raw pool jobs automatically: callbacks or the
parallel/DAG/pipeline adapters must inspect them. Pool queue cancellation and
token cancellation are intentionally different mechanisms.

## Validation

`tests/test_tasks_process.c` proves two workers can be simultaneously blocked in
independent callbacks, deterministically fills a one-slot pending queue, checks
cleanup selection and closed rejection, and verifies draining teardown. Allocation
failure injection is not exposed. Counts are checked before allocating; thread
creation failure joins already-started workers before returning.
