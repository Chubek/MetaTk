/* Standalone assertions for ATN, GSS, SPPF and genuine generalized LR parsing. */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

/* Deterministic allocation-failure injection, confined to this translation unit. */
static size_t grammar_allocations_left = SIZE_MAX;
static int grammar_allocation_allowed(void) {
    if (grammar_allocations_left == SIZE_MAX) return 1;
    if (!grammar_allocations_left) return 0;
    --grammar_allocations_left;
    return 1;
}
static void *grammar_test_malloc(size_t n) {
    return grammar_allocation_allowed() ? malloc(n) : NULL;
}
static void *grammar_test_calloc(size_t n, size_t stride) {
    return grammar_allocation_allowed() ? calloc(n, stride) : NULL;
}
static void *grammar_test_realloc(void *p, size_t n) {
    return grammar_allocation_allowed() ? realloc(p, n) : NULL;
}
#define malloc grammar_test_malloc
#define calloc grammar_test_calloc
#define realloc grammar_test_realloc
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BITSET
#define MULIB_IMPL_ATN
#define MULIB_IMPL_GSS
#define MULIB_IMPL_SPPF
#define MULIB_IMPL_GLR
#include "mulib-glr.h"
#undef malloc
#undef calloc
#undef realloc

static size_t grammar_nt(mulib_glr_t *g) {
    size_t id;
    assert(mulib_glr_nonterminal(g, &id) == MULIB_GLR_OK);
    return id;
}
static size_t grammar_term(mulib_glr_t *g, uint32_t lo, uint32_t hi) {
    size_t id;
    assert(mulib_glr_terminal(g, lo, hi, &id) == MULIB_GLR_OK);
    return id;
}
static void grammar_rule(mulib_glr_t *g, size_t lhs, const size_t *rhs, size_t n) {
    size_t id;
    assert(mulib_glr_production(g, lhs, rhs, n, &id) == MULIB_GLR_OK);
}
static void grammar_expect(mulib_glr_t *g, const uint32_t *input, size_t n, size_t expected) {
    mulib_glr_result_t r;
    mulib_sppf_node_t root;
    size_t count;
    mulib_glr_result_init(&r);
    assert(mulib_glr_parse(g, input, n, &r) == MULIB_GLR_OK);
    assert(r.position == n && r.root != MULIB_GLR_NONE);
    assert(mulib_sppf_node(r.forest, r.root, &root) == MULIB_SPPF_OK);
    assert(root.begin == 0 && root.end == n);
    assert(mulib_sppf_count(r.forest, r.root, SIZE_MAX, 0, &count) == MULIB_SPPF_OK);
    assert(count == expected);
    mulib_glr_result_free(&r);
    assert(!r.forest && !r.gss && r.root == MULIB_GLR_NONE);
}

static void test_atn(void) {
    mulib_atn_t *a = mulib_atn_create(4, 5);
    mulib_bitset_t set, next;
    mulib_atn_edge_t e;
    size_t p, q, r, s, edge, ignored, work;
    uint32_t good[] = {0, UINT32_MAX}, bad[] = {1, UINT32_MAX};
    int accepted;
    assert(a);
    assert(mulib_atn_add_state(a, 0, &p) == 0);
    assert(mulib_atn_add_state(a, 0, &q) == 0);
    assert(mulib_atn_add_state(a, 0, &r) == 0);
    assert(mulib_atn_add_state(a, 1, &s) == 0);
    assert(mulib_atn_add_state(a, 1, &ignored) == MULIB_ATN_LIMIT);
    assert(mulib_atn_add_epsilon(a, p, q, &edge) == 0);
    assert(mulib_atn_add_epsilon(a, q, p, &edge) == 0);
    assert(mulib_atn_add_range(a, q, r, 0, 0, &edge) == 0);
    assert(mulib_atn_add_range(a, r, s, UINT32_MAX, UINT32_MAX, &edge) == 0);
    assert(mulib_atn_add_range(a, s, s, 9, 8, &edge) == MULIB_ATN_INVALID);
    assert(mulib_atn_add_epsilon(a, s, s, &edge) == 0);
    assert(mulib_atn_add_epsilon(a, s, s, &edge) == MULIB_ATN_LIMIT);
    assert(mulib_atn_edge(a, edge, &e) == 0 && e.epsilon);
    assert(mulib_atn_match(a, p, good, 2, 0, &accepted, &work) == 0 && accepted && work);
    assert(mulib_atn_match(a, p, bad, 2, 0, &accepted, NULL) == 0 && !accepted);
    assert(mulib_atn_match(a, p, NULL, 0, 0, &accepted, NULL) == 0 && !accepted);
    assert(mulib_atn_match(a, p, good, 2, 1, &accepted, &work) == MULIB_ATN_LIMIT && !accepted && work == 1);
    assert(mulib_atn_match(a, p, NULL, 1, 0, &accepted, NULL) == MULIB_ATN_INVALID);
    assert(mulib_bitset_init(&set, 4) && mulib_bitset_init(&next, 4));
    assert(mulib_bitset_set(&set, p, 1));
    assert(mulib_atn_closure(a, &set, 0, NULL) == 0);
    assert(mulib_bitset_count(&set) == 2);
    assert(mulib_atn_step(a, &set, 0, &next, 0, NULL) == 0);
    assert(mulib_bitset_count(&next) == 1 && mulib_bitset_test(&next, r));
    assert(mulib_atn_step(a, &set, 0, &set, 0, NULL) == MULIB_ATN_INVALID);
    assert(mulib_atn_set_accepting(a, q, 1) == 0);
    assert(mulib_atn_match(a, p, NULL, 0, 0, &accepted, NULL) == 0 && accepted);
    mulib_bitset_free(&set); mulib_bitset_free(&next); mulib_atn_destroy(a);
}

