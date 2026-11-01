#include <assert.h>
#define MULIB_IMPL_TASKPOOL
#define MULIB_IMPL_PARALLEL
#include "mulib-parallel.h"
static int square(size_t i, const void *in, void *out, void *context) {
    int value = *(const int *)in;
    (void)i; (void)context;
    *(int *)out = value * value; return 0;
}
int main(void) {
    const int input[] = {1,2,3,4};
    int output[4], error;
    mulib_taskpool_t *pool;
    assert(!mulib_taskpool_create(&pool, 2, 2));
    assert(!mulib_parallel_map(pool, input, sizeof(int), output, sizeof(int), 4, 2,
                               square, NULL, NULL, &error));
    assert(!error && output[0] == 1 && output[3] == 16);
    mulib_taskpool_destroy(pool, 0); return 0;
}
