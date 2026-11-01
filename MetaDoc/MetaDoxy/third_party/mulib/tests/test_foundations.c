#define MULIB_IMPL_PORT
#define MULIB_IMPL_ALLOC
#define MULIB_IMPL_HASH
#define MULIB_IMPL_MEMPOOL
#define MULIB_IMPL_STRPOOL
#define MULIB_IMPL_MEMTKX
#define MULIB_IMPL_VECTOR
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
/* Inject failures only into vec's implementation, not unrelated modules. */
#include "mulib-vec.h"
static int test_vec_fail;
static void *test_vec_reallocate(void *memory, size_t size);
#define MULIB_IMPL_VEC
#define realloc test_vec_reallocate
#include "mulib-vec.h"
#undef realloc
#include "mulib-mempool.h"
#include "mulib-strpool.h"
#include "mulib-memtkx.h"
#include "mulib-vector.h"
#include "mulib-buffer.h"
#include "mulib-strbuf.h"
#include <assert.h>
#include <float.h>

static void *test_vec_reallocate(void *memory, size_t size) {
    if (test_vec_fail) return NULL;
    return realloc(memory, size);
}

typedef struct failing_allocator { size_t calls, fail_at, live; } failing_allocator;
static void *test_allocate(void *context, size_t n) {
    failing_allocator *f = (failing_allocator *)context;
    void *p;
    if (++f->calls == f->fail_at) return NULL;
    p = malloc(n);
    if (p) ++f->live;
    return p;
}
static void test_deallocate(void *context, void *p) {
    failing_allocator *f = (failing_allocator *)context;
    if (p) { assert(f->live); --f->live; free(p); }
}
static void *test_reallocate(void *context, void *p, size_t n) {
    failing_allocator *f = (failing_allocator *)context;
    int fresh = !p;
    void *next;
    if (++f->calls == f->fail_at) return NULL;
    next = realloc(p, n);
    if (next && fresh) ++f->live;
    return next;
}
static void test_array_resize(void) {
    failing_allocator f = {0, 0, 0};
    mulib_allocator_t a = {&f, test_allocate, test_reallocate, test_deallocate};
    void *p = NULL, *saved;
    assert(mulib_alloc_resize_array(&a, &p, 16, 1));
    memset(p, 37, 16);
    saved = p;
    f.fail_at = f.calls + 1;
    assert(!mulib_alloc_resize_array(&a, &p, 32, 1));
    assert(p == saved && ((unsigned char *)p)[15] == 37 && f.live == 1);
    a.reallocate = NULL;
    assert(!mulib_alloc_resize_array(&a, &p, 32, 1));
    assert(p == saved && ((unsigned char *)p)[15] == 37 && f.live == 1);
    a.reallocate = test_reallocate;
    f.fail_at = 0;
    assert(mulib_alloc_resize_array(&a, &p, 32, 1));
    assert(((unsigned char *)p)[15] == 37);
    assert(mulib_alloc_resize_array(&a, &p, 0, 1) && !p && !f.live);
}
typedef struct moving_record { void *pointer; size_t size; } moving_record;
typedef struct moving_allocator {
    moving_record records[32];
    void *quarantine[64];
    size_t live, retired, calls, fail_at;
} moving_allocator;
static void *test_moving_allocate(void *context, size_t n) {
    moving_allocator *a = (moving_allocator *)context;
    void *p;
    if (++a->calls == a->fail_at) return NULL;
    for (size_t i = 0; i < 32; ++i) if (!a->records[i].pointer) {
        p = malloc(n);
        if (p) { a->records[i].pointer = p; a->records[i].size = n; ++a->live; }
        return p;
    }
    assert(0 && "test allocator record capacity exhausted");
    return NULL;
}
static void test_moving_deallocate(void *context, void *p) {
    moving_allocator *a = (moving_allocator *)context;
    if (!p) return;
    for (size_t i = 0; i < 32; ++i) if (a->records[i].pointer == p) {
        assert(a->retired < 64);
        a->quarantine[a->retired++] = p;
        a->records[i].pointer = NULL;
        --a->live;
        return;
    }
    assert(0 && "backing allocator received an unknown pointer");
}
static void *test_moving_reallocate(void *context, void *p, size_t n) {
    moving_allocator *a = (moving_allocator *)context;
    void *next;
    if (++a->calls == a->fail_at) return NULL;
    for (size_t i = 0; i < 32; ++i) if (a->records[i].pointer == p) {
        next = malloc(n);
        if (!next) return NULL;
        memcpy(next, p, n < a->records[i].size ? n : a->records[i].size);
        assert(a->retired < 64);
        a->quarantine[a->retired++] = p;
        a->records[i].pointer = next;
        a->records[i].size = n;
        return next;
    }
    assert(0 && "realloc received an unknown pointer");
    return NULL;
}
static void test_arithmetic(void) {
    size_t n = 11;
    unsigned char secret[] = {1, 2, 3};
    assert(!mulib_port_add_size(SIZE_MAX, 1, &n) && n == 11);
    assert(!mulib_port_mul_size(SIZE_MAX, 2, &n) && n == 11);
    assert(!mulib_port_align_size(SIZE_MAX, 2, &n) && n == 11);
    assert(!mulib_port_align_size(0, 0, &n));
    assert(mulib_port_align_size(SIZE_MAX, 1, &n) && n == SIZE_MAX);
    assert(mulib_port_mul_size(0, SIZE_MAX, &n) && n == 0);
    mulib_port_secure_zero(secret, sizeof(secret));
    assert(secret[0] == 0 && secret[2] == 0);
}
static void test_pool(void) {
    for (size_t failure = 1; failure <= 6; ++failure) {
        failing_allocator f = {0, failure, 0};
        mulib_allocator_t a = {&f, test_allocate, NULL, test_deallocate};
        mulib_mempool_t *p = mulib_mempool_create(1, 3, &a);
        if (p) { (void)mulib_mempool_acquire(p); mulib_mempool_destroy(p); }
        assert(f.live == 0);
    }
    for (size_t failure = 1; failure <= 3; ++failure) {
        failing_allocator f = {0, 0, 0};
        mulib_allocator_t a = {&f, test_allocate, NULL, test_deallocate};
        mulib_mempool_t *pool = mulib_mempool_create(4, 1, &a);
        unsigned char *first;
        assert(pool && (first = mulib_mempool_acquire(pool)));
        first[0] = 29;
        f.fail_at = f.calls + failure;
        assert(!mulib_mempool_acquire(pool));
        assert(first[0] == 29 && mulib_mempool_in_use(pool) == 1);
        assert(mulib_mempool_capacity(pool) == 1 && f.live == 4);
        f.fail_at = 0;
        assert(mulib_mempool_acquire(pool));
        assert(mulib_mempool_capacity(pool) == 2 && first[0] == 29);
        mulib_mempool_destroy(pool);
        assert(f.live == 0);
    }
    mulib_mempool_t *p = mulib_mempool_create(3, 3, NULL);
    void *slots[100];
    int foreign;
    assert(p);
    for (size_t i = 0; i < 100; ++i) {
        slots[i] = mulib_mempool_acquire(p);
        assert(slots[i] && (uintptr_t)slots[i] % _Alignof(max_align_t) == 0);
        memset(slots[i], (int)i, 3);
    }
    assert(mulib_mempool_in_use(p) == 100 && mulib_mempool_capacity(p) == 102);
    assert(!mulib_mempool_release(p, &foreign));
    assert(!mulib_mempool_release(p, (unsigned char *)slots[0] + 1));
    for (size_t i = 0; i < 100; ++i) {
        assert(((unsigned char *)slots[i])[0] == i);
        assert(mulib_mempool_release(p, slots[i]));
        assert(!mulib_mempool_release(p, slots[i]));
    }
    assert(mulib_mempool_acquire(p));
    mulib_mempool_reset(p);
    assert(mulib_mempool_in_use(p) == 0);
    mulib_mempool_clear(p);
    assert(mulib_mempool_capacity(p) == 0 && mulib_mempool_acquire(p));
    mulib_mempool_destroy(p);
    assert(!mulib_mempool_create(SIZE_MAX, 10, NULL));
}
static void test_strings(void) {
    mulib_strpool_t *p = mulib_strpool_create(NULL);
    char text[32];
    const char *saved, *binary;
    assert(p);
    saved = mulib_strpool_intern(p, "stable");
    binary = mulib_strpool_intern_n(p, "a\0b", 3);
    assert(saved && binary);
    assert(binary == mulib_strpool_intern_n(p, "a\0b", 3));
    assert(binary != mulib_strpool_intern(p, "a"));
    for (size_t i = 0; i < 2000; ++i) {
        (void)snprintf(text, sizeof(text), "%zu", i);
        assert(mulib_strpool_intern(p, text));
    }
    assert(saved == mulib_strpool_intern(p, "stable"));
    assert(mulib_strpool_find_n(p, "a\0b", 3) == binary);
    assert(!mulib_strpool_find_n(p, "absent", 6));
    assert(mulib_strpool_intern_n(p, NULL, 0));
    assert(!mulib_strpool_reserve(p, SIZE_MAX));
    mulib_strpool_reset(p);
    assert(mulib_strpool_size(p) == 0 && mulib_strpool_bytes(p) == 0);
    mulib_strpool_destroy(p);
    for (size_t failure = 1; failure < 10; ++failure) {
        failing_allocator f = {0, failure, 0};
        mulib_allocator_t a = {&f, test_allocate, NULL, test_deallocate};
        p = mulib_strpool_create(&a);
        if (p) {
            (void)mulib_strpool_intern(p, "allocation failure");
            mulib_strpool_destroy(p);
        }
        assert(f.live == 0);
    }
    for (size_t failure = 1; failure <= 3; ++failure) {
        failing_allocator f = {0, 0, 0};
        mulib_allocator_t a = {&f, test_allocate, NULL, test_deallocate};
        const char *existing[12];
        p = mulib_strpool_create(&a);
        assert(p);
        for (size_t i = 0; i < 12; ++i) {
            (void)snprintf(text, sizeof(text), "%zu", i);
            existing[i] = mulib_strpool_intern(p, text);
            assert(existing[i]);
        }
        f.fail_at = f.calls + failure;
        assert(!mulib_strpool_intern(p, "thirteenth"));
        assert(mulib_strpool_size(p) == 12 && mulib_strpool_bytes(p) == 14);
        for (size_t i = 0; i < 12; ++i) {
            (void)snprintf(text, sizeof(text), "%zu", i);
            assert(mulib_strpool_find_n(p, text, strlen(text)) == existing[i]);
        }
        f.fail_at = 0;
        assert(mulib_strpool_intern(p, "thirteenth"));
        mulib_strpool_destroy(p);
        assert(f.live == 0);
    }
}
static void test_tracking(void) {
    failing_allocator f = {0, 0, 0};
    mulib_allocator_t backing = {&f, test_allocate, NULL, test_deallocate};
    mulib_memtkx_t *t = mulib_memtkx_create(&backing);
    mulib_allocator_t a = mulib_memtkx_allocator(t);
    void *p = mulib_alloc(&a, 12);
    uint64_t mark;
    mulib_memtkx_stats_t stats;
    assert(t && p && (uintptr_t)p % _Alignof(max_align_t) == 0);
    memset(p, 42, 12);
    mark = mulib_memtkx_mark(t);
    assert(mulib_alloc(&a, 10));
    p = mulib_alloc_realloc(&a, p, 32);
    assert(p && ((unsigned char *)p)[11] == 42);
    assert(mulib_memtkx_set_limit(t, 50));
    assert(!mulib_alloc_realloc(&a, p, 100));
    assert(((unsigned char *)p)[11] == 42);
    f.fail_at = f.calls + 1;
    assert(!mulib_alloc_realloc(&a, p, 36));
    assert(((unsigned char *)p)[11] == 42);
    f.fail_at = 0;
    assert(mulib_memtkx_release_since(t, mark) == 1);
    stats = mulib_memtkx_stats(t);
    assert(stats.live_blocks == 1 && stats.live_bytes == 32 && stats.reallocations == 1);
    assert(stats.failures == 2 && stats.peak_bytes == 42);
    assert(!mulib_memtkx_set_limit(t, 31));
    { int foreign; mulib_alloc_free(&a, &foreign); }
    assert(mulib_memtkx_stats(t).failures == 3);
    mulib_alloc_free(&a, p);
    mulib_memtkx_destroy(t);
    assert(f.live == 0);
}
static void test_tracking_moves(void) {
    moving_allocator moving = {0};
    mulib_allocator_t backing = {&moving, test_moving_allocate, test_moving_reallocate, test_moving_deallocate};
    mulib_memtkx_t *t = mulib_memtkx_create(&backing);
    mulib_allocator_t view = mulib_memtkx_allocator(t);
    void *p[3], *old;
    uint64_t mark = 0;
    assert(t);
    for (size_t i = 0; i < 3; ++i) {
        p[i] = mulib_alloc(&view, 8);
        assert(p[i]);
        memset(p[i], (int)(i + 10), 8);
        if (!i) mark = mulib_memtkx_mark(t);
    }
    for (size_t i = 3; i; --i) {
        old = p[i - 1];
        p[i - 1] = mulib_alloc_realloc(&view, p[i - 1], 16);
        /* Quarantined storage remains alive, making this comparison valid C11. */
        assert(p[i - 1] && p[i - 1] != old);
        assert(((unsigned char *)p[i - 1])[7] == i + 9);
    }
    assert(mulib_memtkx_stats(t).live_bytes == 48);
    assert(mulib_memtkx_release_since(t, mark) == 2);
    assert(mulib_memtkx_stats(t).live_blocks == 1);
    moving.fail_at = moving.calls + 1;
    assert(!mulib_alloc_realloc(&view, p[0], 20));
    assert(((unsigned char *)p[0])[7] == 10 && mulib_memtkx_stats(t).live_bytes == 16);
    moving.fail_at = 0;
    mulib_alloc_free(&view, p[0]);
    mulib_alloc_free(&view, p[0]);
    assert(mulib_memtkx_stats(t).failures == 2);
    mulib_memtkx_destroy(t);
    assert(moving.live == 0);
    for (size_t i = 0; i < moving.retired; ++i) free(moving.quarantine[i]);
}
static void test_vectors(void) {
    mulib_vector_t a, b, empty;
    const double coords[] = {3, 4, 0};
    double result = 99;
    assert(mulib_vector_from(&a, coords, 3));
    assert(mulib_vector_from(&b, coords, 3));
    assert(mulib_vector_init(&empty, 0));
    assert(mulib_vector_dot(&a, &b, &result) && result == 25);
    assert(!mulib_vector_dot(&a, &empty, &result) && result == 25);
    assert(mulib_vector_add(&a, &a, &b));
    assert(mulib_vector_norm(&a) == 10);
    assert(mulib_vector_cross(&a, &a, &b) && mulib_vector_norm(&a) == 0);
    assert(!mulib_vector_normalize(&a));
    assert(mulib_vector_normalize(&b) && fabs(mulib_vector_norm(&b) - 1) < 1e-14);
    assert(mulib_vector_set(&b, 0, DBL_MAX / 2));
    assert(mulib_vector_set(&b, 1, DBL_MAX / 2));
    assert(isfinite(mulib_vector_norm(&b)));
    assert(mulib_vector_set(&b, 0, INFINITY));
    assert(!mulib_vector_normalize(&b) && isinf(mulib_vector_data(&b)[0]));
    assert(!mulib_vector_get(&b, 3, &result) && result == 25);
    mulib_vector_free(&a);
    mulib_vector_free(&b);
    mulib_vector_free(&empty);
}
static void test_vector_replacement(void) {
    mulib_vector_t a, b, out, empty;
    const double x[] = {1, 2, 3}, y[] = {4, 5, 6};
    double *saved;
    assert(mulib_vector_from(&a, x, 3) && mulib_vector_from(&b, y, 3));
    assert(mulib_vector_from(&out, y, 3) && mulib_vector_init(&empty, 0));
    assert(mulib_vector_cross(&a, &a, &b));
    assert(mulib_vector_data(&a)[0] == -3 && mulib_vector_data(&a)[1] == 6 && mulib_vector_data(&a)[2] == -3);
    mulib_vector_free(&a);
    assert(mulib_vector_from(&a, x, 3));
    assert(mulib_vector_cross(&b, &a, &b));
    assert(mulib_vector_data(&b)[0] == -3 && mulib_vector_data(&b)[1] == 6 && mulib_vector_data(&b)[2] == -3);
    assert(mulib_vector_copy(&b, &a) && mulib_vector_copy(&b, &b));
    assert(mulib_vector_scale(&b, 2) && mulib_vector_axpy(&b, -1, &a));
    assert(mulib_vector_sub(&out, &a, &b) && mulib_vector_norm(&out) == 0);
    saved = mulib_vector_data(&out);
    assert(!mulib_vector_add(&out, &a, &empty) && mulib_vector_data(&out) == saved);
    test_vec_fail = 1;
    assert(!mulib_vector_copy(&out, &a) && mulib_vector_data(&out) == saved);
    assert(!mulib_vector_add(&out, &a, &b) && mulib_vector_data(&out) == saved);
    assert(!mulib_vector_sub(&out, &a, &b) && mulib_vector_data(&out) == saved);
    assert(!mulib_vector_cross(&out, &a, &b) && mulib_vector_data(&out) == saved);
    assert(mulib_vector_norm(&out) == 0);
    test_vec_fail = 0;
    mulib_vector_free(&a); mulib_vector_free(&b); mulib_vector_free(&out); mulib_vector_free(&empty);
}
static void test_shared_builders(void) {
    mulib_buffer_t b;
    mulib_strbuf_t s;
    mulib_buffer_init(&b);
    mulib_strbuf_init(&s);
    assert(mulib_buffer_remove(&b, 0, 0));
    for (size_t i = 0; i < 63; ++i) assert(mulib_buffer_append(&b, "a", 1));
    assert(mulib_buffer_append(&b, b.data, b.length));
    assert(b.length == 126);
    for (size_t i = 0; i < b.length; ++i) assert(b.data[i] == 'a');
    assert(mulib_buffer_append(&b, b.data + 100, 20));
    assert(!mulib_buffer_append(&b, b.data + b.length, 1));
    assert(mulib_buffer_remove(&b, 0, b.length));
    assert(mulib_buffer_remove(&b, 0, 0));
    for (size_t i = 0; i < 31; ++i) assert(mulib_strbuf_putc(&s, 'b'));
    assert(mulib_strbuf_append(&s, s.data));
    assert(s.length == 62 && s.data[62] == 0);
    for (size_t i = 0; i < s.length; ++i) assert(s.data[i] == 'b');
    assert(mulib_strbuf_append_n(&s, s.data + 10, 50));
    assert(!mulib_strbuf_append_n(&s, s.data + s.length, 2));
    mulib_buffer_free(&b);
    mulib_strbuf_free(&s);
}
int main(void) {
    test_arithmetic();
    test_array_resize();
    test_pool();
    test_strings();
    test_tracking();
    test_tracking_moves();
    test_vectors();
    test_vector_replacement();
    test_shared_builders();
    return 0;
}