static void test_gss(void) {
    mulib_gss_t *g = mulib_gss_create(3, 3);
    size_t base, a, b, id, again;
    mulib_gss_edge_t edge;
    int created;
    assert(g);
    assert(mulib_gss_intern(g, 0, 0, &base, &created) == 0 && created);
    assert(mulib_gss_intern(g, 1, 1, &a, NULL) == 0);
    assert(mulib_gss_intern(g, 2, 1, &b, NULL) == 0);
    assert(mulib_gss_intern(g, 1, 1, &again, &created) == 0 && !created && again == a);
    assert(mulib_gss_intern(g, 3, 1, &id, NULL) == MULIB_GSS_LIMIT);
    assert(mulib_gss_link(g, a, base, 0, &id, NULL) == 0);
    assert(mulib_gss_link(g, a, b, 1, &id, NULL) == 0);
    assert(mulib_gss_link(g, b, a, 2, &id, NULL) == 0); /* same-position cycle */
    assert(mulib_gss_link(g, b, a, 2, &again, &created) == 0 && !created && again == id);
    assert(mulib_gss_link(g, base, a, 0, &id, NULL) == MULIB_GSS_INVALID);
    assert(mulib_gss_link(g, a, base, MULIB_GSS_NONE, &id, NULL) == MULIB_GSS_INVALID);
    assert(mulib_gss_link(g, b, base, 3, &id, NULL) == MULIB_GSS_LIMIT);
    assert(mulib_gss_edge(g, again, &edge) == 0 && edge.predecessor == a);
    mulib_gss_clear(g);
    assert(mulib_gss_node_count(g) == 0 && mulib_gss_edge_count(g) == 0);
    mulib_gss_destroy(g);
}

