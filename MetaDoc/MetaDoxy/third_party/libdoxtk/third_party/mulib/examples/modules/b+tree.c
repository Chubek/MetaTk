#include <assert.h>
#define MULIB_IMPL_BPTREE
#include "mulib-b+tree.h"
static int visit(int64_t key, uint64_t value, void *ctx) {
    size_t *count = (size_t *)ctx;
    assert(key >= 40 && key <= 50); assert(value == (uint64_t)key + 1);
    ++*count; return 1;
}
int main(void) {
    mulib_bptree_t tree; size_t count = 0; uint64_t value;
    mulib_bptree_init(&tree);
    for (int64_t i = 0; i < 1000; ++i) assert(mulib_bptree_set(&tree, i, (uint64_t)i + 1));
    assert(mulib_bptree_get(&tree, 42, &value) && value == 43);
    assert(mulib_bptree_range(&tree, 40, 50, visit, &count) == 11 && count == 11);
    for (int64_t i = 0; i < 1000; ++i) assert(mulib_bptree_remove(&tree, i, NULL));
    assert(mulib_bptree_validate(&tree) && tree.size == 0);
    mulib_bptree_free(&tree); return 0;
}
