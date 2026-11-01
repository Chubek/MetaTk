#include <assert.h>
#include <stdatomic.h>
#define MULIB_IMPL_TASKPOOL
#include "mulib-taskpool.h"
static void increment(void *p) { (void)atomic_fetch_add((atomic_int *)p, 1); }
int main(void) {
    mulib_taskpool_t *pool;
    atomic_int count;
    atomic_init(&count, 0);
    assert(!mulib_taskpool_create(&pool, 2, 4));
    assert(!mulib_taskpool_submit(pool, increment, &count, NULL));
    assert(!mulib_taskpool_submit(pool, increment, &count, NULL));
    assert(!mulib_taskpool_wait(pool));
    assert(atomic_load(&count) == 2);
    mulib_taskpool_destroy(pool, 0);
    return 0;
}
