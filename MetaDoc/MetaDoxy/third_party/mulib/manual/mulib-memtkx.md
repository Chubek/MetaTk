# mulib-memtkx

An original memory-toolkit extension: a per-instance tracking allocator with
live-byte budgets and allocation checkpoints. It does not reproduce an external
project's API. Define `MULIB_IMPL_MEMTKX`, `MULIB_IMPL_ALLOC`, and
`MULIB_IMPL_PORT` once. C11 only; no global hooks or external libraries.

## Ownership and behavior

`create(backing)` returns an owned tracker and copies backing callbacks (NULL
means libc). `allocator(tracker)` returns a borrowed callback view suitable for
MuLIB pools and other allocator-aware objects. The tracker must outlive every
user of that view. `destroy` also frees every outstanding tracked block; it
does not run object destructors. Destroy higher-level objects first.

`stats` reports live blocks, live payload bytes, peak payload bytes, successful
allocation/reallocation counts, and failures. Tracking headers and the tracker
object are not included. Cumulative counters saturate at SIZE_MAX. Allocations
are max_align_t aligned, not over-aligned. Reallocation preserves payload up
to the minimum size and supports backing allocators without realloc through a
checked allocate/copy/free fallback.

`set_limit` sets a live-payload budget and rejects a budget below current use.
Failed growth preserves the original allocation. Foreign/interior/double free
callbacks are rejected and counted, but stale pointers to reused addresses are
not detectable. Validation scans the live list and is O(live blocks).

`mark` returns an allocation sequence. `release_since(t,mark)` invalidates
allocations created later and returns their count; reallocating a pre-mark
block keeps its original age. `reset` releases all live blocks without clearing
cumulative counters or reusing sequence numbers. Checkpoints do **not** roll
back writes, resizes, or non-memory resources. Sequence exhaustion causes
subsequent new allocations to fail. No operation is thread-safe.

## Example

```c
#define MULIB_IMPL_PORT
#define MULIB_IMPL_ALLOC
#define MULIB_IMPL_MEMTKX
#include <mulib-memtkx.h>
#include <assert.h>
int main(void) {
    mulib_memtkx_t *t = mulib_memtkx_create(NULL);
    mulib_allocator_t a = mulib_memtkx_allocator(t);
    uint64_t mark;
    assert(t);
    mark = mulib_memtkx_mark(t);
    assert(mulib_alloc(&a, 32));
    assert(mulib_memtkx_release_since(t, mark) == 1);
    mulib_memtkx_destroy(t);
    return 0;
}
```

`examples/modules/memtkx.c` also demonstrates tracking a MuLIB memory pool.