static void test_sppf(void) {
    mulib_sppf_t *f = mulib_sppf_create(5, 6, 8);
    size_t leaf, root, empty, dead, alt, again, count, pair[2];
    mulib_sppf_alternative_t a;
    int created;
    assert(f);
    assert(mulib_sppf_intern(f, MULIB_SPPF_TERMINAL, 1, 0, 1, &leaf, NULL) == 0);
    assert(mulib_sppf_intern(f, MULIB_SPPF_NONTERMINAL, 2, 0, 1, &root, NULL) == 0);
    assert(mulib_sppf_intern(f, MULIB_SPPF_NONTERMINAL, 2, 0, 1, &again, &created) == 0 && !created && again == root);
    assert(mulib_sppf_intern(f, MULIB_SPPF_TERMINAL, 1, 0, 0, &again, NULL) == MULIB_SPPF_INVALID);
    assert(mulib_sppf_intern(f, MULIB_SPPF_NONTERMINAL, 3, 0, 0, &empty, NULL) == 0);
    assert(mulib_sppf_intern(f, MULIB_SPPF_NONTERMINAL, 4, 0, 1, &dead, NULL) == 0);
    assert(mulib_sppf_pack(f, dead, 7, &dead, 1, &alt, NULL) == 0);
    assert(mulib_sppf_count(f, dead, SIZE_MAX, 0, &count) == 0 && count == 0);
    assert(mulib_sppf_pack(f, empty, 0, NULL, 0, &alt, NULL) == 0);
    assert(mulib_sppf_pack(f, root, 1, &leaf, 1, &alt, NULL) == 0);
    assert(mulib_sppf_pack(f, root, 1, &leaf, 1, &again, &created) == 0 && !created && again == alt);
    pair[0] = empty; pair[1] = leaf;
    assert(mulib_sppf_pack(f, root, 2, pair, 2, &alt, NULL) == 0);
    assert(mulib_sppf_pack(f, root, 3, &dead, 1, &alt, NULL) == 0); /* ignored unproductive choice */
    assert(mulib_sppf_count(f, root, SIZE_MAX, 0, &count) == 0 && count == 2);
    assert(mulib_sppf_count(f, root, 1, 0, &count) == MULIB_SPPF_LIMIT && count == 1);
    assert(mulib_sppf_count(f, root, SIZE_MAX, 1, &count) == MULIB_SPPF_LIMIT && count == 0);
    assert(mulib_sppf_alternative(f, alt, &a) == 0 && a.parent == root);
    assert(mulib_sppf_child(f, alt, 0, &again) == 0 && again == dead);
    assert(mulib_sppf_pack(f, root, 4, &root, 1, &alt, NULL) == 0);
    assert(mulib_sppf_count(f, root, SIZE_MAX, 0, &count) == MULIB_SPPF_INFINITE && count == SIZE_MAX);
    assert(mulib_sppf_pack(f, root, 5, &leaf, 1, &alt, NULL) == MULIB_SPPF_LIMIT);
    assert(mulib_sppf_pack(f, leaf, 0, NULL, 0, &alt, NULL) == MULIB_SPPF_INVALID);
    pair[0] = leaf; pair[1] = empty;
    assert(mulib_sppf_pack(f, root, 6, pair, 2, &alt, NULL) == MULIB_SPPF_INVALID);
    mulib_sppf_clear(f);
    assert(mulib_sppf_node_count(f) == 0 && mulib_sppf_alternative_count(f) == 0);
    mulib_sppf_destroy(f);
}

static void test_sppf_arithmetic(void) {
    mulib_sppf_t *f = mulib_sppf_create(0, 0, 0);
    size_t node, parent, alternative, pair[2], count, i;
    assert(f);
    assert(mulib_sppf_intern(f, MULIB_SPPF_NONTERMINAL, 0, 0, 0, &node, NULL) == 0);
    assert(mulib_sppf_pack(f, node, 0, NULL, 0, &alternative, NULL) == 0);
    assert(mulib_sppf_pack(f, node, 1, NULL, 0, &alternative, NULL) == 0);
    for (i = 1; i < 9; ++i) {
        assert(mulib_sppf_intern(f, MULIB_SPPF_NONTERMINAL, i, 0, 0, &parent, NULL) == 0);
        pair[0] = node; pair[1] = node;
        assert(mulib_sppf_pack(f, parent, i, pair, 2, &alternative, NULL) == 0);
        node = parent;
        if (i == 1) {
            assert(mulib_sppf_count(f, node, 4, 0, &count) == MULIB_SPPF_OK && count == 4);
            assert(mulib_sppf_count(f, node, 3, 0, &count) == MULIB_SPPF_LIMIT && count == 3);
        }
    }
    assert(mulib_sppf_count(f, node, SIZE_MAX, 0, &count) == MULIB_SPPF_LIMIT && count == SIZE_MAX);
    mulib_sppf_destroy(f);
}

