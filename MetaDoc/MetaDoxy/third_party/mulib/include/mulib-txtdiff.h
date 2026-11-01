/**
 * @file mulib-txtdiff.h
 * @brief Bounded line LCS diff with exact, transactional patch application.
 * Define MULIB_IMPL_TXTDIFF, MULIB_IMPL_VEC, MULIB_IMPL_BUFFER in one TU.
 * Lines include their LF, if any; all bytes (including NUL/CR) are preserved.
 * Patches are owning in-memory objects, not unified-diff files.
 */
#ifndef MULIB_TXTDIFF_H
#define MULIB_TXTDIFF_H
#include <stddef.h>
#include "mulib-vec.h"
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Explicit error codes; no errno contract. */
typedef enum mulib_txtdiff_status {
    MULIB_TXTDIFF_OK = 0, MULIB_TXTDIFF_INVALID, MULIB_TXTDIFF_LIMIT, MULIB_TXTDIFF_NOMEM
} mulib_txtdiff_status_t;
/** @brief KEEP/DELETE offsets address source; INSERT offsets address inserts. */
typedef enum mulib_txtdiff_kind {
    MULIB_TXTDIFF_KEEP = 1, MULIB_TXTDIFF_DELETE = 2, MULIB_TXTDIFF_INSERT = 3
} mulib_txtdiff_kind_t;
/** @brief A byte span containing one or more complete line edits. */
typedef struct mulib_txtdiff_edit { mulib_txtdiff_kind_t kind; size_t offset, length; } mulib_txtdiff_edit_t;
/** @brief Owning patch. Source copy permits exact rejection of a wrong base.
 * Initialize/free through the API; do not shallow-copy owning objects. */
typedef struct mulib_txtdiff_patch {
    mulib_vec_t edits;
    mulib_buffer_t source, inserts;
    size_t target_length;
} mulib_txtdiff_patch_t;
/** @brief Caps each input byte length and the full (old_lines+1)*(new_lines+1)
 * LCS table. NULL options mean 16 MiB per input and 1,000,000 cells. */
typedef struct mulib_txtdiff_limits { size_t max_bytes, max_cells; } mulib_txtdiff_limits_t;
/** @brief Initialize an uninitialized patch. */
void mulib_txtdiff_init(mulib_txtdiff_patch_t *patch);
/** @brief Release and reset a patch; NULL is accepted. */
void mulib_txtdiff_free(mulib_txtdiff_patch_t *patch);
/** @brief Construct a minimal line insertion/deletion script using LCS.
 * NULL data is allowed only at length zero. On error, initialized out is unchanged.
 * Time/memory are O(old_lines*new_lines); the caller can impose smaller limits. */
mulib_txtdiff_status_t mulib_txtdiff_make(const void *old_data, size_t old_length,
    const void *new_data, size_t new_length, const mulib_txtdiff_limits_t *limits,
    mulib_txtdiff_patch_t *out);
/** @brief Validate complete-line spans, source consumption, exact base, target
 * size and cap; then replace initialized out transactionally. An unterminated
 * line may only end the output. No terminating NUL is appended.
 * out must not be a buffer member of patch; base may alias out->data. */
