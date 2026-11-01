#define MULIB_IMPL_DYNCALL
#include "mulib-dyncall.h"
#include <assert.h>
#include <stddef.h>

static int64_t add(int64_t a, int64_t b) { return a + b; }
static double average(double a, double b) { return (a + b) / 2.0; }

int main(void)
{
    int64_t integer;
    double real;
    assert(mulib_dyncall_i64_2(add, 20, 22, &integer) == 0 && integer == 42);
    assert(mulib_dyncall_f64_2(average, 1.0, 2.0, &real) == 0 && real == 1.5);
    assert(mulib_dyncall_i64_2(NULL, 1, 2, &integer) == -1 && integer == 42);
    return 0;
}
