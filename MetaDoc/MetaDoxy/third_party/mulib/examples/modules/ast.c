#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#include "mulib-ast.h"
#include <assert.h>
#include <stdlib.h>
int main(void) {
    mulib_ast_t *root = mulib_ast_create("sum", "1+2", 3);
    mulib_ast_t *number = mulib_ast_create("number", "1", 1);
    char *json;
    assert(root && number && mulib_ast_append(root, number));
    json = mulib_ast_render(root, MULIB_AST_JSON); assert(json);
    free(json); mulib_ast_destroy(root); return 0;
}
