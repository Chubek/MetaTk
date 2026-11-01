# MuLIB epoll

**Header:** `include/mulib-epoll.h` · **Switch:** `MULIB_IMPL_EPOLL`  
**Dependencies:** none; Linux libc/kernel epoll. C11 and C-linkage declarations.

## Native subset

This is a real Linux epoll wrapper, not an emulation. `create()` uses
`epoll_create1(EPOLL_CLOEXEC)`. Other platforms return `-1`/`ENOSYS` for valid
native requests. The header does not set feature-test macros and supports strict
C11 inclusion after standard headers. No heap allocation or background work.

```c
typedef struct mulib_epoll_event { unsigned events; uint64_t token; } mulib_epoll_event;
int mulib_epoll_create(void);
int mulib_epoll_add(int epfd, int fd, unsigned events, uint64_t token);
int mulib_epoll_modify(int epfd, int fd, unsigned events, uint64_t token);
int mulib_epoll_remove(int epfd, int fd);
int mulib_epoll_wait(int epfd, mulib_epoll_event *events, size_t capacity, int timeout_ms);
int mulib_epoll_close(int epfd);
```

## Masks and registration

Input flags are `MULIB_EPOLL_READ`, `WRITE`, `PRIORITY`, `RDHUP`, `EDGE`, and
`ONESHOT`. A zero input mask is allowed (native error/hangup-only interest).
`ERROR` and `HANGUP` are output-only and always reported when the kernel reports
them. Wait results translate native readiness into these flags; `EDGE` and
`ONESHOT` are registration policies, not returned readiness bits. No support for
exclusive wakeups, wake locks, pointer tokens, or arbitrary epoll flags.

`add()` registers a borrowed fd and all 64 bits of an uninterpreted application
token. `modify()` replaces mask/token and rearms a one-shot registration.
`remove()` removes interest without closing the watched descriptor. Duplicate
adds propagate `EEXIST`; modifying/removing missing registrations propagates
native errors. Regular files may produce `EPERM`: epoll cannot watch them.

The application owns fd reuse and token lifetime. Unregister before closing a
watched fd; duplicated descriptors have native open-file-description semantics.
The library cannot detect stale application tokens.

## Waiting and resource limits

`wait()` accepts a nonnull array with capacity `1..MULIB_EPOLL_BATCH_MAX` (256),
using a fixed native-event stack batch. It returns an event count, zero for
timeout, or `-1`. Only the returned array prefix is modified. Timeout `0` polls,
`-1` blocks indefinitely, positive values are milliseconds, and smaller values
fail. EINTR is returned directly: no hidden retry can extend a deadline.
Readiness is not completion; use nonblocking descriptors. Edge-triggered users
must drain until `EAGAIN`. One-shot users must explicitly rearm.

## Ownership and errno

`create()` returns a newly owned descriptor or `-1`. Other mutators and `close()`
return 0/-1. Invalid negative fds, masks, capacities, or timeout set `EINVAL`;
native failures preserve OS `errno`. No errno guarantee on success.
Close only the epoll descriptor with `close()`; watched descriptors remain owned
by their creator. Close is attempted once; relinquish ownership when calling it
and never retry an interrupted close on a potentially reused descriptor number.
There is no userspace mutable global state or implicit synchronization.

## Example and tests

`examples/modules/epoll.c` registers an eventfd with token 42, increments it,
waits with a finite timeout, drains it, unregisters, and closes both owned fds.
It explicitly defines `MULIB_IMPL_EPOLL` and `MULIB_IMPL_EVENTFD` (the latter is
an example dependency, not a header dependency). Permission/platform failures
are printed as skips.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/epoll.c -o /tmp/opencode/epoll-example
/tmp/opencode/epoll-example
```

`tests/test_os_events.c` checks actual readiness, full-width tokens, duplicate
registration, one-shot rearming, invalid bounds, removal, and borrowed ownership.
