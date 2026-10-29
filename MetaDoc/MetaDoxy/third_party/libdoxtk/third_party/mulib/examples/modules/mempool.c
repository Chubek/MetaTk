#define MULIB_IMPL_PORT
#define MULIB_IMPL_ALLOC
#define MULIB_IMPL_MEMPOOL
#include "mulib-mempool.h"
#include <assert.h>
int main(void) {
    mulib_mempool_t *pool = mulib_mempool_create(sizeof(double), 8, NULL);
    double *number;
    assert(pool);
    number = (double *)mulib_mempool_acquire(pool);
    assert(number);
    *number = 42;
    assert(mulib_mempool_in_use(pool) == 1);
    assert(mulib_mempool_release(pool, number));
    assert(!mulib_mempool_release(pool, number));
    mulib_mempool_destroy(pool);
    return 0;
}
