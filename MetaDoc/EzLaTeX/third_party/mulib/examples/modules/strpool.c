#define MULIB_IMPL_PORT
#define MULIB_IMPL_ALLOC
#define MULIB_IMPL_HASH
#define MULIB_IMPL_STRPOOL
#include "mulib-strpool.h"
#include <assert.h>
int main(void) {
    mulib_strpool_t *pool = mulib_strpool_create(NULL);
    const char *a, *b;
    assert(pool);
    a = mulib_strpool_intern(pool, "hello");
    b = mulib_strpool_intern_n(pool, "hello!", 5);
    assert(a && a == b && mulib_strpool_size(pool) == 1);
    mulib_strpool_destroy(pool);
    return 0;
}
