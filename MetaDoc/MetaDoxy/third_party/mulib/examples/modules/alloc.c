#define MULIB_IMPL_PORT
#define MULIB_IMPL_ALLOC
#include "mulib-alloc.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    void *values = NULL;
    assert(mulib_alloc_resize_array(NULL, &values, 8, sizeof(int)));
    assert(!mulib_alloc_resize_array(NULL, &values, SIZE_MAX, sizeof(int)));
    assert(values);
    assert(mulib_alloc_resize_array(NULL, &values, 0, sizeof(int)));
    assert(!values);
    return 0;
}
