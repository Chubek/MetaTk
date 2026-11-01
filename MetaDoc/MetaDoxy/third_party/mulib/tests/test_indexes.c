/* Standalone: cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test_indexes.c -o test_indexes */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* Deterministic allocation-failure injection, confined to these implementations. */
static long fail_after = -1;
static int allocation_allowed(void) {
    if (fail_after < 0) return 1;
    if (!fail_after) return 0;
    --fail_after; return 1;
}
static void *test_malloc(size_t n) { return allocation_allowed() ? malloc(n) : NULL; }
static void *test_calloc(size_t n, size_t s) { return allocation_allowed() ? calloc(n, s) : NULL; }
static void *test_realloc(void *p, size_t n) { return allocation_allowed() ? realloc(p, n) : NULL; }
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define MULIB_IMPL_AVL
#define MULIB_IMPL_BPTREE
#define MULIB_IMPL_HATTRIE
#define MULIB_IMPL_HASH
#define MULIB_IMPL_TXTDIFF
#define MULIB_IMPL_MMDIFF
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BUFFER
#include "mulib-avl.h"
#include "mulib-b+tree.h"
#include "mulib-hattrie.h"
#include "mulib-txtdiff.h"
#include "mulib-mmdiff.h"
#undef malloc
#undef calloc
#undef realloc

static uint32_t random_state = UINT32_C(0x49f29a13);
static uint32_t random_u32(void) {
    uint32_t x = random_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    random_state = x; return x;
}
#define DOMAIN 1024
typedef struct reference {
    unsigned char present[DOMAIN];
    uint64_t value[DOMAIN];
    size_t next, high, count;
} reference_t;
static int ordered_visit(int64_t key, uint64_t value, void *context) {
    reference_t *r = (reference_t *)context;
    while (r->next < DOMAIN && !r->present[r->next]) ++r->next;
    assert(r->next <= r->high && r->next < DOMAIN);
    assert(key == (int64_t)r->next && value == r->value[r->next]);
    ++r->next; ++r->count; return 1;
}
static int stop_ordered(int64_t key, uint64_t value, void *context) {
    (void)key; (void)value; (void)context; return 0;
}
static void trees_check(mulib_avl_t *a, mulib_bptree_t *b, reference_t *r) {
    size_t expected = 0;
    assert(mulib_avl_validate(a) && mulib_bptree_validate(b));
    for (size_t i = 0; i < DOMAIN; ++i) {
        uint64_t av = UINT64_MAX, bv = UINT64_MAX;
        assert(mulib_avl_get(a, (int64_t)i, &av) == r->present[i]);
        assert(mulib_bptree_get(b, (int64_t)i, &bv) == r->present[i]);
        if (r->present[i]) { ++expected; assert(av == r->value[i] && bv == av); }
        else assert(av == UINT64_MAX && bv == UINT64_MAX);
    }
    assert(a->size == expected && b->size == expected);
    r->next = r->count = 0; r->high = DOMAIN - 1;
    assert(mulib_avl_foreach(a, ordered_visit, r) == expected && r->count == expected);
    r->next = r->count = 0;
    assert(mulib_bptree_range(b, INT64_MIN, INT64_MAX, ordered_visit, r) == expected && r->count == expected);
    r->next = 113; r->high = 701; r->count = 0; expected = 0;
    for (size_t i = r->next; i <= r->high; ++i) expected += r->present[i];
    assert(mulib_bptree_range(b, 113, 701, ordered_visit, r) == expected && r->count == expected);
}
static void test_trees(void) {
    mulib_avl_t a; mulib_bptree_t b; reference_t r;
    mulib_avl_init(&a); mulib_bptree_init(&b); memset(&r, 0, sizeof(r));
    trees_check(&a, &b, &r);
    assert(!mulib_avl_remove(&a, 0, NULL) && !mulib_bptree_remove(&b, 0, NULL));
    for (size_t step = 0; step < 40000; ++step) {
        size_t k = random_u32() % DOMAIN;
        uint64_t v = ((uint64_t)random_u32() << 32) | random_u32();
        if (random_u32() % 3) {
            assert(mulib_avl_set(&a, (int64_t)k, v)); assert(mulib_bptree_set(&b, (int64_t)k, v));
            r.present[k] = 1; r.value[k] = v;
        } else {
            uint64_t av = UINT64_MAX, bv = UINT64_MAX;
            assert(mulib_avl_remove(&a, (int64_t)k, &av) == r.present[k]);
            assert(mulib_bptree_remove(&b, (int64_t)k, &bv) == r.present[k]);
            if (r.present[k]) assert(av == r.value[k] && av == bv);
            else assert(av == UINT64_MAX && bv == UINT64_MAX);
            r.present[k] = 0;
        }
        assert(a.size == b.size);
        if (step % 79 == 0) trees_check(&a, &b, &r);
    }
    trees_check(&a, &b, &r);
    assert(mulib_avl_foreach(&a, stop_ordered, NULL) == 1);
    assert(mulib_bptree_range(&b, INT64_MIN, INT64_MAX, stop_ordered, NULL) == 1);
    assert(!mulib_bptree_range(&b, 5, 4, stop_ordered, NULL));
    mulib_avl_free(&a); mulib_bptree_free(&b);
    for (int mode = 0; mode < 4; ++mode) {
        for (int64_t i = 0; i < 6000; ++i) {
            int64_t k = mode & 1 ? 5999 - i : i;
            assert(mulib_avl_set(&a, k, (uint64_t)k));
            assert(mulib_bptree_set(&b, k, (uint64_t)k));
            assert(mulib_avl_validate(&a) && mulib_bptree_validate(&b));
        }
        for (int64_t i = 0; i < 6000; ++i) {
            int64_t k = mode & 2 ? 5999 - i : i;
            assert(mulib_avl_remove(&a, k, NULL) && mulib_bptree_remove(&b, k, NULL));
            assert(mulib_avl_validate(&a) && mulib_bptree_validate(&b));
        }
        assert(!a.size && !b.size);
    }
    assert(mulib_avl_set(&a, INT64_MIN, 0) && mulib_avl_set(&a, INT64_MAX, UINT64_MAX));
    assert(mulib_bptree_set(&b, INT64_MIN, 0) && mulib_bptree_set(&b, INT64_MAX, UINT64_MAX));
    assert(mulib_avl_validate(&a) && mulib_bptree_validate(&b));
    assert(mulib_avl_remove(&a, INT64_MIN, NULL) && mulib_bptree_remove(&b, INT64_MIN, NULL));
    mulib_avl_free(&a); mulib_bptree_free(&b);
    assert(!mulib_avl_set(NULL, 0, 0) && !mulib_bptree_set(NULL, 0, 0));
}

