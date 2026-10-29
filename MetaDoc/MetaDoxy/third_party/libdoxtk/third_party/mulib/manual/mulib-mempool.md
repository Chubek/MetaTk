# mulib-mempool

A growing pool of fixed-size objects, dogfooding `mulib-alloc` and
`mulib-port`. Define `MULIB_IMPL_MEMPOOL`, `MULIB_IMPL_ALLOC`, and
`MULIB_IMPL_PORT` once. No external dependencies.

## API and ownership

`mulib_mempool_create(object_size,slots_per_chunk,allocator)` copies the
allocator and returns an owned pool. Both sizes must be positive and their
aligned product representable. Slots are aligned to `max_align_t`; types with
greater alignment are unsupported. Capacity grows a chunk at a time.

`acquire(pool)` returns an uninitialized pool-owned slot, or NULL on failure.
Growth never moves outstanding slots. `release(pool,slot)` returns 1 for a live
slot and 0 for NULL, foreign pointers, interior pointers, or already freed
slots. These checks do not provide temporal safety: a stale pointer cannot be
distinguished from a new live allocation reusing the same address.

`in_use` and `capacity` report slot counts. `reset` invalidates all slots while
retaining chunks. `clear` releases chunks but retains the reusable pool object.
`destroy` releases everything. No nested-object destructors run. Do not access
released slots or destroy an allocator context while a pool using it lives.
All operations on a pool require external synchronization.

Acquiring/releasing scans chunks for slot validation; do not assume constant
time independently of the number of chunks. Occupancy uses one bit per slot.

## Example

```c
#define MULIB_IMPL_PORT
#define MULIB_IMPL_ALLOC
#define MULIB_IMPL_MEMPOOL
#include <mulib-mempool.h>
#include <assert.h>
int main(void) {
    mulib_mempool_t *p = mulib_mempool_create(sizeof(int), 64, NULL);
    int *value;
    assert(p && (value = mulib_mempool_acquire(p)));
    *value = 7;
    assert(mulib_mempool_release(p, value));
    mulib_mempool_destroy(p);
    return 0;
}
```

Runnable example: `examples/modules/mempool.c`.
