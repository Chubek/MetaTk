/**
 * @file mulib-atn.h
 * @brief Epsilon/range nondeterministic finite automata over uint32_t tokens.
 *
 * This is a finite transition network, not a recursive grammar interpreter.
 * Epsilon closures visit each state once and safely handle cycles. Define
 * MULIB_IMPL_ATN and provide MULIB_IMPL_VEC and MULIB_IMPL_BITSET. See the manual
 * for subset stepping, work accounting, and resource limits.
 */
#ifndef MULIB_ATN_H
#define MULIB_ATN_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-vec.h"
#include "mulib-bitset.h"
#ifdef __cplusplus
extern "C" {
#endif

/** Missing state or transition ID. */
#define MULIB_ATN_NONE SIZE_MAX
/** Operation results; a nonmatch is OK with accepted == 0. */
typedef enum mulib_atn_status {
    MULIB_ATN_OK = 0, MULIB_ATN_INVALID = -1,
    MULIB_ATN_NOMEM = -2, MULIB_ATN_LIMIT = -3
} mulib_atn_status_t;
/** Owning transition graph. State/edge IDs are stable until destruction. */
typedef struct mulib_atn mulib_atn_t;
/** State information, copied by state(). */
typedef struct mulib_atn_state { int accepting; size_t first_edge; } mulib_atn_state_t;
/** Transition; lower/upper apply only when epsilon == 0. */
typedef struct mulib_atn_edge {
    size_t from, to, next;
    uint32_t lower, upper;
    int epsilon;
} mulib_atn_edge_t;

/** Create an empty graph; zero limits select 65536 states and 262144 edges. */
mulib_atn_t *mulib_atn_create(size_t max_states, size_t max_edges);
/** Release a graph; NULL is allowed. */
void mulib_atn_destroy(mulib_atn_t *atn);
/** Append a state. out_id is required; failure leaves the graph unchanged. */
int mulib_atn_add_state(mulib_atn_t *atn, int accepting, size_t *out_id);
/** Change a state's accepting flag. */
int mulib_atn_set_accepting(mulib_atn_t *atn, size_t state, int accepting);
/** Append an epsilon transition; parallel transitions are allowed. */
int mulib_atn_add_epsilon(mulib_atn_t *atn, size_t from, size_t to, size_t *out_id);
/** Append an inclusive uint32 range transition; lower must not exceed upper. */
int mulib_atn_add_range(mulib_atn_t *atn, size_t from, size_t to,
                       uint32_t lower, uint32_t upper, size_t *out_id);
/** Copy a state record, returning INVALID for missing IDs/NULL outputs. */
int mulib_atn_state(const mulib_atn_t *atn, size_t id, mulib_atn_state_t *out);
/** Copy an edge record; next iterates a state's outgoing transitions. */
int mulib_atn_edge(const mulib_atn_t *atn, size_t id, mulib_atn_edge_t *out);
/** Number of states (zero for NULL). */
size_t mulib_atn_state_count(const mulib_atn_t *atn);
/** Number of transitions (zero for NULL). */
size_t mulib_atn_edge_count(const mulib_atn_t *atn);
/**
 * Expand an initialized state bitset in place. Its bit count must equal the
 * graph's state count. max_steps counts visited states and inspected edges;
 * zero selects 10000000. Optional work receives the amount spent, also on
 * failure. On failure the set may be partially expanded.
 */
int mulib_atn_closure(const mulib_atn_t *atn, mulib_bitset_t *states,
                      size_t max_steps, size_t *work);
/**
 * Move an epsilon-closed input set by one token, then take epsilon closure.
 * Both bitsets must be initialized and match the state count, with distinct
 * backing storage. Output is cleared first and may be partial on failure.
 */
int mulib_atn_step(const mulib_atn_t *atn, const mulib_bitset_t *from,
                   uint32_t token, mulib_bitset_t *to,
                   size_t max_steps, size_t *work);
/**
 * Full-input match from one state. NULL tokens is valid only for length zero.
 * Empty input is accepted iff the start's epsilon closure contains an accepting
 * state. accepted is required and is zero on errors. Budget covers the entire
 * match. This function does not mutate the graph.
 */
int mulib_atn_match(const mulib_atn_t *atn, size_t start,
                    const uint32_t *tokens, size_t length, size_t max_steps,
                    int *accepted, size_t *work);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ATN_H */

#if defined(MULIB_IMPL_ATN) && !defined(MULIB_ATN_IMPLEMENTED)
#define MULIB_ATN_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_atn { mulib_vec_t states, edges; size_t max_states, max_edges; };

static const mulib_atn_state_t *mulib_atn_state_(const mulib_atn_t *a, size_t i) {
    return (const mulib_atn_state_t *)(a->states.data + i * sizeof(mulib_atn_state_t));
}
static const mulib_atn_edge_t *mulib_atn_edge_(const mulib_atn_t *a, size_t i) {
    return (const mulib_atn_edge_t *)(a->edges.data + i * sizeof(mulib_atn_edge_t));
}
static int mulib_atn_tick_(size_t *work, size_t limit) {
    if (*work >= limit) return 0;
    ++*work; return 1;
}
static int mulib_atn_set_valid_(const mulib_atn_t *a, const mulib_bitset_t *set) {
    return a && set && set->bits == a->states.length && set->bytes &&
           set->bits <= SIZE_MAX - 7;
}

mulib_atn_t *mulib_atn_create(size_t states, size_t edges) {
    mulib_atn_t *a = (mulib_atn_t *)malloc(sizeof(*a));
    if (!a) return NULL;
    mulib_vec_init(&a->states, sizeof(mulib_atn_state_t));
    mulib_vec_init(&a->edges, sizeof(mulib_atn_edge_t));
    a->max_states = states ? states : 65536;
    a->max_edges = edges ? edges : 262144;
    return a;
}
void mulib_atn_destroy(mulib_atn_t *a) {
    if (!a) return;
    mulib_vec_free(&a->states); mulib_vec_free(&a->edges); free(a);
}
int mulib_atn_add_state(mulib_atn_t *a, int accepting, size_t *out_id) {
    mulib_atn_state_t state;
    if (!a || !out_id) return MULIB_ATN_INVALID;
    if (a->states.length >= a->max_states ||
        a->states.length >= SIZE_MAX / sizeof(state) || a->states.length >= SIZE_MAX - 7)
        return MULIB_ATN_LIMIT;
    state.accepting = accepting != 0; state.first_edge = MULIB_ATN_NONE;
    if (!mulib_vec_push(&a->states, &state)) return MULIB_ATN_NOMEM;
    *out_id = a->states.length - 1; return MULIB_ATN_OK;
}
int mulib_atn_set_accepting(mulib_atn_t *a, size_t state, int accepting) {
    mulib_atn_state_t *s;
    if (!a || state >= a->states.length) return MULIB_ATN_INVALID;
    s = (mulib_atn_state_t *)mulib_vec_at(&a->states, state);
    s->accepting = accepting != 0; return MULIB_ATN_OK;
}
static int mulib_atn_add_edge_(mulib_atn_t *a, size_t from, size_t to,
                              uint32_t lower, uint32_t upper, int epsilon,
                              size_t *out_id) {
    mulib_atn_state_t *s;
    mulib_atn_edge_t edge;
    if (!a || !out_id || from >= a->states.length || to >= a->states.length || lower > upper)
        return MULIB_ATN_INVALID;
    if (a->edges.length >= a->max_edges || a->edges.length >= SIZE_MAX / sizeof(edge))
        return MULIB_ATN_LIMIT;
    s = (mulib_atn_state_t *)mulib_vec_at(&a->states, from);
    edge.from = from; edge.to = to; edge.next = s->first_edge;
    edge.lower = lower; edge.upper = upper; edge.epsilon = epsilon;
    if (!mulib_vec_push(&a->edges, &edge)) return MULIB_ATN_NOMEM;
    s->first_edge = a->edges.length - 1; *out_id = s->first_edge;
    return MULIB_ATN_OK;
}
int mulib_atn_add_epsilon(mulib_atn_t *a, size_t from, size_t to, size_t *out_id) {
    return mulib_atn_add_edge_(a, from, to, 0, 0, 1, out_id);
}
int mulib_atn_add_range(mulib_atn_t *a, size_t from, size_t to,
                       uint32_t lower, uint32_t upper, size_t *out_id) {
    return mulib_atn_add_edge_(a, from, to, lower, upper, 0, out_id);
}
int mulib_atn_state(const mulib_atn_t *a, size_t id, mulib_atn_state_t *out) {
    if (!a || !out || id >= a->states.length) return MULIB_ATN_INVALID;
    *out = *mulib_atn_state_(a, id); return MULIB_ATN_OK;
}
int mulib_atn_edge(const mulib_atn_t *a, size_t id, mulib_atn_edge_t *out) {
    if (!a || !out || id >= a->edges.length) return MULIB_ATN_INVALID;
    *out = *mulib_atn_edge_(a, id); return MULIB_ATN_OK;
}
size_t mulib_atn_state_count(const mulib_atn_t *a) { return a ? a->states.length : 0; }
size_t mulib_atn_edge_count(const mulib_atn_t *a) { return a ? a->edges.length : 0; }

static int mulib_atn_closure_(const mulib_atn_t *a, mulib_bitset_t *set,
                             size_t limit, size_t *work, mulib_vec_t *queue) {
    size_t i, q;
    mulib_vec_clear(queue);
    for (i = 0; i < a->states.length; ++i) {
        if (!mulib_atn_tick_(work, limit)) return MULIB_ATN_LIMIT;
        if (mulib_bitset_test(set, i) && !mulib_vec_push(queue, &i)) return MULIB_ATN_NOMEM;
    }
    for (q = 0; q < queue->length; ++q) {
        size_t state = ((const size_t *)queue->data)[q];
        for (i = mulib_atn_state_(a, state)->first_edge; i != MULIB_ATN_NONE; ) {
            const mulib_atn_edge_t *edge = mulib_atn_edge_(a, i);
            if (!mulib_atn_tick_(work, limit)) return MULIB_ATN_LIMIT;
            if (edge->epsilon && !mulib_bitset_test(set, edge->to)) {
                if (!mulib_vec_push(queue, &edge->to)) return MULIB_ATN_NOMEM;
                (void)mulib_bitset_set(set, edge->to, 1);
            }
            i = edge->next;
        }
    }
    return MULIB_ATN_OK;
}
static int mulib_atn_step_(const mulib_atn_t *a, const mulib_bitset_t *from,
                          uint32_t token, mulib_bitset_t *to,
                          size_t limit, size_t *work, mulib_vec_t *queue) {
    size_t i, edge_id;
    memset(to->bytes, 0, (to->bits + 7) / 8);
    for (i = 0; i < a->states.length; ++i) {
        if (!mulib_atn_tick_(work, limit)) return MULIB_ATN_LIMIT;
        if (!mulib_bitset_test(from, i)) continue;
        for (edge_id = mulib_atn_state_(a, i)->first_edge; edge_id != MULIB_ATN_NONE; ) {
            const mulib_atn_edge_t *edge = mulib_atn_edge_(a, edge_id);
            if (!mulib_atn_tick_(work, limit)) return MULIB_ATN_LIMIT;
            if (!edge->epsilon && token >= edge->lower && token <= edge->upper)
                (void)mulib_bitset_set(to, edge->to, 1);
            edge_id = edge->next;
        }
    }
    return mulib_atn_closure_(a, to, limit, work, queue);
}
int mulib_atn_closure(const mulib_atn_t *a, mulib_bitset_t *states,
                      size_t max_steps, size_t *out_work) {
    mulib_vec_t queue;
    size_t work = 0;
    int status;
    if (out_work) *out_work = 0;
    if (!mulib_atn_set_valid_(a, states)) return MULIB_ATN_INVALID;
    mulib_vec_init(&queue, sizeof(size_t));
    status = mulib_atn_closure_(a, states, max_steps ? max_steps : 10000000, &work, &queue);
    mulib_vec_free(&queue);
    if (out_work) *out_work = work;
    return status;
}
int mulib_atn_step(const mulib_atn_t *a, const mulib_bitset_t *from,
                   uint32_t token, mulib_bitset_t *to, size_t max_steps, size_t *out_work) {
    mulib_vec_t queue;
    size_t work = 0;
    int status;
    if (out_work) *out_work = 0;
    if (!mulib_atn_set_valid_(a, from) || !mulib_atn_set_valid_(a, to) || from->bytes == to->bytes)
        return MULIB_ATN_INVALID;
    mulib_vec_init(&queue, sizeof(size_t));
    status = mulib_atn_step_(a, from, token, to, max_steps ? max_steps : 10000000, &work, &queue);
    mulib_vec_free(&queue);
    if (out_work) *out_work = work;
    return status;
}
int mulib_atn_match(const mulib_atn_t *a, size_t start, const uint32_t *tokens,
                    size_t length, size_t max_steps, int *accepted, size_t *out_work) {
    mulib_bitset_t current, next;
    mulib_vec_t queue;
    size_t i, work = 0;
    int status;
    if (accepted) *accepted = 0;
    if (out_work) *out_work = 0;
    if (!a || !accepted || start >= a->states.length || (length && !tokens))
        return MULIB_ATN_INVALID;
    if (!max_steps) max_steps = 10000000;
    if (!mulib_bitset_init(&current, a->states.length)) return MULIB_ATN_NOMEM;
    if (!mulib_bitset_init(&next, a->states.length)) {
        mulib_bitset_free(&current); return MULIB_ATN_NOMEM;
    }
    mulib_vec_init(&queue, sizeof(size_t));
    (void)mulib_bitset_set(&current, start, 1);
    status = mulib_atn_closure_(a, &current, max_steps, &work, &queue);
    for (i = 0; status == MULIB_ATN_OK && i < length; ++i) {
        mulib_bitset_t swap;
        status = mulib_atn_step_(a, &current, tokens[i], &next, max_steps, &work, &queue);
        swap = current; current = next; next = swap;
    }
    if (status == MULIB_ATN_OK) for (i = 0; i < a->states.length; ++i) {
        if (!mulib_atn_tick_(&work, max_steps)) { status = MULIB_ATN_LIMIT; break; }
        if (mulib_bitset_test(&current, i) && mulib_atn_state_(a, i)->accepting) {
            *accepted = 1; break;
        }
    }
    if (status != MULIB_ATN_OK) *accepted = 0;
    mulib_vec_free(&queue); mulib_bitset_free(&current); mulib_bitset_free(&next);
    if (out_work) *out_work = work;
    return status;
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