#define KEY_COUNT 512
typedef struct key_reference {
    unsigned char key[KEY_COUNT][16];
    size_t length[KEY_COUNT];
    unsigned char present[KEY_COUNT], seen[KEY_COUNT];
    uint64_t value[KEY_COUNT];
    const unsigned char *prefix;
    size_t prefix_length, count;
} key_reference_t;
static int trie_visit(const void *key, size_t length, uint64_t value, void *context) {
    key_reference_t *r = (key_reference_t *)context;
    size_t i;
    assert(length >= r->prefix_length && (!r->prefix_length || !memcmp(key, r->prefix, r->prefix_length)));
    for (i = 0; i < KEY_COUNT; ++i) if (r->length[i] == length && (!length || !memcmp(key, r->key[i], length))) break;
    assert(i < KEY_COUNT && r->present[i] && !r->seen[i] && r->value[i] == value);
    r->seen[i] = 1; ++r->count; return 1;
}
static int stop_trie(const void *key, size_t n, uint64_t v, void *ctx) {
    (void)key; (void)n; (void)v; (void)ctx; return 0;
}
static void trie_check(mulib_hattrie_t *t, key_reference_t *r) {
    size_t total = 0;
    assert(mulib_hattrie_validate(t));
    for (size_t i = 0; i < KEY_COUNT; ++i) {
        uint64_t v = UINT64_MAX;
        assert(mulib_hattrie_get(t, r->key[i], r->length[i], &v) == r->present[i]);
        if (r->present[i]) { ++total; assert(v == r->value[i]); }
        else assert(v == UINT64_MAX);
    }
    assert(total == t->size);
    for (size_t prefix_length = 0; prefix_length <= 10; ++prefix_length) {
        size_t expected = 0;
        r->prefix = r->key[127]; r->prefix_length = prefix_length; r->count = 0;
        memset(r->seen, 0, sizeof(r->seen));
        for (size_t i = 0; i < KEY_COUNT; ++i) if (r->present[i] && r->length[i] >= prefix_length &&
                (!prefix_length || !memcmp(r->key[i], r->prefix, prefix_length))) ++expected;
        assert(mulib_hattrie_prefix(t, r->prefix, prefix_length, trie_visit, r) == expected);
        assert(r->count == expected);
    }
}
static void test_trie(void) {
    mulib_hattrie_t t; key_reference_t r;
    memset(&r, 0, sizeof(r)); mulib_hattrie_init(&t);
    r.length[0] = 0; r.length[1] = 1;
    for (size_t i = 2; i < KEY_COUNT; ++i) {
        memcpy(r.key[i], "common\0", 7); r.key[i][7] = (unsigned char)i;
        r.key[i][8] = (unsigned char)(i >> 8); r.length[i] = 9 + i % 4;
    }
    trie_check(&t, &r);
    for (size_t step = 0; step < 24000; ++step) {
        size_t i = random_u32() % KEY_COUNT;
        if (random_u32() % 3) {
            uint64_t v = random_u32();
            assert(mulib_hattrie_set(&t, r.key[i], r.length[i], v));
            r.present[i] = 1; r.value[i] = v;
        } else {
            uint64_t v = UINT64_MAX;
            assert(mulib_hattrie_remove(&t, r.key[i], r.length[i], &v) == r.present[i]);
            if (r.present[i]) assert(v == r.value[i]); else assert(v == UINT64_MAX);
            r.present[i] = 0;
        }
        if (step % 113 == 0) trie_check(&t, &r);
    }
    trie_check(&t, &r);
    assert(mulib_hattrie_prefix(&t, NULL, 0, stop_trie, NULL) == 1);
    for (size_t i = 0; i < KEY_COUNT; ++i) {
        assert(mulib_hattrie_remove(&t, r.key[i], r.length[i], NULL) == r.present[i]);
        assert(mulib_hattrie_validate(&t));
    }
    assert(!t.root && !t.size);
    /* Long common prefixes must compress rather than recurse once per byte. */
    {
        unsigned char key[16385]; memset(key, 'x', sizeof(key));
        for (unsigned int i = 0; i < 64; ++i) {
            key[16384] = (unsigned char)i;
            assert(mulib_hattrie_set(&t, key, sizeof(key), i));
        }
        assert(t.root->branch && t.root->prefix_length == 16384);
        assert(mulib_hattrie_set(&t, key, 4, 100)); /* split at a shorter key */
        key[2] = 'y'; assert(mulib_hattrie_set(&t, key, 10, 101)); /* divergent compressed prefix */
        assert(mulib_hattrie_validate(&t));
        assert(mulib_hattrie_prefix(&t, key, 3, stop_trie, NULL) == 1);
        assert(mulib_hattrie_get(&t, key, 10, NULL));
    }
    mulib_hattrie_free(&t); assert(mulib_hattrie_validate(&t));
    {
        unsigned char key[512]; memset(key, 'a', sizeof(key));
        for (size_t n = 1; n <= sizeof(key); ++n) assert(mulib_hattrie_set(&t, key, n, n));
        assert(mulib_hattrie_validate(&t));
        for (size_t n = sizeof(key); n > 0; --n) {
            uint64_t value;
            assert(mulib_hattrie_get(&t, key, n, &value) && value == n);
        }
        /* Exercise nonrecursive release of a deeply nested exact-prefix family. */
        mulib_hattrie_free(&t); assert(mulib_hattrie_validate(&t));
    }
    assert(mulib_hattrie_set(&t, NULL, 0, 7));
    assert(mulib_hattrie_get(&t, NULL, 0, NULL));
    assert(!mulib_hattrie_set(&t, NULL, 1, 0));
    assert(!mulib_hattrie_get(&t, NULL, 1, NULL));
    assert(!mulib_hattrie_remove(&t, NULL, 1, NULL));
    mulib_hattrie_free(&t);
}

