# MuLIB initsys

**Header/switch:** `include/mulib-initsys.h` / `MULIB_IMPL_INITSYS`  
**Transitive implementations:** `MULIB_IMPL_TASKDAG`, `MULIB_IMPL_TASKPOOL`,
`MULIB_IMPL_PROCESS`, each provided explicitly in exactly one C11 translation unit.
Link **`-pthread`** on a supported POSIX host. C++17 declarations are supported.

## Purpose and boundaries

An embeddable bounded service-lifecycle supervisor. It composes taskdag's actual
worker-pool dependency execution with process's argv-based child ownership.
Services can be application callbacks or foreground child processes. Dependencies
control successful **start completion**, not ongoing health or socket readiness.
This is not PID 1: no system boot configuration, orphan adoption, signal handlers,
daemonization, privilege management, automatic restart, cgroups, process groups,
graceful signal escalation or persisted state are implemented.

## Constructing services

`mulib_initsys_create(&system, service_capacity, dependency_capacity)` requires
positive service capacity; zero dependencies is valid. Fixed arrays and a DAG
are allocated with overflow checks. No service removal is provided.

`mulib_initsys_add(system, start, stop, context, &id)` borrows two non-NULL
`int (*)(void *)` callbacks and a context, returning a stable zero-based ID.
Start returns zero only when the service is live and must later be stopped.
**A failed start must clean up its own partial work.** Successful starts are
paired with stop; stop returns zero when teardown is complete. A failed stop
must leave its context safe for another stop attempt. Callback storage remains
application-owned; callbacks must return normally and not wait on their own pool.

`mulib_initsys_add_process(system, file, argv, argc, optional_io, &id)` creates a
process service. `file`, argument vector and strings are **borrowed until system
destruction**, because subsequent start cycles may spawn again. IO configuration
is copied, but descriptor ownership remains with the caller; configured fds must
stay usable for future starts. NULL IO inherits standard streams. Arguments are
literal; no shell is invoked. See process's descriptor and PATH contracts.
Use foreground, non-daemonizing commands so the direct child is the actual service.

`mulib_initsys_depend(system, prerequisite, dependent)` adds a DAG edge. Duplicate
edges are harmless, capacity exhaustion returns FULL, and cycles (including
self-dependencies) are rejected by start before any callbacks run. Add/depend
are rejected while any service is live. All IDs must already exist.

## Starting and rollback

`mulib_initsys_start(system, pool, optional_token)` requires no currently live
service. It runs successful dependency frontiers in parallel using taskdag.
Independent service callbacks may therefore run concurrently and need safe
shared contexts. Caller must be outside that pool and exclusively control pool
lifecycle. All system operations/queries must be externally serialized; internal
parallelism is confined to distinct service records.

For a process service, a successful start means posix_spawn succeeded, **not** that
the child stayed alive or passed a readiness probe. `check` detects later exit.
A nonzero callback, spawn error, cancellation or cycle returns a task status.
Failure triggers rollback: every service whose start succeeded is stopped in
reverse topological order. Independent starts may have succeeded before failure
was observed; they are rolled back too. Callback/OS details are not exposed as
a per-service diagnostics log; the start result is the DAG-level status.
If rollback fails, `FAILED`/the stop error is returned and live records are
retained for explicit stop/destroy retry. Do not discard the supervisor.

## Stopping, monitoring, destruction

`mulib_initsys_stop(system)` traverses a topological order in reverse and calls
stop synchronously on the calling thread. Process stops use **SIGKILL followed
by waitpid**, ensuring owned direct children are reaped, rather than claiming a
graceful shutdown. Stop continues after errors. Failed records remain live;
prerequisites may already have been stopped, so callbacks must tolerate partial
teardown and retries. A callback that requires stronger transactional teardown
must implement it in application state. No operation forcibly interrupts custom
callbacks. Reverse-order calculation allocates temporary O(V) storage; an
allocation failure leaves services retained and returns an error for retry.

`mulib_initsys_check(system, optional_exited_id, optional_status)` performs
nonblocking waitpid on live process services. If none exited, returns OK and
leaves outputs untouched. **Any** exit, including exit code zero, is an unexpected
service loss: reap that child, mark it stopped, fill outputs, stop all remaining
services and return `MULIB_TASK_FAILED`. If stopping others fails, they remain
live for retry. A wait-system error returns SYSTEM. If an external reaper caused
ECHILD, the stale service is marked stopped and remaining services are stopped;
outputs remain unchanged. Stop likewise reports failure but clears an invalidated
child record, allowing a later destroy retry instead of retaining an unusable PID.
Competing reapers remain an ownership violation. Custom callback services have
no implicit liveness probe. Call check periodically from the application event
loop; it does not run a background monitoring thread. Once all are stopped,
another explicit start is permitted; no restart policy is hidden.

`mulib_initsys_live(system,id)` reports whether the record still needs stopping,
not a kernel liveness guarantee. Invalid input returns `MULIB_TASK_INVALID`.
`mulib_initsys_destroy(system)` first stops; **only on success** does it free
the DAG, service records and order buffer. On error, the pointer remains valid
and must be retained for retry. NULL destroy succeeds. User contexts/argv are
never freed. Custom start/stop callbacks or uninterruptible kernel operations
can still block; choose bounded application callbacks.

All lifecycle success codes are zero; other statuses are taskpool's negative
enum (`INVALID`, `FULL`, `NOMEM`, `CYCLE`, `FAILED`, `CANCELLED`, `SYSTEM`, or pool
errors). Unsupported POSIX spawning fails the process start; unsupported worker
platforms cannot create the required pool. No dependency is silently emulated.

## Runnable example and validation

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -pthread -Iinclude \
  examples/modules/initsys.c -o /tmp/initsys
/tmp/initsys
```

The example starts a callback resource before `/bin/sleep`, then destroys the
supervisor immediately: sleep is killed/reaped before the resource stops.
`tests/test_tasks_process.c` checks exact dependency start/reverse-stop order,
failed-start rollback, failed-stop retry retaining the object, cycle detection
without side effects, direct-child reaping and nonblocking unexpected-exit
supervision. Its process tests use only local standard executables and no network.