static void test_ambiguous(void) {
    mulib_glr_t *g = mulib_glr_create(NULL);
    mulib_glr_result_t r;
    size_t s = grammar_nt(g), a = grammar_term(g, 'a', 'a'), rhs[2];
    uint32_t input[] = {'a', 'a', 'a', 'a', 'a', 'a'}, bad[] = {'a', 'b'};
    rhs[0] = s; rhs[1] = s;
    grammar_rule(g, s, rhs, 2); grammar_rule(g, s, &a, 1);
    assert(mulib_glr_compile(g, s) == 0 && mulib_glr_state_count(g) > 1);
    grammar_expect(g, input, 1, 1); grammar_expect(g, input, 3, 2);
    grammar_expect(g, input, 4, 5); grammar_expect(g, input, 6, 42);
    mulib_glr_result_init(&r);
    assert(mulib_glr_parse(g, bad, 2, &r) == MULIB_GLR_SYNTAX && r.position == 1 && r.root == MULIB_GLR_NONE);
    assert(r.forest && r.gss);
    mulib_glr_result_free(&r);
    assert(mulib_glr_parse(g, NULL, 0, &r) == MULIB_GLR_SYNTAX && r.position == 0);
    mulib_glr_result_free(&r);
    assert(mulib_glr_parse(g, NULL, 1, &r) == MULIB_GLR_INVALID);
    assert(mulib_glr_compile(g, a) == MULIB_GLR_INVALID); /* old machine survives */
    grammar_expect(g, input, 3, 2);
    grammar_rule(g, s, &a, 1); /* duplicate rules are distinct parses */
    assert(mulib_glr_parse(g, input, 1, &r) == MULIB_GLR_UNCOMPILED);
    assert(mulib_glr_compile(g, s) == 0);
    grammar_expect(g, input, 1, 2);
    assert(mulib_glr_parse(g, input, 3, &r) == 0);
    mulib_glr_destroy(g); /* result storage is independently owned */
    {
        size_t count;
        assert(mulib_sppf_count(r.forest, r.root, SIZE_MAX, 0, &count) == 0 && count == 16);
    }
    mulib_glr_result_free(&r);
}

static void test_nullable(void) {
    mulib_glr_t *g = mulib_glr_create(NULL);
    size_t s = grammar_nt(g), a = grammar_nt(g), token = grammar_term(g, 0, UINT32_MAX), rhs[2];
    uint32_t input[] = {UINT32_MAX, 0};
    rhs[0] = a; rhs[1] = a;
    grammar_rule(g, s, rhs, 2); grammar_rule(g, a, NULL, 0); grammar_rule(g, a, &token, 1);
    assert(mulib_glr_compile(g, s) == 0);
    grammar_expect(g, NULL, 0, 1); grammar_expect(g, input, 1, 2); grammar_expect(g, input, 2, 1);
    mulib_glr_destroy(g);
}

static void test_cycles_and_left_recursion(void) {
    mulib_glr_t *g = mulib_glr_create(NULL);
    mulib_glr_result_t r;
    size_t s = grammar_nt(g), a = grammar_nt(g), count, rhs[2], token = grammar_term(g, 'x', 'x');
    uint32_t input[32];
    size_t i;
    grammar_rule(g, s, &a, 1); grammar_rule(g, a, &s, 1); grammar_rule(g, a, NULL, 0);
    assert(mulib_glr_compile(g, s) == 0);
    mulib_glr_result_init(&r);
    assert(mulib_glr_parse(g, NULL, 0, &r) == 0);
    assert(mulib_sppf_count(r.forest, r.root, SIZE_MAX, 0, &count) == MULIB_SPPF_INFINITE);
    mulib_glr_result_free(&r); mulib_glr_destroy(g);
    g = mulib_glr_create(NULL); s = grammar_nt(g); token = grammar_term(g, 'x', 'x');
    rhs[0] = s; rhs[1] = token;
    grammar_rule(g, s, rhs, 2); grammar_rule(g, s, NULL, 0);
    assert(mulib_glr_compile(g, s) == 0);
    for (i = 0; i < 32; ++i) input[i] = 'x';
    grammar_expect(g, input, 32, 1); grammar_expect(g, NULL, 0, 1);
    mulib_glr_destroy(g);
    g = mulib_glr_create(NULL); s = grammar_nt(g);
    grammar_rule(g, s, &s, 1); /* unproductive cycle rejects rather than hangs */
    assert(mulib_glr_compile(g, s) == 0);
    assert(mulib_glr_parse(g, NULL, 0, &r) == MULIB_GLR_SYNTAX);
    mulib_glr_result_free(&r); mulib_glr_destroy(g);
    g = mulib_glr_create(NULL); s = grammar_nt(g);
    rhs[0] = s; rhs[1] = s;
    grammar_rule(g, s, rhs, 2); grammar_rule(g, s, NULL, 0);
    assert(mulib_glr_compile(g, s) == 0);
    assert(mulib_glr_parse(g, NULL, 0, &r) == 0);
    assert(mulib_sppf_count(r.forest, r.root, SIZE_MAX, 0, &count) == MULIB_SPPF_INFINITE);
    mulib_glr_result_free(&r); mulib_glr_destroy(g);
}

