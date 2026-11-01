#define MULIB_IMPL_LAZYSTREAM
#include "mulib-lazystream.h"
#include <assert.h>
static int sum(void *accumulator, void *value, void *context) {
    (void)context; *(int *)accumulator += *(int *)value; return 1;
}
int main(void) {
    int a = 1, b = 2, c = 3, total = 0;
    void *values[] = {&a, &b, &c};
    mulib_lazystream_t *stream = mulib_lazystream_array(values, 3);
    assert(stream);
    stream = mulib_lazystream_take(stream, 2); assert(stream);
    assert(mulib_lazystream_fold(stream, &total, sum, NULL) == 1 && total == 3);
    mulib_lazystream_destroy(stream); return 0;
}
