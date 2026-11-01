# MuLIB process

**Header/switch:** `include/mulib-process.h` / `MULIB_IMPL_PROCESS`  
**Dependencies:** POSIX `posix_spawnp`, pipes/fcntl, signals and waitpid; no MuLIB
dependencies and no shell. C11 implementation, C++17 declaration inclusion.

## Spawn without interpolation

```c
mulib_process_t child;
const char *args[] = {"echo", "literal;$(not-a-command)"};
mulib_process_init(&child);
/* args need not contain a trailing NULL: argc is explicit. */
int rc = mulib_process_spawn(&child, "/bin/echo", args, 2, NULL);
```

The file is passed to `posix_spawnp`; a bare name uses PATH, a path containing `/`
is used directly. Pass an absolute executable path when PATH is not trusted.
The environment is inherited. Every argument is passed literally; no quoting,
glob expansion, pipes, variables or shell metacharacters are interpreted. `argv[0]`
must be supplied and may differ from the file path. Argument strings are borrowed
only for the spawn call; a temporary NULL-terminated pointer vector is allocated
after overflow validation. Empty file, zero argc, NULL strings, an already-live
output handle and impossible pointer-vector sizes are rejected. Child exec
failure is reported by spawn, not hidden behind a successful parent-side fork.

## Descriptors and composing pipes

`mulib_process_io_t` has `stdin_fd`, `stdout_fd`, `stderr_fd`; NULL io inherits
all three, and a `-1` member inherits that stream. Other members must be open
descriptors >=3 or equal to their target standard descriptor (0,1,2). Swapping
standard descriptors is deliberately rejected. Spawn duplicates configured
sources into child standard streams and closes original source descriptors >=3
in the child, after all duplications; shared sources are supported. Parent
descriptor ownership is unchanged. Unrelated descriptors follow ordinary POSIX
inheritance: mark them close-on-exec yourself.

`mulib_process_pipe(fds)` returns `[read_end, write_end]`, both >=3 and marked
FD_CLOEXEC. On failure a non-NULL output is set to `{-1,-1}` and any partial
resources are closed. `mulib_process_close(fd)` closes once; it does not retry
EINTR because the descriptor may already have been closed and reused. Parent
must close **every endpoint it no longer needs**, especially write ends, or a
reader cannot observe EOF. Read/write use ordinary POSIX APIs, including EINTR
handling. Pipe buffering is finite: drain output while a child produces it,
rather than waiting for an output-heavy child before reading. Full-duplex
streaming needs an application event loop or separate I/O threads.

Linux builds exposing `O_CLOEXEC` use atomic `pipe2`; relocation above standard
descriptors uses `F_DUPFD_CLOEXEC` where available. The fallback POSIX pipe+fcntl
sequence is not atomic against a concurrent exec. Serialize fallback pipe creation
against other process launches when descriptor-leak security matters.
This module does not change global feature-test macros in a
public header; its C11 implementation declares the POSIX `kill(pid_t,int)`
prototype if strict C mode hides that declaration.

## Child ownership, waiting and errors

Initialize unused handles with `mulib_process_init`. A successful spawn stores
one positive pid. Handles are **unique owners**, not copyable references. Do not
reinitialize/overwrite/discard a live handle; always wait or terminate it. Do not
use SIGCHLD auto-reaping, a competing waitpid handler, or another library to reap
these children. Do not mutate live pids or use inherited handles after fork.

`mulib_process_wait(&child, &status, nohang)` retries waitpid on EINTR. With
`nohang != 0`, `MULIB_PROCESS_RUNNING` leaves both handle and status untouched.
Successful reaping clears pid and reports `status.exited`, `exit_code` (normal
exit only), and `term_signal` (signal termination only). Waiting twice is invalid.
An ECHILD system error also clears the handle to avoid retaining a stale pid
after ownership was violated by an external reaper; status remains untouched.
Child exit code nonzero is **not** a wait error; inspect status. Wait normally
blocks until child termination, so the application owns any execution timeout.

`mulib_process_terminate(&child, &status)` first checks for an already exited or
externally reaped child before using the PID as a signal target. A running child
receives **SIGKILL** and is synchronously reaped. This does not make competing
reapers safe: ownership must remain exclusive throughout the call. This is deliberately abrupt,
without grace periods, restart, process-group killing or shell job control.
Descendants are not managed. Do not spawn daemonizing services if you need
complete teardown. No safe API can force an uninterruptible kernel operation to
finish within an exact wall-clock deadline.

Returns: `MULIB_PROCESS_OK=0`, `RUNNING=1`, and negative `INVALID`, `NOMEM`,
`SYSTEM`, `UNSUPPORTED`. SYSTEM preserves a relevant POSIX error in `errno`,
including posix_spawn's returned error number; successful calls make no errno
promise. Invalid arguments do not promise errno. Output status is meaningful
only after successful reap. No allocator injection is provided.

## Platform and example

Unix/Apple builds use POSIX. Other platforms or `MULIB_PROCESS_NO_POSIX` builds
return `UNSUPPORTED` for OS operations; initialization is still usable. There is
no fake success, shell fallback or Windows emulation.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/process.c -o /tmp/process
/tmp/process
```

The example uses `/bin/echo`, captures a literal metacharacter-containing argument
through a pipe, and reaps the child. The focused Linux tests compose echo -> cat
with two pipes, check missing executable/nonzero exit/nohang, and verify SIGKILL
teardown leaves no waitable child. Examples assume `/bin/echo`, `/bin/cat`,
`/bin/false`, `/bin/true`, `/bin/sleep` as appropriate; no public network is used.
