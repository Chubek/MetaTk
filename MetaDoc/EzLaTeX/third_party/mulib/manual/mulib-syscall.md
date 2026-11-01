# MuLIB syscall

**Header:** `include/mulib-syscall.h` · **Switch:** `MULIB_IMPL_SYSCALL`  
**Dependencies:** none; POSIX libc. C11 / C-linkage declarations.

## Exact native subset

Despite its name this is **not an arbitrary syscall-number dispatcher**. It
offers five focused descriptor operations with checked sizes and ownership:

```c
int mulib_syscall_pipe(int descriptors[2]);
int mulib_syscall_nonblock(int fd, int enabled);
int mulib_syscall_read(int fd, void *buffer, size_t size, size_t *count);
int mulib_syscall_write(int fd, const void *buffer, size_t size, size_t *count);
int mulib_syscall_close(int fd);
```

Supported on Unix/macOS. Other platforms return `-1` with `ENOSYS` for valid
native requests; there is no fake fd or successful unsupported operation. The
header needs no feature-test macros and compiles after standard headers in
strict C11. No heap allocation or implicit MuLIB implementation.

## Pipe creation and flags

`pipe()` creates two owned ends (index 0 read, index 1 write), configures both
`O_NONBLOCK` and `FD_CLOEXEC`, and returns 0. On any failure it closes every end
created so far, preserves the original error, and leaves the caller's output
pair unchanged. The pair pointer must provide two integer slots.

Linux builds exposing `O_CLOEXEC` use `pipe2` to install both flags atomically.
Other builds use portable `pipe` plus `fcntl`: close-on-exec installation is
then **not atomic with creation**. Serialize fallback creation against concurrent
fork/exec when descriptor inheritance matters. No feature-test macro is set by
the header. `nonblock()` accepts exactly 0 or 1,
reads native flags, changes only `O_NONBLOCK`, and preserves unrelated flags.
Concurrent read-modify-write flag changes require application synchronization.

## Read/write and errno

One call means one native read or write; there is no retry or write-all loop.
Return 0 for a successful operation and store the native transferred byte count,
which may be short. A read count of zero means EOF for a nonzero request. A
zero-size request is permitted with a null buffer and succeeds/fails according
to the native descriptor semantics. Nonzero requests need an accessible buffer.
The output count pointer is always required. It stays unchanged on failure.

Negative fds, null required pointers, mode errors, or sizes above `PTRDIFF_MAX`
set `EINVAL`. Native errors such as `EAGAIN`, `EINTR`, and `EBADF` propagate
unchanged. Errno is unspecified after success. **Writes can generate SIGPIPE**;
the application must select a signal policy before writing to possibly closed
pipes/sockets. This module never changes process-wide dispositions silently.

## Close and ownership

`close()` attempts native close once and returns 0/-1. The caller relinquishes
ownership on call, even on EINTR, and must never blindly retry a possibly reused
descriptor number. Platforms differ in interrupted-close state; recover using
platform-specific policy if necessary. The wrapper neither tracks provenance
nor makes repeated closes safe. Do not close fds while select/epoll/reactor
watches still borrow them. Reads and writes never consume ownership.

## Runnable example and tests

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/syscall.c -o /tmp/opencode/syscall-example
/tmp/opencode/syscall-example
```

The example writes and reads a local pipe, closes the writer, verifies EOF, and
closes the reader without any blocking read. Platform/permission restrictions
print explicit skips. `tests/test_os_events.c` covers fd flags, nonblocking empty
reads, invalid sizes and pointers, unchanged failure outputs, readiness, EOF,
loopback sockets, and reactor integration.
