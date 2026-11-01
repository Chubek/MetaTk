# MuLIB signal

**Header:** `include/mulib-signal.h` · **Switch:** `MULIB_IMPL_SIGNAL`  
**Dependencies:** none; hosted ISO C `signal()`. C11 / C-linkage declarations.

## Deliberately narrow scope

Scoped installation/restoration of native C signal dispositions. This module is
**not** a signal-to-callback event dispatcher, sigaction abstraction, signalfd,
signal mask manager, interprocess signal sender, or guarantee of persistent
handlers. No POSIX feature-test macros are needed, even after standard headers.
The actual runtime's `signal()` semantics apply, including possible reset of a
handler on delivery and implementation-defined restart/blocking behavior.
Only the prior handler/ignore/default value returned by `signal()` is saved;
POSIX sigaction masks, flags, and SA_SIGINFO handlers are not preserved. Do not
use this guard to wrap dispositions that require that richer state restored;
use native sigaction with a full native saved action instead.

```c
typedef void (*mulib_signal_handler)(int);
typedef struct mulib_signal_guard {
    int number;
    mulib_signal_handler previous;
    int active;
} mulib_signal_guard;
int mulib_signal_install(mulib_signal_guard *guard, int number,
                         mulib_signal_handler handler);
int mulib_signal_restore(mulib_signal_guard *guard);
```

## Lifecycle and safe policy

Initialize a guard as `{0, 0, 0}`. Install saves the prior disposition and marks
the guard active only after success. Pass a valid platform signal number and a
handler, `SIG_DFL`, or `SIG_IGN`; `SIG_ERR`, nonpositive numbers, and exposed
`SIGKILL`/`SIGSTOP` are rejected. Other native unsupported signal numbers fail
through `signal()`. Reinstalling into an active guard reports `EBUSY` without
changing it. Restore reinstates the saved disposition, then marks inactive;
restoring an inactive guard succeeds without touching native signal state.
Restore failure keeps the guard active so the caller can inspect/recover.

Never copy an active guard, overwrite its fields, restore out of nesting order,
or modify the same disposition independently while a guard owns its restore
responsibility. Guards for a shared signal must be strictly nested and restored
in reverse order. Installation and restoration are process-global operations:
perform them from ordinary program flow with application synchronization,
not from a handler or concurrent threads racing signal setup.

For portable safety a handler should only assign a `volatile sig_atomic_t`
notification flag and return. Do **not** call malloc/free, printf, reactor/job
functions, these install/restore functions, or arbitrary callbacks in a handler.
Poll the flag in ordinary code with a finite reactor timeout. Flags coalesce
notifications; this is not lossless signal counting. Do not read/modify ordinary
shared objects from the handler. Do not attempt recovery from hardware-generated
SIGSEGV/SIGILL/SIGFPE by simply returning. The wrapper cannot enforce callback
body safety or provide signal masking around a read-and-clear operation.

## Errors and resource behavior

Return 0 for success and -1 for failure. Null guards and invalid inputs set
`EINVAL`. Native errors propagate; if the C runtime fails without setting errno,
the wrapper supplies `EINVAL`. Successful install/restore preserve incoming
errno. No allocation, descriptor, worker, hidden state, or platform emulation is
involved. Hosted C signal support is the native backend; failure is never
silently treated as successful installation.

## Example and tests

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/signal.c -o /tmp/opencode/signal-example
/tmp/opencode/signal-example
```

The example installs a flag-only SIGINT handler, uses `raise(SIGINT)` once,
verifies synchronous delivery, and restores the original disposition. It does
not send external signals or wait indefinitely. The focused OS suite adds
invalid input, active-guard rejection, and idempotent restoration tests.