typedef struct test_line { size_t offset, length; } test_line_t;
static size_t split_lines(const unsigned char *s, size_t n, test_line_t *lines, size_t capacity) {
    size_t count = 0, start = 0;
    for (size_t i = 0; i < n; ++i) if (s[i] == '\n' || i == n - 1) {
        if (lines) {
            if (count == capacity) return SIZE_MAX;
            lines[count].offset = start; lines[count].length = i - start + 1;
        }
        ++count; start = i + 1;
    }
    return count;
}
static size_t reference_edit_count(const unsigned char *a, size_t an, const unsigned char *b, size_t bn) {
    size_t ac = split_lines(a, an, NULL, 0), bc = split_lines(b, bn, NULL, 0);
    test_line_t *al, *bl;
    size_t *dp, result;
    assert(ac <= SIZE_MAX / sizeof(*al) && bc <= SIZE_MAX / sizeof(*bl));
    assert(bc < SIZE_MAX && bc + 1 <= SIZE_MAX / sizeof(*dp) && ac <= SIZE_MAX - bc);
    al = ac ? (test_line_t *)malloc(ac * sizeof(*al)) : NULL;
    bl = bc ? (test_line_t *)malloc(bc * sizeof(*bl)) : NULL;
    dp = (size_t *)calloc(bc + 1, sizeof(*dp));
    assert((!ac || al) && (!bc || bl) && dp);
    assert(split_lines(a, an, al, ac) == ac && split_lines(b, bn, bl, bc) == bc);
    for (size_t i = 0; i < ac; ++i) {
        size_t diagonal = 0;
        for (size_t j = 0; j < bc; ++j) {
            size_t above = dp[j + 1];
            if (al[i].length == bl[j].length && !memcmp(a + al[i].offset, b + bl[j].offset, al[i].length))
                dp[j + 1] = diagonal + 1;
            else if (dp[j] > above) dp[j + 1] = dp[j];
            diagonal = above;
        }
    }
    result = ac + bc - 2 * dp[bc];
    free(al); free(bl); free(dp); return result;
}
static void expect_bytes(const mulib_buffer_t *out, const void *data, size_t n) {
    assert(out->length == n && (!n || !memcmp(out->data, data, n)));
}
static void text_roundtrip(const unsigned char *a, size_t an, const unsigned char *b, size_t bn) {
    mulib_txtdiff_patch_t p; mulib_buffer_t out;
    size_t changed = 0;
    mulib_txtdiff_init(&p); mulib_buffer_init(&out);
    assert(mulib_txtdiff_make(a, an, b, bn, NULL, &p) == MULIB_TXTDIFF_OK);
    for (size_t i = 0; i < p.edits.length; ++i) {
        mulib_txtdiff_edit_t *e = (mulib_txtdiff_edit_t *)mulib_vec_at(&p.edits, i);
        const mulib_buffer_t *bytes = e->kind == MULIB_TXTDIFF_INSERT ? &p.inserts : &p.source;
        assert(e->offset == 0 || bytes->data[e->offset - 1] == '\n');
        assert(e->offset + e->length == bytes->length || bytes->data[e->offset + e->length - 1] == '\n');
        if (e->kind != MULIB_TXTDIFF_KEEP) changed += split_lines(bytes->data + e->offset, e->length, NULL, 0);
    }
    assert(changed == reference_edit_count(a, an, b, bn));
    assert(mulib_txtdiff_apply(&p, a, an, bn, &out) == MULIB_TXTDIFF_OK); expect_bytes(&out, b, bn);
    if (bn) assert(mulib_txtdiff_apply(&p, a, an, bn - 1, &out) == MULIB_TXTDIFF_LIMIT);
    expect_bytes(&out, b, bn);
    assert(mulib_txtdiff_make(b, bn, a, an, NULL, &p) == MULIB_TXTDIFF_OK);
    assert(mulib_txtdiff_apply(&p, out.data, out.length, an, &out) == MULIB_TXTDIFF_OK);
    expect_bytes(&out, a, an); mulib_txtdiff_free(&p); mulib_buffer_free(&out);
}
static size_t random_text(unsigned char *s) {
    size_t n = random_u32() % 150;
    for (size_t i = 0; i < n; ++i) {
        uint32_t x = random_u32() % 8;
        s[i] = x == 0 ? '\n' : x == 1 ? 0 : x == 2 ? '\r' : (unsigned char)('a' + x % 3);
    }
    return n;
}
static void test_text_line_scripts(void) {
    typedef struct script_case {
        const char *source, *inserts, *expected;
        size_t target_length, edit_count;
        mulib_txtdiff_edit_t edits[3];
    } script_case_t;
    static const script_case_t cases[] = {
        /* Source spans may not remove or retain just part of a line. */
        { "a\n", "", NULL, 1, 2,
          {{MULIB_TXTDIFF_KEEP, 0, 1}, {MULIB_TXTDIFF_DELETE, 1, 1}} },
        { "a\n", "", NULL, 1, 2,
          {{MULIB_TXTDIFF_DELETE, 0, 1}, {MULIB_TXTDIFF_KEEP, 1, 1}} },
        { "a\nb\n", "", NULL, 3, 3,
          {{MULIB_TXTDIFF_KEEP, 0, 2}, {MULIB_TXTDIFF_DELETE, 2, 1}, {MULIB_TXTDIFF_KEEP, 3, 1}} },
        /* Literal spans must also start/end at complete-line boundaries. */
        { "", "a\n", NULL, 1, 1, {{MULIB_TXTDIFF_INSERT, 0, 1}} },
        { "", "a\n", NULL, 1, 1, {{MULIB_TXTDIFF_INSERT, 1, 1}} },
        { "", "a\n", NULL, 2, 2,
          {{MULIB_TXTDIFF_INSERT, 0, 1}, {MULIB_TXTDIFF_INSERT, 1, 1}} },
        /* An unterminated emitted line cannot be extended by a later edit. */
        { "a", "\n", NULL, 2, 2,
          {{MULIB_TXTDIFF_KEEP, 0, 1}, {MULIB_TXTDIFF_INSERT, 0, 1}} },
        { "a\n", "x", NULL, 3, 2,
          {{MULIB_TXTDIFF_INSERT, 0, 1}, {MULIB_TXTDIFF_KEEP, 0, 2}} },
        { "a\nb\n", "x", NULL, 3, 3,
          {{MULIB_TXTDIFF_INSERT, 0, 1}, {MULIB_TXTDIFF_DELETE, 0, 2}, {MULIB_TXTDIFF_KEEP, 2, 2}} },
        { "", "x\ny", NULL, 3, 2,
          {{MULIB_TXTDIFF_INSERT, 2, 1}, {MULIB_TXTDIFF_INSERT, 0, 2}} },
        { "a\n", "x", NULL, 2, 3,
          {{MULIB_TXTDIFF_INSERT, 0, 1}, {MULIB_TXTDIFF_DELETE, 0, 2}, {MULIB_TXTDIFF_INSERT, 0, 1}} },
        /* Unterminated final lines are valid, including before trailing deletes. */
        { "a\n", "x", "a\nx", 3, 2,
          {{MULIB_TXTDIFF_KEEP, 0, 2}, {MULIB_TXTDIFF_INSERT, 0, 1}} },
        { "a\n", "x", "x", 1, 2,
          {{MULIB_TXTDIFF_INSERT, 0, 1}, {MULIB_TXTDIFF_DELETE, 0, 2}} },
        { "a\nb", "x\n", "a\nx\nb", 5, 3,
          {{MULIB_TXTDIFF_KEEP, 0, 2}, {MULIB_TXTDIFF_INSERT, 0, 2}, {MULIB_TXTDIFF_KEEP, 2, 1}} },
        /* Adding/removing a final LF requires replacing the whole source line. */
        { "a", "a\n", "a\n", 2, 2,
          {{MULIB_TXTDIFF_DELETE, 0, 1}, {MULIB_TXTDIFF_INSERT, 0, 2}} },
        { "a\n", "a", "a", 1, 2,
          {{MULIB_TXTDIFF_DELETE, 0, 2}, {MULIB_TXTDIFF_INSERT, 0, 1}} },
        { "", "x\ny", "x\ny", 3, 2,
          {{MULIB_TXTDIFF_INSERT, 0, 2}, {MULIB_TXTDIFF_INSERT, 2, 1}} },
        { "a", "", "", 0, 1, {{MULIB_TXTDIFF_DELETE, 0, 1}} },
        { "a\r\nb", "", "a\r\nb", 4, 2,
          {{MULIB_TXTDIFF_KEEP, 0, 3}, {MULIB_TXTDIFF_KEEP, 3, 1}} },
        { "old\n", "new\r\nlast", "new\r\nlast", 9, 2,
          {{MULIB_TXTDIFF_DELETE, 0, 4}, {MULIB_TXTDIFF_INSERT, 0, 9}} }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const script_case_t *c = &cases[i];
        mulib_txtdiff_patch_t p; mulib_buffer_t out;
        mulib_txtdiff_status_t status;
        unsigned char *saved_data;
        size_t saved_capacity;
        mulib_txtdiff_init(&p); mulib_buffer_init(&out);
        assert(mulib_buffer_append(&p.source, c->source, strlen(c->source)));
        assert(mulib_buffer_append(&p.inserts, c->inserts, strlen(c->inserts)));
        p.target_length = c->target_length;
        for (size_t j = 0; j < c->edit_count; ++j) assert(mulib_vec_push(&p.edits, &c->edits[j]));
        assert(mulib_buffer_append(&out, "sentinel", 8));
        saved_data = out.data; saved_capacity = out.capacity;
        if (!c->expected) fail_after = 0; /* Rejection must precede allocation. */
        status = mulib_txtdiff_apply(&p, c->source, strlen(c->source), 128, &out);
        fail_after = -1;
        if (c->expected) {
            assert(status == MULIB_TXTDIFF_OK && strlen(c->expected) == c->target_length);
            expect_bytes(&out, c->expected, c->target_length);
        } else {
            assert(status == MULIB_TXTDIFF_INVALID);
            assert(out.data == saved_data && out.capacity == saved_capacity);
            expect_bytes(&out, "sentinel", 8);
        }
        mulib_txtdiff_free(&p); mulib_buffer_free(&out);
    }
}
static void test_text(void) {
    unsigned char a[160], b[160];
    text_roundtrip(NULL, 0, NULL, 0);
    text_roundtrip((const unsigned char *)"a\nb\na\n", 6, (const unsigned char *)"b\na\nb", 5);
    text_roundtrip((const unsigned char *)"\n\n", 2, (const unsigned char *)"\n", 1);
    {
        test_line_t limited[1];
        assert(split_lines((const unsigned char *)"\n\n", 2, limited, 1) == SIZE_MAX);
        assert(limited[0].offset == 0 && limited[0].length == 1);
        memset(a, '\n', sizeof(a)); memset(b, '\n', sizeof(b));
        text_roundtrip(a, 65, NULL, 0); /* Original fixed-size oracle overflow. */
        text_roundtrip(NULL, 0, b, 129);
        text_roundtrip(a, sizeof(a), b, 129);
        b[64] = 'x'; b[sizeof(b) - 1] = 'z';
        text_roundtrip(a, sizeof(a), b, sizeof(b));
    }
    test_text_line_scripts();
    for (size_t i = 0; i < 1200; ++i) {
        size_t an = random_text(a), bn = random_text(b);
        if (i % 3 == 0) { bn = an; memcpy(b, a, an); if (bn) b[random_u32() % bn] ^= 1; }
        text_roundtrip(a, an, b, bn);
    }
    {
        mulib_txtdiff_patch_t p; mulib_buffer_t out; mulib_txtdiff_edit_t saved, *e;
        mulib_txtdiff_limits_t limits = { 100, 3 };
        mulib_txtdiff_init(&p); mulib_buffer_init(&out);
        assert(mulib_txtdiff_make("a\n", 2, "b\n", 2, NULL, &p) == MULIB_TXTDIFF_OK);
        assert(mulib_buffer_append(&out, "sentinel", 8));
        assert(mulib_txtdiff_make("a\n", 2, "b\n", 2, &limits, &p) == MULIB_TXTDIFF_LIMIT);
        limits.max_cells = 100; limits.max_bytes = 1;
        assert(mulib_txtdiff_make("a\n", 2, "b\n", 2, &limits, &p) == MULIB_TXTDIFF_LIMIT);
        assert(mulib_txtdiff_make(NULL, 1, NULL, 0, NULL, &p) == MULIB_TXTDIFF_INVALID);
        e = (mulib_txtdiff_edit_t *)mulib_vec_at(&p.edits, 0); saved = *e;
        for (int mode = 0; mode < 5; ++mode) {
            *e = saved;
            if (mode == 0) e->kind = (mulib_txtdiff_kind_t)99;
            if (mode == 1) e->offset = SIZE_MAX;
            if (mode == 2) e->length = SIZE_MAX;
            if (mode == 3) e->length = 0;
            if (mode == 4) e->length = 1; /* incomplete consumption */
            assert(mulib_txtdiff_apply(&p, "a\n", 2, 100, &out) == MULIB_TXTDIFF_INVALID);
            expect_bytes(&out, "sentinel", 8);
        }
        *e = saved;
        e = (mulib_txtdiff_edit_t *)mulib_vec_at(&p.edits, p.edits.length - 1); saved = *e;
        e->offset = SIZE_MAX;
        assert(mulib_txtdiff_apply(&p, "a\n", 2, 100, &out) == MULIB_TXTDIFF_INVALID); *e = saved;
        ++p.target_length; assert(mulib_txtdiff_apply(&p, "a\n", 2, 100, &out) == MULIB_TXTDIFF_INVALID); --p.target_length;
        assert(mulib_txtdiff_apply(&p, "z\n", 2, 100, &out) == MULIB_TXTDIFF_INVALID);
        --p.edits.stride; assert(mulib_txtdiff_apply(&p, "a\n", 2, 100, &out) == MULIB_TXTDIFF_INVALID); ++p.edits.stride;
        assert(mulib_txtdiff_apply(&p, "a\n", 2, 100, &p.source) == MULIB_TXTDIFF_INVALID);
        assert(mulib_txtdiff_apply(&p, "a\n", 2, 100, &out) == MULIB_TXTDIFF_OK); expect_bytes(&out, "b\n", 2);
        mulib_txtdiff_free(&p); mulib_buffer_free(&out);
    }
}
static void binary_roundtrip(const unsigned char *a, size_t an, const unsigned char *b, size_t bn) {
    mulib_mmdiff_patch_t p; mulib_buffer_t out;
    mulib_mmdiff_limits_t limits = { 1 + random_u32() % 16, 1024, 1024, 10000000 };
    mulib_mmdiff_init(&p); mulib_buffer_init(&out);
    assert(mulib_mmdiff_make(a, an, b, bn, &limits, &p) == MULIB_MMDIFF_OK);
    assert(mulib_mmdiff_apply(&p, a, an, bn, &out) == MULIB_MMDIFF_OK); expect_bytes(&out, b, bn);
    if (bn) assert(mulib_mmdiff_apply(&p, a, an, bn - 1, &out) == MULIB_MMDIFF_LIMIT);
    expect_bytes(&out, b, bn);
    assert(mulib_mmdiff_make(b, bn, a, an, &limits, &p) == MULIB_MMDIFF_OK);
    assert(mulib_mmdiff_apply(&p, out.data, out.length, an, &out) == MULIB_MMDIFF_OK);
    expect_bytes(&out, a, an); mulib_mmdiff_free(&p); mulib_buffer_free(&out);
}
static void test_binary(void) {
    unsigned char a[256], b[512];
    binary_roundtrip(NULL, 0, NULL, 0);
    for (size_t i = 0; i < 1500; ++i) {
        size_t an = random_u32() % sizeof(a), bn = random_u32() % sizeof(b);
        for (size_t j = 0; j < an; ++j) a[j] = (unsigned char)random_u32();
        for (size_t j = 0; j < bn; ++j) b[j] = (an && random_u32() % 3) ? a[random_u32() % an] : (unsigned char)random_u32();
        if (an && i % 3 == 0) { bn = 2 * an; memcpy(b, a, an); memcpy(b + an, a, an); b[bn / 2] ^= 1; }
        binary_roundtrip(a, an, b, bn);
    }
    memset(a, 7, sizeof(a)); memset(b, 7, sizeof(b)); binary_roundtrip(a, sizeof(a), b, sizeof(b));
    {
        mulib_mmdiff_patch_t p; mulib_buffer_t out;
        mulib_mmdiff_limits_t limits = { 4, 1024, 1024, 1000000 };
        mulib_mmdiff_command_t *c, saved;
        mulib_mmdiff_init(&p); mulib_buffer_init(&out);
        assert(mulib_mmdiff_make("abcdefgh12345678", 16, "12345678!abcdefgh", 17, &limits, &p) == MULIB_MMDIFF_OK);
        assert(p.commands.length == 3);
        c = (mulib_mmdiff_command_t *)mulib_vec_at(&p.commands, 0); assert(c->kind == MULIB_MMDIFF_COPY && c->offset == 8);
        assert(mulib_buffer_append(&out, "sentinel", 8)); saved = *c;
        for (int mode = 0; mode < 4; ++mode) {
            *c = saved;
            if (mode == 0) c->kind = (mulib_mmdiff_kind_t)99;
            if (mode == 1) c->offset = SIZE_MAX;
            if (mode == 2) c->length = SIZE_MAX;
            if (mode == 3) c->length = 0;
            assert(mulib_mmdiff_apply(&p, "abcdefgh12345678", 16, 100, &out) == MULIB_MMDIFF_INVALID);
            expect_bytes(&out, "sentinel", 8);
        }
        *c = saved;
        c = (mulib_mmdiff_command_t *)mulib_vec_at(&p.commands, 1); saved = *c; c->offset = SIZE_MAX;
        assert(mulib_mmdiff_apply(&p, "abcdefgh12345678", 16, 100, &out) == MULIB_MMDIFF_INVALID); *c = saved;
        ++p.target_length; assert(mulib_mmdiff_apply(&p, "abcdefgh12345678", 16, 100, &out) == MULIB_MMDIFF_INVALID); --p.target_length;
        assert(mulib_mmdiff_apply(&p, "abcdefgh12345679", 16, 100, &out) == MULIB_MMDIFF_INVALID);
        --p.commands.stride; assert(mulib_mmdiff_apply(&p, "abcdefgh12345678", 16, 100, &out) == MULIB_MMDIFF_INVALID); ++p.commands.stride;
        assert(mulib_mmdiff_apply(&p, "abcdefgh12345678", 16, 100, &p.source) == MULIB_MMDIFF_INVALID);
        limits.max_work = 0;
        assert(mulib_mmdiff_make("abcdefgh", 8, "abcdefgh", 8, &limits, &p) == MULIB_MMDIFF_LIMIT);
        limits.max_work = 100000; limits.max_index_entries = 0;
        assert(mulib_mmdiff_make("abcdefgh", 8, "abcdefgh", 8, &limits, &p) == MULIB_MMDIFF_LIMIT);
        limits.max_index_entries = 100; limits.max_bytes = 1;
        assert(mulib_mmdiff_make("abcdefgh", 8, "abcdefgh", 8, &limits, &p) == MULIB_MMDIFF_LIMIT);
        limits.max_bytes = 100; limits.min_match = 0;
        assert(mulib_mmdiff_make("a", 1, "b", 1, &limits, &p) == MULIB_MMDIFF_INVALID);
        assert(mulib_mmdiff_make(NULL, 1, NULL, 0, NULL, &p) == MULIB_MMDIFF_INVALID);
        assert(mulib_mmdiff_apply(&p, "abcdefgh12345678", 16, 100, &out) == MULIB_MMDIFF_OK);
        expect_bytes(&out, "12345678!abcdefgh", 17);
        limits.min_match = SIZE_MAX;
        assert(mulib_mmdiff_make("a", 1, "b", 1, &limits, &p) == MULIB_MMDIFF_OK);
        mulib_mmdiff_free(&p); mulib_buffer_free(&out);
    }
}

