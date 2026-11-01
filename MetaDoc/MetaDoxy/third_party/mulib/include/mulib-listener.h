/** @file mulib-listener.h
 * @brief Production-specific enter/exit AST callbacks. Define MULIB_IMPL_LISTENER;
 * explicitly implement EVTSTREAM too. Return zero from a callback to stop.
 */
#ifndef MULIB_LISTENER_H
#define MULIB_LISTENER_H
#include "mulib-evtstream.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_listener {
    const char *production; /* NULL matches every production. */
    mulib_ast_action_fn enter, leave;
    void *user;
} mulib_listener_t;
int mulib_listener_walk(const mulib_ast_t *root, const mulib_listener_t *listeners, size_t count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_LISTENER) && !defined(MULIB_LISTENER_IMPLEMENTED)
#define MULIB_LISTENER_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_listener_walk(const mulib_ast_t *root, const mulib_listener_t *a, size_t count) {
    mulib_evtstream_t s = mulib_evtstream_from_ast(root);
    mulib_event_t e;
    size_t i;
    if (!root || (!a && count)) return -1;
    while (mulib_evtstream_next(&s, &e) == 1) for (i = 0; i < count; ++i) {
        mulib_ast_action_fn f = e.kind == MULIB_EVENT_ENTER ? a[i].enter : a[i].leave;
        if (f && (!a[i].production || !strcmp(a[i].production, e.node->production)))
            if (!f(e.node, a[i].user)) return 0;
    }
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif
