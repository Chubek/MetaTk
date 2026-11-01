#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_EVTSTREAM
#define MULIB_IMPL_LISTENER
#include "mulib-listener.h"
#include <assert.h>
static int count(const mulib_ast_t *node, void *user) { (void)node; ++*(int *)user; return 1; }
int main(void) {
    int calls = 0;
    mulib_ast_t *root = mulib_ast_create("root", "", 0);
    mulib_listener_t listener = {"root", count, count, &calls};
    assert(root && mulib_listener_walk(root, &listener, 1) == 1 && calls == 2);
    mulib_ast_destroy(root); return 0;
}
