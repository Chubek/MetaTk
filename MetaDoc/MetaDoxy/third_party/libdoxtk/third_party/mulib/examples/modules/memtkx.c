#define MULIB_IMPL_PORT
#define MULIB_IMPL_ALLOC
#define MULIB_IMPL_MEMTKX
#define MULIB_IMPL_MEMPOOL
#include "mulib-memtkx.h"
#include "mulib-mempool.h"
#include <assert.h>
int main(void) {
    mulib_memtkx_t *tracker = mulib_memtkx_create(NULL);
    mulib_allocator_t allocator = mulib_memtkx_allocator(tracker);
    mulib_mempool_t *pool;
    assert(tracker);
    pool = mulib_mempool_create(sizeof(int), 16, &allocator);
    assert(pool && mulib_mempool_acquire(pool));
    assert(mulib_memtkx_stats(tracker).live_bytes > 0);
    mulib_mempool_destroy(pool);
    assert(mulib_memtkx_stats(tracker).live_blocks == 0);
    mulib_memtkx_destroy(tracker);
    return 0;
}
