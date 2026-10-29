/* cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/sppf.c -o ./sppf */
#define MULIB_IMPL_VEC
#define MULIB_IMPL_SPPF
#include "mulib-sppf.h"
#include <assert.h>

int main(void) {
    mulib_sppf_t *f = mulib_sppf_create(0, 0, 0);
    size_t leaf, root, alt, again, count;
    int created;
    assert(f);
    assert(mulib_sppf_intern(f, MULIB_SPPF_TERMINAL, 'a', 0, 1, &leaf, NULL) == MULIB_SPPF_OK);
    assert(mulib_sppf_intern(f, MULIB_SPPF_NONTERMINAL, 'S', 0, 1, &root, NULL) == MULIB_SPPF_OK);
    assert(mulib_sppf_pack(f, root, 0, &leaf, 1, &alt, NULL) == MULIB_SPPF_OK);
    assert(mulib_sppf_pack(f, root, 1, &leaf, 1, &alt, NULL) == MULIB_SPPF_OK);
    assert(mulib_sppf_pack(f, root, 1, &leaf, 1, &again, &created) == MULIB_SPPF_OK && !created && alt == again);
    assert(mulib_sppf_count(f, root, SIZE_MAX, 0, &count) == MULIB_SPPF_OK && count == 2);
    assert(mulib_sppf_node_count(f) == 2 && mulib_sppf_alternative_count(f) == 2);
    mulib_sppf_destroy(f);
    return 0;
}
