/* cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/gss.c -o ./gss */
#define MULIB_IMPL_VEC
#define MULIB_IMPL_GSS
#include "mulib-gss.h"
#include <assert.h>

int main(void) {
    mulib_gss_t *g = mulib_gss_create(0, 0);
    size_t base, left, right, shared, again, edge;
    mulib_gss_node_t n;
    int created;
    assert(g);
    assert(mulib_gss_intern(g, 0, 0, &base, NULL) == MULIB_GSS_OK);
    assert(mulib_gss_intern(g, 1, 1, &left, NULL) == MULIB_GSS_OK);
    assert(mulib_gss_intern(g, 2, 1, &right, NULL) == MULIB_GSS_OK);
    assert(mulib_gss_intern(g, 3, 2, &shared, NULL) == MULIB_GSS_OK);
    assert(mulib_gss_intern(g, 3, 2, &again, &created) == MULIB_GSS_OK && !created && again == shared);
    assert(mulib_gss_link(g, left, base, 10, &edge, NULL) == MULIB_GSS_OK);
    assert(mulib_gss_link(g, right, base, 11, &edge, NULL) == MULIB_GSS_OK);
    assert(mulib_gss_link(g, shared, left, 12, &edge, NULL) == MULIB_GSS_OK);
    assert(mulib_gss_link(g, shared, right, 12, &edge, NULL) == MULIB_GSS_OK);
    assert(mulib_gss_link(g, shared, right, 12, &again, &created) == MULIB_GSS_OK && !created && again == edge);
    assert(mulib_gss_node(g, shared, &n) == MULIB_GSS_OK && n.first_edge == edge);
    assert(mulib_gss_node_count(g) == 4 && mulib_gss_edge_count(g) == 4);
    mulib_gss_destroy(g);
    return 0;
}
