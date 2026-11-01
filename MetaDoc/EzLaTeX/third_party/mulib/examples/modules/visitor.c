#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_VISITOR
#include "mulib-visitor.h"
#include <assert.h>
static mulib_visit_result_t count(const mulib_ast_t *node, void *user) {
    (void)node; ++*(int *)user; return MULIB_VISIT_DESCEND;
}
int main(void) {
    int calls = 0;
    mulib_ast_t *root = mulib_ast_create("root", "", 0);
    assert(root && mulib_visitor_walk(root, count, &calls) == 1 && calls == 1);
    mulib_ast_destroy(root); return 0;
}
