/** @file mulib-evtstream.h
 * @brief Allocation-free pull stream of balanced AST enter/leave events.
 * Define MULIB_IMPL_EVTSTREAM. Trees remain borrowed and must not be mutated.
 */
#ifndef MULIB_EVTSTREAM_H
#define MULIB_EVTSTREAM_H
#include "mulib-ast.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_event_kind { MULIB_EVENT_ENTER, MULIB_EVENT_LEAVE } mulib_event_kind_t;
typedef struct mulib_event { mulib_event_kind_t kind; const mulib_ast_t *node; size_t depth; } mulib_event_t;
typedef struct mulib_evtstream {
    const mulib_ast_t *root, *node;
    size_t depth;
    int leaving;
} mulib_evtstream_t;
mulib_evtstream_t mulib_evtstream_from_ast(const mulib_ast_t *root);
/** Returns 1 for an event, 0 at end, -1 for invalid arguments. */
int mulib_evtstream_next(mulib_evtstream_t *stream, mulib_event_t *event);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EVTSTREAM) && !defined(MULIB_EVTSTREAM_IMPLEMENTED)
#define MULIB_EVTSTREAM_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
mulib_evtstream_t mulib_evtstream_from_ast(const mulib_ast_t *root) {
    mulib_evtstream_t s;
    s.root = s.node = root; s.depth = 0; s.leaving = 0; return s;
}
int mulib_evtstream_next(mulib_evtstream_t *s, mulib_event_t *e) {
    if (!s || !e) return -1;
    if (!s->node) return 0;
    e->node = s->node; e->depth = s->depth;
    e->kind = s->leaving ? MULIB_EVENT_LEAVE : MULIB_EVENT_ENTER;
    if (!s->leaving) {
        if (s->node->child) { s->node = s->node->child; ++s->depth; }
        else s->leaving = 1;
    } else if (s->node == s->root) s->node = NULL;
    else if (s->node->next) { s->node = s->node->next; s->leaving = 0; }
    else { s->node = s->node->parent; --s->depth; }
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif
