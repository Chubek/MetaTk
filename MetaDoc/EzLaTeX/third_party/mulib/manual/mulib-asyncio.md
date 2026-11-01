# MuLIB asyncio

**Header:** `include/mulib-asyncio.h` · **Switch:** `MULIB_IMPL_ASYNCIO`  
**Header dependency:** `mulib-select.h`; separately define `MULIB_IMPL_SELECT`
in one implementation translation unit. No switch is implicitly enabled.
C11, C++ C-linkage declarations; no third-party dependency.

## Exact supported reactor

A single-threaded, fixed-capacity **readiness** reactor plus FIFO callback jobs,
using native POSIX select. It is not kernel AIO/io_uring, a coroutine scheduler,
thread pool, cross-thread wakeup queue, networking protocol implementation, or
automatic file-I/O offloader. Windows is unsupported: creation reports
NULL/`ENOSYS` via select. No threads, global loop, implicit polling, signal
handler, or infinite run function is created.

```c
typedef struct mulib_asyncio mulib_asyncio;
typedef void (*mulib_asyncio_callback)(mulib_asyncio *, int fd,
                                      unsigned events, void *user);
typedef void (*mulib_asyncio_job)(mulib_asyncio *, void *user);
mulib_asyncio *mulib_asyncio_create(size_t watch_capacity, size_t job_capacity);
int mulib_asyncio_destroy(mulib_asyncio *loop);
int mulib_asyncio_watch(mulib_asyncio *loop, int fd, unsigned events,
                        mulib_asyncio_callback callback, void *user, uint64_t *id);
int mulib_asyncio_unwatch(mulib_asyncio *loop, uint64_t id);
int mulib_asyncio_post(mulib_asyncio *loop, mulib_asyncio_job callback, void *user);
int mulib_asyncio_run_once(mulib_asyncio *loop, int timeout_ms);
```

## Capacity, allocation, and identifiers

Create accepts `1..mulib_select_limit()` watch slots and `1..65536` job slots.
Descriptor numbers must independently be in `[0, mulib_select_limit())`.
Combined watch/job capacities must also fit `INT_MAX`, bounding callback counts.
Creation allocates the loop plus four fixed arrays; all multiplication is
checked. It returns NULL on invalid capacity (`EINVAL`), allocation failure
(`ENOMEM`), or unsupported backend (`ENOSYS`), cleaning all partial allocations.
No operation after creation allocates or grows storage.

Allocator overrides `MULIB_ASYNCIO_ALLOC(size)` and `MULIB_ASYNCIO_FREE(pointer)`
may be defined before implementation inclusion; they default to malloc/free.
Supply a matching pair with ordinary malloc alignment and free(NULL) semantics.
They affect only this module. Failure injection is tested at all five allocation
points. Allocation/user callback state is not protected by locks.

Only one watch per fd is permitted (`EEXIST`). Full watch/job tables return
`ENOSPC`. Watch IDs are nonzero 64-bit monotonic identifiers, never reused within
a loop; exhaustion reports `EOVERFLOW`, not a wrapped identifier collision.
`watch()` changes its output id only on success. Removing a missing id reports
`ENOENT`. Interest is a nonempty subset of `MULIB_SELECT_READ`, `WRITE`, `EXCEPT`;
EXCEPT is native exceptional readiness, **not** a generic error flag.

## One iteration and callback mutation

`run_once()` accepts only a finite nonnegative millisecond timeout (`0` polls).
It returns the number of invoked callbacks/jobs, zero for no work, or -1.
Execution order is precisely:

1. Capture the queue length at entry. Execute exactly those jobs in FIFO order,
   popping before invocation; jobs posted by them remain for the next call.
2. Snapshot currently active watches and call select once. If entry-time jobs
   ran, use timeout zero rather than sleeping after doing work.
3. Invoke ready callbacks in watch-slot order. Before each dispatch verify its
   original watch ID is still current, suppressing removed/replaced watches.

Callbacks may post jobs and add/remove watches. Watches added during readiness
dispatch are eligible on the next iteration, never through stale snapshot
events. Removing another ready watch suppresses its pending callback; replacing
it with the same fd still has a new ID. Watches added by jobs are eligible in
the same iteration's readiness poll. Slots are reused, so slot order is not a
stable application priority. Jobs posted by readiness callbacks wait until the
next iteration. There is no starvation guarantee beyond the bounded batch.

Recursive run and destroy during any callback fail with `EBUSY`. Callbacks must
return normally: no longjmp out of dispatch, destroying the loop behind its
back, or concurrent mutation. Callback bodies must themselves be nonblocking;
a finite wait timeout cannot bound arbitrary application callback duration.
Neither post nor other APIs are async-signal-safe or thread-safe. Posting from
another thread does not wake the blocked select and is unsupported.

## Ownership, errors, and native integration

The loop **borrows** fds and all user pointers. It never closes fds, frees user
objects, or invokes queued jobs during destroy. Keep pointers alive until their
watch/job is removed, executed, or discarded by destroy. `destroy(NULL)` is
allowed; destroy otherwise frees only idle loop storage. All ordinary mutators
return 0/-1 with errno. Null required inputs, invalid masks/fds/timeouts/ids set
`EINVAL`; native select errors (including EINTR/EBADF) propagate without retry.
Jobs may already have run before a wait error: -1 does not roll back callbacks.
Errno is unspecified after success. The caller owns retry/deadline policy.

Use nonblocking fds and handle `EAGAIN` even after readiness. Unwatch **before**
closing a borrowed descriptor; otherwise fd reuse can target an unrelated
resource. Supported integration is direct fd readiness:

| Source | Callback action |
| --- | --- |
| POSIX pipe/socket | native or `mulib_syscall_read/write`; read zero means EOF |
| Linux eventfd | `mulib_eventfd_read` drains counter/semaphore unit |
| Linux timerfd | `mulib_timerfd_read` consumes expiration count |
| Linux inotify | `mulib_fswatch_read`, then decode batch with `next` |
| Signal notification flag | inspect in ordinary jobs/program flow, using finite waits |

Those modules have no required header dependency on the reactor and must have
their own implementation switches supplied when used. `mulib_perf_now` can
measure loop duration/deadlines; epoll is a separate native scalable wrapper,
**not** this reactor's backend. High fd numbers require a separate epoll-based
application loop rather than pretending select supports them.

## Runnable example and checks

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/asyncio.c -o /tmp/opencode/asyncio-example
/tmp/opencode/asyncio-example
```

The example defines ASYNCIO, SELECT, and SYSCALL explicitly. A queued job writes
a nonblocking pipe; readiness reads it and removes the watch, then the program
destroys the loop and closes its own fds. The focused suite adds job deferral,
mutation/stale-ID suppression, allocation failures, limits, recursion/lifecycle
errors, fd ownership, and a single reactor integrating real eventfd, timerfd,
and inotify events with bounded waits. Platform/permission restrictions are
reported as explicit skips, never simulated successes.