static void test_overlap_and_limits(void) {
    mulib_glr_limits_t limits;
    mulib_glr_t *g;
    mulib_glr_result_t r;
    size_t s, a, b, id;
    uint32_t token = '5';
    mulib_glr_limits_default(&limits);
    limits.max_gss_nodes = 1;
    g = mulib_glr_create(&limits); assert(g);
    s = grammar_nt(g); a = grammar_term(g, '0', '9'); grammar_rule(g, s, &a, 1);
    assert(mulib_glr_compile(g, s) == 0);
    mulib_glr_result_init(&r);
    assert(mulib_glr_parse(g, &token, 1, &r) == MULIB_GLR_LIMIT && !r.gss && !r.forest);
    mulib_glr_destroy(g);
    limits.max_gss_nodes = 0; limits.max_lr_states = 1;
    g = mulib_glr_create(&limits); s = grammar_nt(g); a = grammar_term(g, '0', '9');
    grammar_rule(g, s, &a, 1);
    assert(mulib_glr_compile(g, s) == MULIB_GLR_LIMIT && !mulib_glr_state_count(g));
    mulib_glr_destroy(g);
    mulib_glr_limits_default(&limits); limits.max_steps = 1;
    g = mulib_glr_create(&limits); s = grammar_nt(g); grammar_rule(g, s, NULL, 0);
    assert(mulib_glr_compile(g, s) == MULIB_GLR_LIMIT); mulib_glr_destroy(g);
    g = mulib_glr_create(NULL); s = grammar_nt(g); a = grammar_term(g, '0', '9'); b = grammar_term(g, '5', '7');
    grammar_rule(g, s, &a, 1); grammar_rule(g, s, &b, 1);
    assert(mulib_glr_compile(g, s) == 0);
    grammar_expect(g, &token, 1, 2);
    assert(mulib_glr_production(g, a, NULL, 0, &id) == MULIB_GLR_INVALID);
    assert(mulib_glr_terminal(g, 1, 0, &id) == MULIB_GLR_INVALID);
    id = MULIB_GLR_NONE;
    assert(mulib_glr_production(g, s, &id, 1, &id) == MULIB_GLR_INVALID);
    grammar_expect(g, &token, 1, 2); /* failed edits did not invalidate */
    assert(mulib_glr_parse(g, &token, 1, &r) == 0);
    assert(mulib_glr_parse(g, &token, 1, &r) == MULIB_GLR_INVALID); /* no implicit overwrite */
    mulib_glr_result_free(&r); mulib_glr_destroy(g);
    /* Exhaust each parse artifact limit and require complete error cleanup. */
    {
        size_t which;
        uint32_t tokens[64];
        for (which = 0; which < 64; ++which) tokens[which] = 'x';
        for (which = 0; which < 7; ++which) {
            size_t rhs[2];
            mulib_glr_limits_default(&limits);
            if (which == 0) limits.max_sppf_nodes = 1;
            if (which == 1) limits.max_alternatives = 1;
            if (which == 2) limits.max_children = 1;
            if (which == 3) limits.max_gss_edges = 1;
            if (which == 4) limits.max_tokens = 1;
            if (which == 5) limits.max_steps = 1000;
            if (which == 6) limits.max_gss_nodes = 1;
            g = mulib_glr_create(&limits);
            s = grammar_nt(g); a = grammar_term(g, 'x', 'x');
            rhs[0] = s; rhs[1] = a;
            grammar_rule(g, s, rhs, 2); grammar_rule(g, s, NULL, 0);
            assert(mulib_glr_compile(g, s) == 0);
            assert(mulib_glr_parse(g, tokens, 64, &r) == MULIB_GLR_LIMIT);
            assert(!r.forest && !r.gss && r.root == MULIB_GLR_NONE && r.position == 0);
            mulib_glr_destroy(g);
        }
    }
}

