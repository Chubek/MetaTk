#include <assert.h>
#define MULIB_IMPL_TASKPOOL
#define MULIB_IMPL_TASKDAG
#include "mulib-taskdag.h"
static int prepare(void *p) { *(int *)p = 20; return 0; }
static int consume(void *p) { assert(*(int *)p == 20); *(int *)p += 22; return 0; }
int main(void) {
    mulib_taskpool_t *pool;
    mulib_taskdag_t *dag;
    size_t first, second;
    int value = 0;
    assert(!mulib_taskpool_create(&pool, 2, 2));
    assert(!mulib_taskdag_create(&dag, 2, 1));
    assert(!mulib_taskdag_add(dag, prepare, &value, &first));
    assert(!mulib_taskdag_add(dag, consume, &value, &second));
    assert(!mulib_taskdag_depend(dag, first, second));
    assert(!mulib_taskdag_run(dag, pool, NULL) && value == 42);
    mulib_taskdag_destroy(dag); mulib_taskpool_destroy(pool, 0);
    return 0;
}
