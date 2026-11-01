/* cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/atn.c -o ./atn */
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BITSET
#define MULIB_IMPL_ATN
#include "mulib-atn.h"
#include <assert.h>

int main(void) {
    mulib_atn_t *a = mulib_atn_create(0, 0);
    size_t start, digits, end, edge;
    uint32_t good[] = {'1', '2', '3'}, bad[] = {'1', 'x'};
    int accepted;
    assert(a);
    assert(mulib_atn_add_state(a, 0, &start) == MULIB_ATN_OK);
    assert(mulib_atn_add_state(a, 0, &digits) == MULIB_ATN_OK);
    assert(mulib_atn_add_state(a, 1, &end) == MULIB_ATN_OK);
    assert(mulib_atn_add_epsilon(a, start, digits, &edge) == MULIB_ATN_OK);
    assert(mulib_atn_add_epsilon(a, digits, start, &edge) == MULIB_ATN_OK);
    assert(mulib_atn_add_range(a, digits, end, '0', '9', &edge) == MULIB_ATN_OK);
    assert(mulib_atn_add_range(a, end, end, '0', '9', &edge) == MULIB_ATN_OK);
    assert(mulib_atn_match(a, start, good, 3, 0, &accepted, NULL) == MULIB_ATN_OK && accepted);
    assert(mulib_atn_match(a, start, bad, 2, 0, &accepted, NULL) == MULIB_ATN_OK && !accepted);
    assert(mulib_atn_match(a, start, NULL, 0, 0, &accepted, NULL) == MULIB_ATN_OK && !accepted);
    mulib_atn_destroy(a);
    return 0;
}
