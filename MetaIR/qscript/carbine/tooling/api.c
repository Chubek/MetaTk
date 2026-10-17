/* Carbine tooling: buffers, string lists, errors and version information. */
#include "internal.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define CB_VERSION "1.0.0"

static char last_error[512];

void cb_set_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(last_error, sizeof last_error, fmt, ap);
    va_end(ap);
}

const char *cb_last_error(void) { return last_error; }
const char *cb_version(void) { return CB_VERSION; }

const char *cb_status_text(cb_status status) {
    switch (status) {
    case CB_OK: return "ok";
    case CB_ERROR: return "error";
    case CB_INVALID_ARGUMENT: return "invalid argument";
    case CB_NOT_FOUND: return "not found";
    case CB_IO_ERROR: return "I/O error";
    case CB_UNSUPPORTED: return "unsupported";
    }
    return "unknown status";
}

char *cb_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* ---------- buffers ---------- */

void cb_buf_init(cb_buf *b) {
    b->data = NULL;
    b->len = b->cap = 0;
}

void cb_buf_free(cb_buf *b) {
    free(b->data);
    cb_buf_init(b);
}

cb_status cb_buf_add(cb_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        char *grown;
        while (cap < b->len + n + 1) cap *= 2;
        grown = realloc(b->data, cap);
        if (!grown) return CB_ERROR;
        b->data = grown;
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return CB_OK;
}

cb_status cb_buf_puts(cb_buf *b, const char *s) { return cb_buf_add(b, s, strlen(s)); }

cb_status cb_buf_printf(cb_buf *b, const char *fmt, ...) {
    char small[512];
    va_list ap;
    int n;
    cb_status st;
    va_start(ap, fmt);
    n = vsnprintf(small, sizeof small, fmt, ap);
    va_end(ap);
    if (n < 0) return CB_ERROR;
    if ((size_t)n < sizeof small) return cb_buf_add(b, small, (size_t)n);
    {
        char *big = malloc((size_t)n + 1);
        if (!big) return CB_ERROR;
        va_start(ap, fmt);
        vsnprintf(big, (size_t)n + 1, fmt, ap);
        va_end(ap);
        st = cb_buf_add(b, big, (size_t)n);
        free(big);
    }
    return st;
}

/* ---------- string lists ---------- */

void cb_list_init(cb_list *l) {
    l->items = NULL;
    l->count = l->cap = 0;
}

void cb_list_free(cb_list *l) {
    size_t i;
    for (i = 0; i < l->count; ++i) free(l->items[i]);
    free(l->items);
    cb_list_init(l);
}

cb_status cb_list_push(cb_list *l, const char *s) {
    char *copy;
    if (l->count == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 8;
        char **grown = realloc(l->items, cap * sizeof *grown);
        if (!grown) return CB_ERROR;
        l->items = grown;
        l->cap = cap;
    }
    copy = cb_strdup(s);
    if (!copy) return CB_ERROR;
    l->items[l->count++] = copy;
    return CB_OK;
}

cb_status cb_list_append(cb_list *dst, const cb_list *src) {
    size_t i;
    for (i = 0; i < src->count; ++i) {
        cb_status st = cb_list_push(dst, src->items[i]);
        if (st != CB_OK) return st;
    }
    return CB_OK;
}