mulib_txtdiff_status_t mulib_txtdiff_apply(const mulib_txtdiff_patch_t *patch,
    const void *base, size_t base_length, size_t max_output, mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TXTDIFF_H */

#if defined(MULIB_IMPL_TXTDIFF) && !defined(MULIB_TXTDIFF_IMPLEMENTED)
#define MULIB_TXTDIFF_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_txtdiff_line { size_t offset, length; } mulib_txtdiff_line_t;
void mulib_txtdiff_init(mulib_txtdiff_patch_t *p) {
    if (p) {
        mulib_vec_init(&p->edits, sizeof(mulib_txtdiff_edit_t));
        mulib_buffer_init(&p->source); mulib_buffer_init(&p->inserts); p->target_length = 0;
    }
}
void mulib_txtdiff_free(mulib_txtdiff_patch_t *p) {
    if (p) {
        mulib_vec_free(&p->edits); mulib_buffer_free(&p->source);
        mulib_buffer_free(&p->inserts); mulib_txtdiff_init(p);
    }
}
static mulib_txtdiff_status_t mulib_txtdiff_lines_(const unsigned char *s, size_t n,
                                                  size_t max_lines, mulib_vec_t *v) {
    size_t start = 0;
    for (size_t i = 0; i < n; ++i) if (s[i] == '\n' || i == n - 1) {
        mulib_txtdiff_line_t line;
        if (v->length == max_lines) return MULIB_TXTDIFF_LIMIT;
        line.offset = start; line.length = i - start + 1;
        if (!mulib_vec_push(v, &line)) return MULIB_TXTDIFF_NOMEM;
        start = i + 1;
    }
    return MULIB_TXTDIFF_OK;
}
static int mulib_txtdiff_edit_(mulib_txtdiff_patch_t *p, mulib_txtdiff_kind_t kind,
                                size_t off, size_t len) {
    mulib_txtdiff_edit_t e, *last;
    if (!len) return 1;
    last = p->edits.length ? (mulib_txtdiff_edit_t *)mulib_vec_at(&p->edits, p->edits.length - 1) : NULL;
    if (last && last->kind == kind && last->length <= SIZE_MAX - last->offset &&
        last->offset + last->length == off && len <= SIZE_MAX - last->length) {
        last->length += len; return 1;
    }
    e.kind = kind; e.offset = off; e.length = len;
    return mulib_vec_push(&p->edits, &e);
}
static int mulib_txtdiff_same_(const unsigned char *a, const mulib_txtdiff_line_t *x,
                               const unsigned char *b, const mulib_txtdiff_line_t *y) {
    return x->length == y->length && !memcmp(a + x->offset, b + y->offset, x->length);
}
mulib_txtdiff_status_t mulib_txtdiff_make(const void *old_data, size_t an,
        const void *new_data, size_t bn, const mulib_txtdiff_limits_t *limits,
        mulib_txtdiff_patch_t *out) {
    mulib_txtdiff_limits_t defaults = { 16u * 1024u * 1024u, 1000000u };
    const unsigned char *a = (const unsigned char *)old_data, *b = (const unsigned char *)new_data;
    mulib_vec_t av, bv;
    mulib_txtdiff_patch_t p;
    mulib_txtdiff_line_t *al, *bl;
    size_t rows, cols, cells, *dp = NULL, i, j;
    mulib_txtdiff_status_t status;
    if (!out || (!a && an) || (!b && bn)) return MULIB_TXTDIFF_INVALID;
    if (!limits) limits = &defaults;
    if (an > limits->max_bytes || bn > limits->max_bytes || !limits->max_cells) return MULIB_TXTDIFF_LIMIT;
    mulib_vec_init(&av, sizeof(mulib_txtdiff_line_t)); mulib_vec_init(&bv, sizeof(mulib_txtdiff_line_t));
    mulib_txtdiff_init(&p);
    status = mulib_txtdiff_lines_(a, an, limits->max_cells - 1, &av);
    if (status != MULIB_TXTDIFF_OK) goto done;
    status = mulib_txtdiff_lines_(b, bn, limits->max_cells - 1, &bv);
    if (status != MULIB_TXTDIFF_OK) goto done;
    rows = av.length + 1; cols = bv.length + 1;
    if (rows > limits->max_cells / cols || rows > SIZE_MAX / cols ||
        rows * cols > SIZE_MAX / sizeof(*dp)) { status = MULIB_TXTDIFF_LIMIT; goto done; }
    cells = rows * cols;
    dp = (size_t *)calloc(cells, sizeof(*dp));
    if (!dp) { status = MULIB_TXTDIFF_NOMEM; goto done; }
    al = (mulib_txtdiff_line_t *)av.data; bl = (mulib_txtdiff_line_t *)bv.data;
    for (i = av.length; i-- > 0;) for (j = bv.length; j-- > 0;) {
        if (mulib_txtdiff_same_(a, &al[i], b, &bl[j])) dp[i * cols + j] = 1 + dp[(i + 1) * cols + j + 1];
        else {
            size_t x = dp[(i + 1) * cols + j], y = dp[i * cols + j + 1];
            dp[i * cols + j] = x > y ? x : y;
        }
    }
    if (!mulib_buffer_append(&p.source, a, an)) { status = MULIB_TXTDIFF_NOMEM; goto done; }
    p.target_length = bn; i = j = 0;
    while (i < av.length || j < bv.length) {
        int ok;
        if (i < av.length && j < bv.length && mulib_txtdiff_same_(a, &al[i], b, &bl[j])) {
            ok = mulib_txtdiff_edit_(&p, MULIB_TXTDIFF_KEEP, al[i].offset, al[i].length); ++i; ++j;
        } else if (i < av.length && (j == bv.length || dp[(i + 1) * cols + j] >= dp[i * cols + j + 1])) {
            ok = mulib_txtdiff_edit_(&p, MULIB_TXTDIFF_DELETE, al[i].offset, al[i].length); ++i;
        } else {
            size_t off = p.inserts.length;
            ok = mulib_buffer_append(&p.inserts, b + bl[j].offset, bl[j].length) &&
                 mulib_txtdiff_edit_(&p, MULIB_TXTDIFF_INSERT, off, bl[j].length);
            ++j;
        }
        if (!ok) { status = MULIB_TXTDIFF_NOMEM; goto done; }
    }
    mulib_txtdiff_free(out); *out = p; mulib_txtdiff_init(&p); status = MULIB_TXTDIFF_OK;
done:
    free(dp); mulib_vec_free(&av); mulib_vec_free(&bv); mulib_txtdiff_free(&p); return status;
}
static int mulib_txtdiff_buffer_valid_(const mulib_buffer_t *b) {
    return b->length <= b->capacity && (!b->length || b->data);
}
static int mulib_txtdiff_line_span_(const mulib_buffer_t *b, size_t off, size_t len) {
    size_t end;
    if (!len || off > b->length || len > b->length - off) return 0;
    end = off + len;
    return (!off || b->data[off - 1] == '\n') &&
           (end == b->length || b->data[end - 1] == '\n');
}
mulib_txtdiff_status_t mulib_txtdiff_apply(const mulib_txtdiff_patch_t *p,
        const void *base, size_t n, size_t cap, mulib_buffer_t *out) {
    size_t consumed = 0, produced = 0;
    int unterminated = 0;
    mulib_buffer_t result;
    if (!p || !out || out == &p->source || out == &p->inserts || (!base && n) ||
        !mulib_txtdiff_buffer_valid_(&p->source) || !mulib_txtdiff_buffer_valid_(&p->inserts) ||
        p->edits.stride != sizeof(mulib_txtdiff_edit_t) || p->edits.length > p->edits.capacity ||
        p->edits.length > SIZE_MAX / sizeof(mulib_txtdiff_edit_t) ||
        (p->edits.length && !p->edits.data) || p->source.length != n ||
        (n && memcmp(p->source.data, base, n))) return MULIB_TXTDIFF_INVALID;
    if (p->target_length > cap) return MULIB_TXTDIFF_LIMIT;
    for (size_t i = 0; i < p->edits.length; ++i) {
        const mulib_txtdiff_edit_t *e = (const mulib_txtdiff_edit_t *)(p->edits.data + i * p->edits.stride);
        const mulib_buffer_t *bytes;
        if (!e->length) return MULIB_TXTDIFF_INVALID;
        if (e->kind == MULIB_TXTDIFF_INSERT) {
            bytes = &p->inserts;
            if (!mulib_txtdiff_line_span_(bytes, e->offset, e->length)) return MULIB_TXTDIFF_INVALID;
        } else if (e->kind == MULIB_TXTDIFF_KEEP || e->kind == MULIB_TXTDIFF_DELETE) {
            bytes = &p->source;
            if (e->offset != consumed || !mulib_txtdiff_line_span_(bytes, e->offset, e->length)) return MULIB_TXTDIFF_INVALID;
            consumed += e->length;
        } else return MULIB_TXTDIFF_INVALID;
        if (e->kind != MULIB_TXTDIFF_DELETE) {
            if (unterminated || e->length > SIZE_MAX - produced) return MULIB_TXTDIFF_INVALID;
            produced += e->length;
            unterminated = bytes->data[e->offset + e->length - 1] != '\n';
        }
    }
    if (consumed != n || produced != p->target_length) return MULIB_TXTDIFF_INVALID;
    mulib_buffer_init(&result);
    for (size_t i = 0; i < p->edits.length; ++i) {
        const mulib_txtdiff_edit_t *e = (const mulib_txtdiff_edit_t *)(p->edits.data + i * p->edits.stride);
        const unsigned char *bytes;
        if (e->kind == MULIB_TXTDIFF_DELETE) continue;
        bytes = e->kind == MULIB_TXTDIFF_KEEP ? p->source.data : p->inserts.data;
        if (!mulib_buffer_append(&result, bytes + e->offset, e->length)) {
            mulib_buffer_free(&result); return MULIB_TXTDIFF_NOMEM;
        }
    }
    mulib_buffer_free(out); *out = result; return MULIB_TXTDIFF_OK;
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
