#define MULIB_IMPL_VEC
#define MULIB_IMPL_VECTOR
#include "mulib-vector.h"
#include <assert.h>
int main(void) {
    mulib_vector_t a, b, cross;
    const double x[] = {1, 0, 0}, y[] = {0, 1, 0};
    double value;
    assert(mulib_vector_from(&a, x, 3));
    assert(mulib_vector_from(&b, y, 3));
    assert(mulib_vector_init(&cross, 0));
    assert(mulib_vector_cross(&cross, &a, &b));
    assert(mulib_vector_get(&cross, 2, &value) && value == 1);
    mulib_vector_free(&a);
    mulib_vector_free(&b);
    mulib_vector_free(&cross);
    return 0;
}
