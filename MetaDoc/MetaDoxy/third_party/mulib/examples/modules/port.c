#define MULIB_IMPL_PORT
#include "mulib-port.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    size_t n = 99;
    assert(mulib_port_mul_size(7, 6, &n) && n == 42);
    assert(!mulib_port_add_size(SIZE_MAX, 1, &n) && n == 42);
    assert(mulib_port_align_size(10, 3, &n) && n == 12);
    assert(mulib_port_os() && mulib_port_cpu());
    return 0;
}