static void test_allocation_failures(void) {
    mulib_glr_t *g = mulib_glr_create(NULL);
    mulib_glr_result_t r;
    size_t s = grammar_nt(g), token = grammar_term(g, 'x', 'x'), rhs[2], fail, count;
    uint32_t input[] = {'x', 'x', 'x'};
    int status = MULIB_GLR_NOMEM;
    rhs[0] = s; rhs[1] = s;
    grammar_rule(g, s, rhs, 2); grammar_rule(g, s, &token, 1);
    assert(mulib_glr_compile(g, s) == 0);
    mulib_glr_result_init(&r);
    /* Sweep every allocation in recompilation; the old machine must survive. */
    for (fail = 0; fail < 256; ++fail) {
        grammar_allocations_left = fail;
        status = mulib_glr_compile(g, s);
        grammar_allocations_left = SIZE_MAX;
        assert(status == MULIB_GLR_OK || status == MULIB_GLR_NOMEM);
        assert(mulib_glr_state_count(g) > 0);
        if (!status) break;
        grammar_expect(g, input, 3, 2);
    }
    assert(status == 0 && fail > 5 && fail < 256);
    for (fail = 0; fail < 256; ++fail) {
        grammar_allocations_left = fail;
        status = mulib_glr_parse(g, input, 3, &r);
        grammar_allocations_left = SIZE_MAX;
        assert(status == MULIB_GLR_OK || status == MULIB_GLR_NOMEM);
        if (!status) break;
        assert(!r.forest && !r.gss && r.root == MULIB_GLR_NONE && r.position == 0);
    }
    assert(status == 0 && fail > 5 && fail < 256);
    for (fail = 0; fail < 32; ++fail) {
        grammar_allocations_left = fail;
        status = mulib_sppf_count(r.forest, r.root, SIZE_MAX, 0, &count);
        grammar_allocations_left = SIZE_MAX;
        assert(status == MULIB_SPPF_OK || status == MULIB_SPPF_NOMEM);
        if (!status) { assert(count == 2); break; }
        assert(count == 0);
    }
    assert(status == 0 && fail > 0 && fail < 32);
    mulib_glr_result_free(&r);
    /* Cloning a borrowed RHS must work even when the RHS vector reallocates. */
    {
        const size_t *borrowed;
        size_t lhs, n, i, id;
        for (i = 0; i < 12; ++i) {
            assert(mulib_glr_rule(g, 0, &lhs, &borrowed, &n) == 0 && n == 2);
            assert(mulib_glr_production(g, lhs, borrowed, n, &id) == 0);
        }
    }
    mulib_glr_destroy(g);
    grammar_allocations_left = 0;
    assert(!mulib_glr_create(NULL));
    assert(!mulib_atn_create(0, 0));
    assert(!mulib_gss_create(0, 0));
    assert(!mulib_sppf_create(0, 0, 0));
    grammar_allocations_left = SIZE_MAX;
}

static void test_transactional_insertions(void) {
    mulib_glr_t *g = mulib_glr_create(NULL);
    size_t s = grammar_nt(g), token = grammar_term(g, 'x', 'x'), rhs[8], i, id;
    const size_t *borrowed, *after;
    size_t lhs, n, states;
    for (i = 0; i < 8; ++i) rhs[i] = token;
    grammar_rule(g, s, rhs, 8);
    assert(mulib_glr_compile(g, s) == 0);
    states = mulib_glr_state_count(g);
    assert(mulib_glr_rule(g, 0, &lhs, &borrowed, &n) == 0 && n == 8);
    id = MULIB_GLR_NONE;
    /* Copy succeeds, RHS reserve fails: borrowing and old compilation survive. */
    grammar_allocations_left = 1;
    assert(mulib_glr_production(g, s, borrowed, n, &id) == MULIB_GLR_NOMEM);
    grammar_allocations_left = SIZE_MAX;
    assert(id == MULIB_GLR_NONE && mulib_glr_production_count(g) == 1);
    assert(mulib_glr_state_count(g) == states);
    assert(mulib_glr_rule(g, 0, &lhs, &after, &n) == 0 && after == borrowed && n == 8);
    for (i = 0; i < n; ++i) assert(after[i] == token);
    mulib_glr_destroy(g);
    {
        mulib_sppf_t *f = mulib_sppf_create(0, 0, 0);
        mulib_sppf_node_t record;
        size_t leaf, root;
        assert(f);
        assert(mulib_sppf_intern(f, MULIB_SPPF_TERMINAL, 0, 0, 1, &leaf, NULL) == 0);
        assert(mulib_sppf_intern(f, MULIB_SPPF_NONTERMINAL, 1, 0, 1, &root, NULL) == 0);
        /* Child storage reserve succeeds, alternative storage reserve fails. */
        grammar_allocations_left = 1;
        assert(mulib_sppf_pack(f, root, 0, &leaf, 1, &id, NULL) == MULIB_SPPF_NOMEM);
        grammar_allocations_left = SIZE_MAX;
        assert(mulib_sppf_alternative_count(f) == 0);
        assert(mulib_sppf_node(f, root, &record) == 0 && record.first_alternative == MULIB_SPPF_NONE);
        assert(mulib_sppf_pack(f, root, 0, &leaf, 1, &id, NULL) == 0);
        assert(mulib_sppf_alternative_count(f) == 1);
        mulib_sppf_destroy(f);
    }
    {
        mulib_gss_t *stack = mulib_gss_create(0, 0);
        mulib_gss_node_t record;
        size_t base, top;
        assert(stack);
        assert(mulib_gss_intern(stack, 0, 0, &base, NULL) == 0);
        assert(mulib_gss_intern(stack, 1, 1, &top, NULL) == 0);
        grammar_allocations_left = 0;
        assert(mulib_gss_link(stack, top, base, 0, &id, NULL) == MULIB_GSS_NOMEM);
        grammar_allocations_left = SIZE_MAX;
        assert(mulib_gss_node(stack, top, &record) == 0 && record.first_edge == MULIB_GSS_NONE);
        assert(mulib_gss_edge_count(stack) == 0);
        mulib_gss_destroy(stack);
    }
}

