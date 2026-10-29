/**
 * @file mulib-gss.h
 * @brief Interned graph-structured stack nodes and labelled predecessor edges.
 *
 * Nodes share (state, input-position) keys. Edges share (head, predecessor,
 * label) keys; same-position cycles are permitted. IDs survive vector growth.
 * Define MULIB_IMPL_GSS in one translation unit and also provide MULIB_IMPL_VEC.
 * See manual/mulib-gss.md for ownership, limits, and iteration rules.
 */
#ifndef MULIB_GSS_H
#define MULIB_GSS_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-vec.h"
#ifdef __cplusplus
extern "C" {
#endif

/** Sentinel for missing node/edge IDs. */
#define MULIB_GSS_NONE SIZE_MAX
/** Operation results; unsuccessful insertion leaves the graph unchanged. */
typedef enum mulib_gss_status {
    MULIB_GSS_OK = 0, MULIB_GSS_INVALID = -1,
    MULIB_GSS_NOMEM = -2, MULIB_GSS_LIMIT = -3
} mulib_gss_status_t;
/** Opaque, owning graph. Not internally synchronized. */
typedef struct mulib_gss mulib_gss_t;
/** A stack top and the first outgoing predecessor edge, or NONE. */
typedef struct mulib_gss_node {
    size_t state, position, first_edge;
} mulib_gss_node_t;
/** A directed head -> predecessor edge. Label is an application-owned ID. */
typedef struct mulib_gss_edge {
    size_t head, predecessor, label, next;
} mulib_gss_edge_t;

/** Create a graph; zero limits select 65536 nodes and 262144 edges. */
mulib_gss_t *mulib_gss_create(size_t max_nodes, size_t max_edges);
/** Release the graph; NULL is allowed. Labels are not destroyed. */
void mulib_gss_destroy(mulib_gss_t *graph);
/** Remove all nodes and edges, retaining capacity. Invalidates all IDs. */
void mulib_gss_clear(mulib_gss_t *graph);
/** Find/create a key. out_id is required; created may be NULL. */
int mulib_gss_intern(mulib_gss_t *graph, size_t state, size_t position,
                     size_t *out_id, int *created);
/**
 * Find/create an edge. Both nodes must exist, predecessor.position must not
 * exceed head.position, and label must not be NONE. out_id is required.
 */
int mulib_gss_link(mulib_gss_t *graph, size_t head, size_t predecessor,
                   size_t label, size_t *out_id, int *created);
/** Copy a node, returning INVALID for an absent ID or NULL output. */
int mulib_gss_node(const mulib_gss_t *graph, size_t id, mulib_gss_node_t *out);
/** Copy an edge; follow next to iterate a node's predecessor edges. */
int mulib_gss_edge(const mulib_gss_t *graph, size_t id, mulib_gss_edge_t *out);
/** Number of interned nodes (zero for NULL). */
size_t mulib_gss_node_count(const mulib_gss_t *graph);
/** Number of interned edges (zero for NULL). */
size_t mulib_gss_edge_count(const mulib_gss_t *graph);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_GSS_H */

#if defined(MULIB_IMPL_GSS) && !defined(MULIB_GSS_IMPLEMENTED)
#define MULIB_GSS_IMPLEMENTED
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_gss {
    mulib_vec_t nodes, edges;
    size_t max_nodes, max_edges;
};

mulib_gss_t *mulib_gss_create(size_t max_nodes, size_t max_edges) {
    mulib_gss_t *g = (mulib_gss_t *)malloc(sizeof(*g));
    if (!g) return NULL;
    mulib_vec_init(&g->nodes, sizeof(mulib_gss_node_t));
    mulib_vec_init(&g->edges, sizeof(mulib_gss_edge_t));
    g->max_nodes = max_nodes ? max_nodes : 65536;
    g->max_edges = max_edges ? max_edges : 262144;
    return g;
}

void mulib_gss_clear(mulib_gss_t *g) {
    if (g) { mulib_vec_clear(&g->nodes); mulib_vec_clear(&g->edges); }
}

void mulib_gss_destroy(mulib_gss_t *g) {
    if (!g) return;
    mulib_vec_free(&g->nodes);
    mulib_vec_free(&g->edges);
    free(g);
}

int mulib_gss_intern(mulib_gss_t *g, size_t state, size_t position,
                     size_t *out_id, int *created) {
    size_t i;
    mulib_gss_node_t node;
    if (created) *created = 0;
    if (!g || !out_id) return MULIB_GSS_INVALID;
    for (i = 0; i < g->nodes.length; ++i) {
        const mulib_gss_node_t *n = (const mulib_gss_node_t *)
            (g->nodes.data + i * sizeof(*n));
        if (n->state == state && n->position == position) {
            *out_id = i;
            return MULIB_GSS_OK;
        }
    }
    if (g->nodes.length >= g->max_nodes || g->nodes.length >= SIZE_MAX / sizeof(node))
        return MULIB_GSS_LIMIT;
    node.state = state; node.position = position; node.first_edge = MULIB_GSS_NONE;
    if (!mulib_vec_push(&g->nodes, &node)) return MULIB_GSS_NOMEM;
    *out_id = g->nodes.length - 1;
    if (created) *created = 1;
    return MULIB_GSS_OK;
}

int mulib_gss_link(mulib_gss_t *g, size_t head, size_t predecessor,
                   size_t label, size_t *out_id, int *created) {
    mulib_gss_node_t *h;
    const mulib_gss_node_t *p;
    mulib_gss_edge_t edge;
    size_t i;
    if (created) *created = 0;
    if (!g || !out_id || head >= g->nodes.length ||
        predecessor >= g->nodes.length || label == MULIB_GSS_NONE)
        return MULIB_GSS_INVALID;
    h = (mulib_gss_node_t *)mulib_vec_at(&g->nodes, head);
    p = (const mulib_gss_node_t *)mulib_vec_at(&g->nodes, predecessor);
    if (p->position > h->position) return MULIB_GSS_INVALID;
    for (i = h->first_edge; i != MULIB_GSS_NONE; ) {
        const mulib_gss_edge_t *e = (const mulib_gss_edge_t *)
            (g->edges.data + i * sizeof(*e));
        if (e->predecessor == predecessor && e->label == label) {
            *out_id = i;
            return MULIB_GSS_OK;
        }
        i = e->next;
    }
    if (g->edges.length >= g->max_edges || g->edges.length >= SIZE_MAX / sizeof(edge))
        return MULIB_GSS_LIMIT;
    edge.head = head; edge.predecessor = predecessor;
    edge.label = label; edge.next = h->first_edge;
    if (!mulib_vec_push(&g->edges, &edge)) return MULIB_GSS_NOMEM;
    h->first_edge = g->edges.length - 1;
    *out_id = h->first_edge;
    if (created) *created = 1;
    return MULIB_GSS_OK;
}

int mulib_gss_node(const mulib_gss_t *g, size_t id, mulib_gss_node_t *out) {
    if (!g || !out || id >= g->nodes.length) return MULIB_GSS_INVALID;
    *out = *(const mulib_gss_node_t *)(g->nodes.data + id * sizeof(*out));
    return MULIB_GSS_OK;
}

int mulib_gss_edge(const mulib_gss_t *g, size_t id, mulib_gss_edge_t *out) {
    if (!g || !out || id >= g->edges.length) return MULIB_GSS_INVALID;
    *out = *(const mulib_gss_edge_t *)(g->edges.data + id * sizeof(*out));
    return MULIB_GSS_OK;
}

size_t mulib_gss_node_count(const mulib_gss_t *g) { return g ? g->nodes.length : 0; }
size_t mulib_gss_edge_count(const mulib_gss_t *g) { return g ? g->edges.length : 0; }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
