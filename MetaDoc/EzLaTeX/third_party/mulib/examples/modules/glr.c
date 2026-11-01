/* cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/glr.c -o ./glr */
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BITSET
#define MULIB_IMPL_ATN
#define MULIB_IMPL_GSS
#define MULIB_IMPL_SPPF
#define MULIB_IMPL_GLR
#include "mulib-glr.h"
#include <assert.h>

int main(void) {
    mulib_glr_t *g = mulib_glr_create(NULL);
    mulib_glr_result_t r;
    size_t s, a, rule, rhs[2], count;
    uint32_t input[] = {'a', 'a', 'a'}, bad[] = {'a', 'b'};
    assert(g);
    assert(mulib_glr_nonterminal(g, &s) == MULIB_GLR_OK);
    assert(mulib_glr_terminal(g, 'a', 'a', &a) == MULIB_GLR_OK);
    rhs[0] = s; rhs[1] = s;
    assert(mulib_glr_production(g, s, rhs, 2, &rule) == MULIB_GLR_OK); /* S -> SS */
    assert(mulib_glr_production(g, s, &a, 1, &rule) == MULIB_GLR_OK); /* S -> a */
    assert(mulib_glr_compile(g, s) == MULIB_GLR_OK);
    mulib_glr_result_init(&r);
    assert(mulib_glr_parse(g, input, 3, &r) == MULIB_GLR_OK);
    assert(mulib_sppf_count(r.forest, r.root, SIZE_MAX, 0, &count) == MULIB_SPPF_OK && count == 2);
    assert(mulib_gss_edge_count(r.gss) > 0);
    mulib_glr_result_free(&r);
    assert(mulib_glr_parse(g, bad, 2, &r) == MULIB_GLR_SYNTAX && r.position == 1);
    mulib_glr_result_free(&r);
    mulib_glr_destroy(g);
    return 0;
}
