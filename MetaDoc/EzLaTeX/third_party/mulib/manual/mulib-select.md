# MuLIB select

**Header:** `include/mulib-select.h`  
**Implementation:** `MULIB_IMPL_SELECT` in exactly one translation unit.  
**Dependencies:** none; POSIX libc. C11, C++ declarations with C linkage.

## Supported scope

A single native `select()` call over borrowed integer descriptors on Unix/macOS.
Windows/Winsock is not implemented: native entry points return `-1` with
`errno = ENOSYS`. No feature-test macros are defined by this header; it compiles
after standard headers in strict C11. There is no heap allocation, thread,
descriptor ownership, automatic retry, cancellation, or timeout deadline tracking.

```c
enum { MULIB_SELECT_READ = 1, MULIB_SELECT_WRITE = 2, MULIB_SELECT_EXCEPT = 4 };
typedef struct mulib_select_entry {
    int fd;
    unsigned events, revents;
} mulib_select_entry;
int mulib_select_limit(void);
int mulib_select_wait(mulib_select_entry *entries, size_t count, int timeout_ms);
```

## Readiness and bounds

`limit()` returns native `FD_SETSIZE`: both the exclusive upper descriptor bound
and maximum entry count. Every descriptor must be in `[0, limit())`; each interest
mask must be a nonempty subset of the three flags. Validation occurs before
`FD_SET`, so a high descriptor never causes an out-of-bounds set operation.
Use epoll instead when Linux descriptor numbers exceed this limit.

`READ` includes EOF; `WRITE` does not guarantee a whole write will fit.
`EXCEPT` is the native exception set, typically socket out-of-band data, **not**
a generic socket-error notification. Query `SO_ERROR` to verify asynchronous
connect completion. Use nonblocking descriptors: readiness is a hint, and a
later read/write can still report `EAGAIN`.

Timeout `0` polls; `-1` waits indefinitely; positive values are milliseconds.
Values below `-1` fail. A zero-entry call accepts `entries == NULL` and acts as a
native timed wait (or indefinite wait for `-1`). Examples use finite timeouts.
Scheduling may delay return beyond the requested timeout.

## Results, errors, ownership

`wait()` returns the number of entries with nonzero `revents`, zero on timeout,
or `-1`. Duplicate descriptors are allowed and counted once per ready entry.
Results include only requested flags. Validation errors set `EINVAL` and leave
entries unchanged. After valid input, `revents` are cleared before calling the
OS; they remain zero on native failure. Native `EBADF`, `EINTR`, etc. propagate
without retry; other successful calls do not promise to preserve `errno`.
Input arrays must remain valid throughout the call. The wrapper never closes,
duplicates, or changes flags on a descriptor. Synchronize close and concurrent
descriptor access externally.

## Runnable example and verification

`examples/modules/select.c` creates a nonblocking pipe, checks empty readiness,
writes, waits at most 1000 ms, reads, and explicitly closes both ends. Its helper
pipe comes from `syscall`; the example defines both implementation switches.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/select.c -o /tmp/opencode/select-example
/tmp/opencode/select-example
```

`tests/test_os_events.c` also covers EOF, duplicate entries, high/negative
descriptors, invalid masks, count limits, empty input, socket loopback connect,
and reactor integration. No third-party library is required.
