# MuLIB timerfd

**Header:** `include/mulib-timerfd.h` · **Switch:** `MULIB_IMPL_TIMERFD`  
**Dependencies:** none. Linux native timerfd; C11 / C-linkage declarations.

## Supported timers

```c
int mulib_timerfd_create(void);
int mulib_timerfd_set(int fd, uint64_t initial_ns, uint64_t interval_ns);
int mulib_timerfd_read(int fd, uint64_t *expirations);
int mulib_timerfd_close(int fd);
```

Creation uses Linux `CLOCK_MONOTONIC`, `TFD_NONBLOCK`, and `TFD_CLOEXEC` and
returns a newly owned, initially disarmed descriptor or `-1`. Monotonic time
does not jump with wall-clock changes and does not include Linux suspend time.
No realtime/boottime/alarm clock, absolute deadlines, cancel-on-set, old-value
query, or native gettime API is exposed. Other platforms return `ENOSYS` for
valid native requests. The header compiles after standard headers in strict
C11 without setting feature macros: when POSIX clock constants are hidden it
uses Linux's defined CLOCK_MONOTONIC ABI value (1).

## Arming and limits

`set()` replaces the current schedule with a **relative** initial expiration and
interval in nanoseconds. Nonzero initial with zero interval is one-shot;
nonzero interval is periodic. Initial zero disarms, irrespective of interval.
Both parameters are range-checked even when disarming. The supported maximum
for either parameter is `2147483647 * 1000000000 + 999999999` ns, intentionally
bounded to signed 32-bit seconds for safe conversion to native `time_t`.
Larger values fail with `EOVERFLOW` before changing the timer. Native kernel
limits still apply and can produce native errors. Set returns 0/-1.

Timers are scheduler-driven, not real-time guarantees. A delayed consumer can
observe multiple periodic expirations in one read. Reading consumes the native
eight-byte expiration count; empty/disarmed/not-yet-expired reads report
`EAGAIN`. The output stays unchanged on any failure. Disarming with settime
also clears unread expirations under Linux semantics.

## Errors, ownership, and composition

All mutators/read/close return 0/-1. Negative fds and null read outputs set
`EINVAL`; native errors, including `EINTR`, propagate without retry. Unexpected
short reads set `EIO`. Only pass actual timerfd descriptors: type provenance is
not tracked and a wrong fd may consume data. Errno is unspecified on success.
No heap allocations, threads, signal handlers, or implicit MuLIB implementations.
Creation failure transfers nothing. Close once; ownership is relinquished on
close even if it reports interruption, so never retry a possibly reused fd.

Select, epoll, and the asyncio reactor can borrow a timerfd for read readiness.
Unregister before closing. Reactor callbacks may read one expiration count,
rearm/disarm, or unwatch. Nonblocking reads must still handle `EAGAIN` in
applications with concurrent consumers.

## Example and verification

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/timerfd.c -o /tmp/opencode/timerfd-example
/tmp/opencode/timerfd-example
```

The example explicitly enables both TIMERFD and SELECT, arms a 1 ms one-shot,
waits at most 2000 ms, reads, disarms, and closes. Unsupported/permission
restrictions print a skip. `tests/test_os_events.c` also covers periodic timers,
empty reads, conversion limits, fd flags, borrowed ownership, and event-loop
integration. No elevated permissions or third-party library is normally needed.
