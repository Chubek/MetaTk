# MuLIB perf

**Header:** `include/mulib-perf.h` · **Switch:** `MULIB_IMPL_PERF`  
**Dependencies:** none; native monotonic libc clock. C11 / C-linkage declarations.

## Supported scope and platforms

This module measures elapsed monotonic nanoseconds, **not** Linux perf hardware
events, process CPU time, sampling, wall-clock dates, or benchmark statistics.
There is no allocation, implicit thread, signal handler, busy wait, or fallback
to a wall clock pretending to be monotonic.

```c
int mulib_perf_now(uint64_t *nanoseconds);
int mulib_perf_elapsed(uint64_t begin, uint64_t end, uint64_t *nanoseconds);
```

`now()` uses `clock_gettime(CLOCK_MONOTONIC, ...)` when the platform exposes that
API. On LP64 Linux (including strict C11 after standard headers), it can also use
Linux's clock-id ABI value 1 and a compatible libc ABI declaration when POSIX
declarations are hidden. This fallback is deliberately limited to LP64 Linux;
it does not guess 32-bit libc time64 symbol redirects. Other systems lacking an
exposed monotonic API return `-1` with `ENOSYS`. On POSIX systems where needed,
define `_POSIX_C_SOURCE=200809L` on the compiler command line **before all system
headers** to expose their native clock interface. This header never defines
feature-test macros too late. Older Linux libc versions can require `-lrt`;
modern Linux libc links without extra libraries.

## Values, arithmetic, and errors

The timestamp's origin is unspecified and is not a Unix date. Compare only
timestamps from the same native clock domain. Linux CLOCK_MONOTONIC excludes
suspend time. Resolution is native: consecutive reads may be equal, and a
nanosecond unit does not imply nanosecond accuracy. Store a starting timestamp,
do work, read an ending timestamp, and use `elapsed()` to subtract.

Both functions return 0/-1. Null output or backwards subtraction (`end < begin`)
sets `EINVAL`. `now()` rejects invalid native timespec values with `EIO` and
checks seconds-to-nanoseconds addition/multiplication before writing; overflow
sets `EOVERFLOW`. The largest supported result is `UINT64_MAX` ns (approximately
584 years). Native clock failures retain OS errno. Outputs remain unchanged on
failure; successful errno is unspecified. `elapsed()` is checked integer
arithmetic and remains available even on platforms without a clock backend.

No global mutable state means independent reads do not need library locking;
caller-owned outputs still need normal synchronization. No MuLIB implementation
switch other than PERF is required. This clock can bound finite select/reactor
retries after EINTR; the readiness wrappers intentionally do not retry for you.

## Runnable example and checks

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/perf.c -o /tmp/opencode/perf-example
/tmp/opencode/perf-example
```

The example checks ordering and elapsed subtraction without assuming a positive
minimum duration; unavailable/denied clocks print a skip. `tests/test_os_events.c`
adds null outputs, full-width subtraction, equal timestamps, and backwards
input. Strict independent compilation also tests inclusion after `<time.h>`.