/* Exercise internal-state errors without relying on uninitialized outputs. */
static void test_inconsistent_parser_records(void) {
    mulib_glr_t *g = mulib_glr_create(NULL);
    mulib_glr_result_t r;
    mulib_glr_context_ context;
    mulib_glr_path_frame_ frames[2];
    size_t children[1], s = grammar_nt(g), token = grammar_term(g, 'x', 'x');
    size_t base, head, edge, leaf, accepting;
    mulib_gss_node_t *head_record;
    mulib_gss_edge_t *edge_record;
    grammar_rule(g, s, &token, 1);
    assert(mulib_glr_compile(g, s) == 0);
    mulib_glr_result_init(&r);
    r.gss = mulib_gss_create(0, 0); r.forest = mulib_sppf_create(0, 0, 0);
    assert(r.gss && r.forest);
    context.grammar = g; context.result = &r; context.position = 0;
    context.work = 0; context.changed = 0;
    assert(mulib_glr_reduce_(&context, MULIB_GSS_NONE, 0, children) == MULIB_GLR_INVALID);
    assert(mulib_glr_paths_(&context, MULIB_GSS_NONE, 0, frames, children) == MULIB_GLR_INVALID);
    assert(mulib_gss_intern(r.gss, MULIB_GLR_NONE, 0, &head, NULL) == 0);
    assert(mulib_glr_reduce_(&context, head, 0, children) == MULIB_GLR_INVALID);
    assert(mulib_glr_saturate_(&context, frames, children) == MULIB_GLR_INVALID);
    assert(mulib_glr_accept_(&context) == MULIB_GLR_INVALID);
    assert(mulib_glr_shift_(&context, 'x') == MULIB_GLR_INVALID);
    mulib_gss_clear(r.gss);
    for (accepting = 0; accepting < g->states.length; ++accepting)
        if (mulib_glr_state_(g, accepting)->accepting) break;
    assert(accepting < g->states.length);
    assert(mulib_gss_intern(r.gss, 0, 0, &base, NULL) == 0);
    assert(mulib_gss_intern(r.gss, accepting, 1, &head, NULL) == 0);
    assert(mulib_gss_link(r.gss, head, base, 0, &edge, NULL) == 0);
    context.position = 1;
    /* The GSS label does not yet identify an existing SPPF node. */
    assert(mulib_glr_paths_(&context, head, 0, frames, children) == MULIB_GLR_INVALID);
    assert(mulib_glr_accept_(&context) == MULIB_GLR_INVALID);
    assert(mulib_sppf_intern(r.forest, MULIB_SPPF_TERMINAL, token, 0, 1, &leaf, NULL) == 0 && leaf == 0);
    /* Deliberately corrupt private records to exercise failed edge/node reads. */
    head_record = (mulib_gss_node_t *)mulib_vec_at(&r.gss->nodes, head);
    head_record->first_edge = MULIB_GSS_NONE - 1;
    assert(mulib_glr_paths_(&context, head, 0, frames, children) == MULIB_GLR_INVALID);
    assert(mulib_glr_accept_(&context) == MULIB_GLR_INVALID);
    head_record->first_edge = edge;
    edge_record = (mulib_gss_edge_t *)mulib_vec_at(&r.gss->edges, edge);
    edge_record->predecessor = MULIB_GSS_NONE;
    assert(mulib_glr_paths_(&context, head, 0, frames, children) == MULIB_GLR_INVALID);
    mulib_glr_result_free(&r); mulib_glr_destroy(g);
}