static void test_allocation_failure(void) {
    mulib_avl_t a; mulib_avl_init(&a);
    assert(mulib_avl_set(&a, 1, 1)); fail_after = 0;
    assert(!mulib_avl_set(&a, 2, 2)); assert(mulib_avl_set(&a, 1, 3));
    fail_after = -1; assert(a.size == 1 && mulib_avl_validate(&a)); mulib_avl_free(&a);
    for (long budget = 0; budget <= 3; ++budget) {
        mulib_bptree_t b; mulib_bptree_init(&b);
        for (int64_t i = 0; i < 36; ++i) assert(mulib_bptree_set(&b, i, (uint64_t)i));
        assert(b.root->n == 8); fail_after = budget;
        assert(mulib_bptree_set(&b, 36, 36) == (budget == 3)); fail_after = -1;
        assert(b.size == (budget == 3 ? 37u : 36u) && mulib_bptree_validate(&b));
        for (int64_t i = 0; i < 36; ++i) assert(mulib_bptree_get(&b, i, NULL));
        mulib_bptree_free(&b);
    }
    for (long budget = 0; budget < 7; ++budget) {
        mulib_hattrie_t t; unsigned char key[64]; int ok;
        memset(key, 'x', sizeof(key)); mulib_hattrie_init(&t);
        for (unsigned int i = 0; i < 17; ++i) { key[63] = (unsigned char)i; assert(mulib_hattrie_set(&t, key, sizeof(key), i)); }
        assert(t.root->branch); key[3] = 'y'; fail_after = budget;
        ok = mulib_hattrie_set(&t, key, sizeof(key), 100); fail_after = -1;
        assert(t.size == (ok ? 18u : 17u) && mulib_hattrie_validate(&t));
        assert(mulib_hattrie_get(&t, key, sizeof(key), NULL) == ok);
        mulib_hattrie_free(&t);
    }
    {
        mulib_hattrie_t t; unsigned char key; mulib_hattrie_init(&t);
        for (unsigned int i = 0; i < 16; ++i) { key = (unsigned char)i; assert(mulib_hattrie_set(&t, &key, 1, i)); }
        key = 16; fail_after = 1; assert(mulib_hattrie_set(&t, &key, 1, 16)); fail_after = -1;
        assert(!t.root->branch && t.size == 17 && mulib_hattrie_validate(&t));
        key = 17; assert(mulib_hattrie_set(&t, &key, 1, 17)); assert(t.root->branch);
        mulib_hattrie_free(&t);
    }
    for (long budget = 0; budget <= 21; ++budget) {
        mulib_hattrie_t t; unsigned char key[8]; int ok;
        memset(key, 'a', sizeof(key)); mulib_hattrie_init(&t);
        for (unsigned int i = 0; i < 16; ++i) {
            key[7] = (unsigned char)i; assert(mulib_hattrie_set(&t, key, sizeof(key), i));
        }
        key[7] = 16; fail_after = budget;
        ok = mulib_hattrie_set(&t, key, sizeof(key), 16); fail_after = -1;
        assert(ok == (budget > 0));
        assert(t.size == (ok ? 17u : 16u) && mulib_hattrie_validate(&t));
        for (unsigned int i = 0; i < 16u + (unsigned int)ok; ++i) {
            uint64_t value; key[7] = (unsigned char)i;
            assert(mulib_hattrie_get(&t, key, sizeof(key), &value) && value == i);
        }
        fail_after = 0; mulib_hattrie_free(&t); fail_after = -1;
    }
    {
        mulib_txtdiff_patch_t p; mulib_buffer_t out; int succeeded = 0;
        mulib_txtdiff_init(&p); mulib_buffer_init(&out);
        assert(mulib_txtdiff_make("a", 1, "b", 1, NULL, &p) == MULIB_TXTDIFF_OK);
        for (long budget = 0; budget < 64; ++budget) {
            mulib_txtdiff_status_t status;
            fail_after = budget; status = mulib_txtdiff_make("x\ny\nz\n", 6, "z\ny\nx\n!", 7, NULL, &p); fail_after = -1;
            if (status == MULIB_TXTDIFF_OK) { succeeded = 1; break; }
            assert(status == MULIB_TXTDIFF_NOMEM);
            assert(mulib_txtdiff_apply(&p, "a", 1, 10, &out) == MULIB_TXTDIFF_OK); expect_bytes(&out, "b", 1);
        }
        assert(succeeded); assert(mulib_buffer_append(&out, "sentinel", 8));
        fail_after = 0;
        assert(mulib_txtdiff_apply(&p, "x\ny\nz\n", 6, 100, &out) == MULIB_TXTDIFF_NOMEM);
        fail_after = -1; expect_bytes(&out, "bsentinel", 9);
        mulib_txtdiff_free(&p); mulib_buffer_free(&out);
    }
    {
        mulib_mmdiff_patch_t p; mulib_buffer_t out; int succeeded = 0;
        mulib_mmdiff_init(&p); mulib_buffer_init(&out);
        assert(mulib_mmdiff_make("a", 1, "b", 1, NULL, &p) == MULIB_MMDIFF_OK);
        for (long budget = 0; budget < 64; ++budget) {
            mulib_mmdiff_status_t status;
            fail_after = budget; status = mulib_mmdiff_make("abcdefgh12345678", 16, "12345678!abcdefgh", 17, NULL, &p); fail_after = -1;
            if (status == MULIB_MMDIFF_OK) { succeeded = 1; break; }
            assert(status == MULIB_MMDIFF_NOMEM);
            assert(mulib_mmdiff_apply(&p, "a", 1, 10, &out) == MULIB_MMDIFF_OK); expect_bytes(&out, "b", 1);
        }
        assert(succeeded); assert(mulib_buffer_append(&out, "sentinel", 8));
        fail_after = 0;
        assert(mulib_mmdiff_apply(&p, "abcdefgh12345678", 16, 100, &out) == MULIB_MMDIFF_NOMEM);
        fail_after = -1; expect_bytes(&out, "bsentinel", 9);
        mulib_mmdiff_free(&p); mulib_buffer_free(&out);
    }
}
int main(void) {
    test_trees(); test_trie(); test_text(); test_binary(); test_allocation_failure();
    puts("indexes/diffs: all tests passed"); return 0;
}
