# MuLIB eventfd

**Header:** `include/mulib-eventfd.h` · **Switch:** `MULIB_IMPL_EVENTFD`  
**Dependencies:** none. Linux eventfd; C11 / C-linkage declarations.

## Exact API

```c
int mulib_eventfd_create(uint32_t initial, int semaphore);
int mulib_eventfd_read(int fd, uint64_t *value);
int mulib_eventfd_write(int fd, uint64_t value);
int mulib_eventfd_close(int fd);
```

The supported counter is the kernel's 64-bit eventfd counter. Creation always
uses `EFD_NONBLOCK | EFD_CLOEXEC`; `semaphore` must be exactly 0 or 1 and selects
`EFD_SEMAPHORE`. `initial` is the native 32-bit initial counter value. The result
is a newly owned fd, or `-1`. There is no allocation, emulation, implicit worker,
feature-test macro, or MuLIB dependency. Non-Linux systems report `ENOSYS` on
valid native requests, never a successful no-op.

## Counter behavior

Writing adds to the counter. `UINT64_MAX` is rejected with `EINVAL`;
the maximum stored value is `UINT64_MAX - 1`. A write that would exceed it fails
with `EAGAIN`, rather than blocking. Writing zero is supported but does not make
an empty counter readable. A normal read returns the accumulated value and
resets the counter to zero. A semaphore read returns one and subtracts one.
Reading an empty counter returns `EAGAIN`. The output is unchanged on failure.

Read/write perform exactly one native eight-byte operation. They return 0 on
success, `-1` on error, propagating `EINTR` without retry. An unexpected short
operation sets `EIO`; supply only genuine eventfd descriptors to these functions,
since a wrong descriptor could consume unrelated bytes before that check.
Negative fds, null read output, and invalid mode/value set `EINVAL`.
Successful calls do not specify errno.

## Ownership and readiness integration

The caller closes the fd once. `close()` returns native 0/-1 and never retries:
do not use the descriptor number again after relinquishing ownership. There is
no tracking of fd provenance or double close.

An eventfd is readable when its counter is nonzero and writable when there is
room for at least one. It can be borrowed by `mulib_select_wait`, epoll, or
`mulib_asyncio_watch(..., MULIB_SELECT_READ, ...)`. A readiness callback should
read the counter, handle possible `EAGAIN`, and unregister before closing it.
Kernel counter operations may be used by multiple producers, but synchronize
close and ownership externally. Eventfd is not a message queue: counts coalesce.

## Runnable example and checks

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/eventfd.c -o /tmp/opencode/eventfd-example
/tmp/opencode/eventfd-example
```

The assert-based example checks empty-counter failure, accumulation/drain, and
invalid values; unsupported/permission restrictions print a skip. The focused
`tests/test_os_events.c` additionally checks saturation, semaphore reads, fd flags,
unchanged failure outputs, epoll registration, reactor integration, and close
ownership. No privileges or third-party libraries are normally needed.
