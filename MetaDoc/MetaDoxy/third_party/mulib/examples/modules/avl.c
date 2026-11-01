#include <assert.h>
#include <stdint.h>
#define MULIB_IMPL_AVL
#include "mulib-avl.h"
static int visit(int64_t key, uint64_t value, void *ctx) {
    int64_t *previous = (int64_t *)ctx;
    assert(key > *previous); assert(value == (uint64_t)key * 10);
    *previous = key; return 1;
}
int main(void) {
    mulib_avl_t tree; uint64_t value; int64_t previous = -1;
    mulib_avl_init(&tree);
    for (int64_t i = 100; i-- > 0;) assert(mulib_avl_set(&tree, i, (uint64_t)i * 10));
    assert(mulib_avl_validate(&tree));
    assert(mulib_avl_get(&tree, 42, &value) && value == 420);
    assert(mulib_avl_remove(&tree, 42, &value) && value == 420);
    assert(!mulib_avl_get(&tree, 42, NULL));
    assert(mulib_avl_foreach(&tree, visit, &previous) == 99);
    assert(mulib_avl_validate(&tree)); mulib_avl_free(&tree);
    return 0;
}
