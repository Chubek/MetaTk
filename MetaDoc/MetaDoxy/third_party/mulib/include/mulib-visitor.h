/** @file mulib-visitor.h
 * @brief Controllable preorder AST traversal. Define MULIB_IMPL_VISITOR.
 */
#ifndef MULIB_VISITOR_H
#define MULIB_VISITOR_H
#include "mulib-ast.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_visit_result { MULIB_VISIT_STOP, MULIB_VISIT_DESCEND, MULIB_VISIT_SKIP } mulib_visit_result_t;
typedef mulib_visit_result_t (*mulib_visitor_fn)(const mulib_ast_t *node, void *user);
/** Returns 1 complete, 0 stopped, -1 invalid callback/result. */
int mulib_visitor_walk(const mulib_ast_t *root, mulib_visitor_fn visit, void *user);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VISITOR) && !defined(MULIB_VISITOR_IMPLEMENTED)
#define MULIB_VISITOR_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_visitor_walk(const mulib_ast_t *root, mulib_visitor_fn f, void *u) {
    const mulib_ast_t *n = root;
    if (!root || !f) return -1;
    for (;;) {
        mulib_visit_result_t r = f(n, u);
        if (r == MULIB_VISIT_STOP) return 0;
        if (r != MULIB_VISIT_DESCEND && r != MULIB_VISIT_SKIP) return -1;
        if (r == MULIB_VISIT_DESCEND && n->child) { n = n->child; continue; }
        while (n != root && !n->next) n = n->parent;
        if (n == root) return 1;
        n = n->next;
    }
}
#ifdef __cplusplus
}
#endif
#endif