/* Independent finite span-relation oracle: no LR items, stack, or forest. */
typedef struct grammar_oracle_rule { size_t lhs, length, rhs[3]; } grammar_oracle_rule;
static int grammar_oracle_accept(const grammar_oracle_rule *rules,
                                 const uint32_t *tokens, size_t length) {
    unsigned char relation[2][5][5] = {{{0}}};
    int changed;
    size_t r, begin, end, k, pos, next;
    do {
        changed = 0;
        for (r = 0; r < 6; ++r) for (begin = 0; begin <= length; ++begin) {
            unsigned char current[5] = {0}, following[5];
            current[begin] = 1;
            for (k = 0; k < rules[r].length; ++k) {
                size_t symbol = rules[r].rhs[k];
                memset(following, 0, sizeof(following));
                for (pos = begin; pos <= length; ++pos) if (current[pos]) {
                    if (symbol >= 2) {
                        if (pos < length && tokens[pos] == (uint32_t)('a' + symbol - 2)) following[pos + 1] = 1;
                    } else for (next = pos; next <= length; ++next)
                        if (relation[symbol][pos][next]) following[next] = 1;
                }
                memcpy(current, following, sizeof(current));
            }
            for (end = begin; end <= length; ++end) if (current[end] && !relation[rules[r].lhs][begin][end]) {
                relation[rules[r].lhs][begin][end] = 1; changed = 1;
            }
        }
    } while (changed);
    return relation[0][0][length] != 0;
}
static uint32_t grammar_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}
static void test_small_cfg_oracle(void) {
    uint32_t random = UINT32_C(73921);
    size_t trial, i, k, length, bits;
    for (trial = 0; trial < 32; ++trial) {
        mulib_glr_t *g = mulib_glr_create(NULL);
        grammar_oracle_rule rules[6];
        size_t symbols[4];
        symbols[0] = grammar_nt(g); symbols[1] = grammar_nt(g);
        symbols[2] = grammar_term(g, 'a', 'a'); symbols[3] = grammar_term(g, 'b', 'b');
        for (i = 0; i < 6; ++i) {
            size_t rhs[3];
            rules[i].lhs = (grammar_random(&random) >> 16) % 2;
            rules[i].length = (grammar_random(&random) >> 16) % 4;
            for (k = 0; k < rules[i].length; ++k) {
                rules[i].rhs[k] = (grammar_random(&random) >> 16) % 4;
                rhs[k] = symbols[rules[i].rhs[k]];
            }
            grammar_rule(g, symbols[rules[i].lhs], rhs, rules[i].length);
        }
        assert(mulib_glr_compile(g, symbols[0]) == 0);
        for (length = 0; length <= 3; ++length) for (bits = 0; bits < ((size_t)1 << length); ++bits) {
            uint32_t tokens[4];
            mulib_glr_result_t r;
            int accepted, status;
            for (i = 0; i < length; ++i) tokens[i] = (uint32_t)('a' + ((bits >> i) & 1));
            accepted = grammar_oracle_accept(rules, tokens, length);
            mulib_glr_result_init(&r);
            status = mulib_glr_parse(g, tokens, length, &r);
            assert(status == (accepted ? MULIB_GLR_OK : MULIB_GLR_SYNTAX));
            if (accepted) {
                size_t count;
                status = mulib_sppf_count(r.forest, r.root, SIZE_MAX, 0, &count);
                assert((status == MULIB_SPPF_OK && count > 0) || status == MULIB_SPPF_INFINITE);
            }
            mulib_glr_result_free(&r);
        }
        mulib_glr_destroy(g);
    }
}

int main(void) {
    test_atn(); test_gss(); test_sppf(); test_sppf_arithmetic(); test_ambiguous(); test_nullable();
    test_cycles_and_left_recursion(); test_overlap_and_limits();
    test_allocation_failures(); test_transactional_insertions(); test_small_cfg_oracle();
    test_inconsistent_parser_records();
    puts("grammar assertions passed");
    return 0;
}
