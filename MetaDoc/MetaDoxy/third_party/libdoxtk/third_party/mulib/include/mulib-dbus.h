/**
 * @file mulib-dbus.h
 * @brief D-Bus version 1 binary message codec, without transport/authentication.
 * Supports basic types except UNIX_FD, arrays, structs, dict entries, variants.
 * Define MULIB_IMPL_DBUS once, plus ENDIAN, BUFFER, STRBUF and UTF8 implementations.
 */
#ifndef MULIB_DBUS_H
#define MULIB_DBUS_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-endian.h"
#include "mulib-buffer.h"
#include "mulib-strbuf.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Results; MORE means a fixed header or complete declared frame is missing. */
typedef enum mulib_dbus_result {
    MULIB_DBUS_MORE = 0, MULIB_DBUS_OK = 1, MULIB_DBUS_INVALID = -1,
    MULIB_DBUS_LIMIT = -2, MULIB_DBUS_NOMEM = -3
} mulib_dbus_result;
/** @brief Typed value. Encode borrows pointers; decode owns them.
 * y/b/q/u/t use u64, n/i/x use i64, d uses real, s/o/g use text+length.
 * a/(/{/v use children+count. Arrays require an element signature; variants
 * require one complete signature and one child. Struct/dict signatures derive
 * from children. Never free stack-built encoder values with value_free.
 */
typedef struct mulib_dbus_value {
    char type;
    uint64_t u64;
    int64_t i64;
    double real;
    char *text;
    size_t length;
    struct mulib_dbus_value *children;
    size_t count;
    char *signature;
} mulib_dbus_value_t;
/** @brief Owned decoded message; encoder borrows this layout.
 * type: 1 call, 2 return, 3 error, 4 signal; endian: 'l' or 'B'.
 * signature may be NULL when encoding (derived automatically).
 */
typedef struct mulib_dbus_message {
    char endian;
    unsigned char type, flags;
    uint32_t serial, reply_serial;
    char *path, *interface, *member, *error_name, *destination, *sender, *signature;
    mulib_dbus_value_t *body;
    size_t count;
} mulib_dbus_message_t;
/** @brief Zero selects 16 MiB frame, 65536 values (including headers), depth 32.
 * Depth may be reduced but not increased beyond the supported maximum of 32.
 */
typedef struct mulib_dbus_limits { size_t max_bytes, max_values, max_depth; } mulib_dbus_limits_t;
/** @brief Validate a complete length-delimited signature (maximum 255 bytes).
 * Empty is valid; UNIX_FD ('h') and standalone dict entries are unsupported.
 */
int mulib_dbus_signature_valid(const char *signature, size_t length);
/** @brief Derive a malloc-owned signature from encoder values; return 1 or 0. */
int mulib_dbus_signature(const mulib_dbus_value_t *values, size_t count, char **signature);
/** @brief Recursively release a decoded value's storage, not the value itself. */
void mulib_dbus_value_free(mulib_dbus_value_t *value);
/** @brief Recursively release and zero a decoded message; NULL is harmless. */
void mulib_dbus_message_free(mulib_dbus_message_t *message);
/** @brief Encode one validated message to malloc-owned bytes. Outputs unchanged on error. */
mulib_dbus_result mulib_dbus_encode(const mulib_dbus_message_t *message,
    const mulib_dbus_limits_t *limits, unsigned char **bytes, size_t *length);
/** @brief Decode one message. On OK, out owns allocations and consumed is the exact
 * frame length (trailing/pipelined bytes are not consumed). Otherwise out is
 * unchanged and consumed is zero. Unknown supported header variants are ignored.
 */
mulib_dbus_result mulib_dbus_decode(const void *bytes, size_t length,
    const mulib_dbus_limits_t *limits, mulib_dbus_message_t *out, size_t *consumed);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DBUS_H */

#if defined(MULIB_IMPL_DBUS) && !defined(MULIB_DBUS_IMPLEMENTED)
#define MULIB_DBUS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_dbus_basic_(char c) { return c && strchr("ybnqiuxtdsog", c) != NULL; }
static int mulib_dbus_sig_one_(const char *s, size_t n, size_t *p, unsigned depth, int dict) {
    char c;
    if (*p >= n || depth > 32) return 0;
    c = s[(*p)++];
    if (mulib_dbus_basic_(c) || c == 'v') return 1;
    if (c == 'a') return mulib_dbus_sig_one_(s, n, p, depth+1, 1);
    if (c == '(') {
        size_t start = *p;
        while (*p < n && s[*p] != ')') if (!mulib_dbus_sig_one_(s, n, p, depth+1, 0)) return 0;
        if (*p == start || *p == n) return 0;
        ++*p; return 1;
    }
    if (c == '{' && dict) {
        if (*p == n || !mulib_dbus_basic_(s[*p])) return 0;
        ++*p;
        if (!mulib_dbus_sig_one_(s, n, p, depth+1, 0) || *p == n || s[*p] != '}') return 0;
        ++*p; return 1;
    }
    return 0;
}
int mulib_dbus_signature_valid(const char *s, size_t n) {
    size_t p = 0;
    if ((!s && n) || n > 255) return 0;
    while (p < n) if (!mulib_dbus_sig_one_(s, n, &p, 0, 0)) return 0;
    return 1;
}
static int mulib_dbus_single_(const char *s) {
    size_t p = 0, n;
    if (!s || !(n = strlen(s)) || !mulib_dbus_signature_valid(s, n)) return 0;
    return mulib_dbus_sig_one_(s, n, &p, 0, 0) && p == n;
}
static int mulib_dbus_sigput_(mulib_strbuf_t *b, const char *s, size_t n, mulib_dbus_result *error) {
    if (n > 255-b->length) { *error = MULIB_DBUS_INVALID; return 0; }
    if (!mulib_strbuf_append_n(b, s, n)) { *error = MULIB_DBUS_NOMEM; return 0; }
    return 1;
}
static int mulib_dbus_infer_(mulib_strbuf_t *b, const mulib_dbus_value_t *v, unsigned depth, int dict, mulib_dbus_result *error) {
    if (depth > 32 || !v || b->length >= 255) return 0;
    if (mulib_dbus_basic_(v->type) || v->type == 'v') return mulib_dbus_sigput_(b, &v->type, 1, error);
    if (v->type == 'a') {
        size_t p = 0, n;
        if (!v->signature || !(n = strlen(v->signature)) || n > 254 || !mulib_dbus_sig_one_(v->signature, n, &p, depth+1, 1) || p != n) return 0;
        return mulib_dbus_sigput_(b, "a", 1, error) && mulib_dbus_sigput_(b, v->signature, n, error);
    }
    if (v->type == '(' || (v->type == '{' && dict)) {
        if (!v->children || !v->count || (v->type == '{' && (v->count != 2 || !mulib_dbus_basic_(v->children[0].type)))) return 0;
        if (!mulib_dbus_sigput_(b, &v->type, 1, error)) return 0;
        for (size_t i = 0; i < v->count; ++i) if (!mulib_dbus_infer_(b, &v->children[i], depth+1, 0, error)) return 0;
        return mulib_dbus_sigput_(b, v->type == '(' ? ")" : "}", 1, error);
    }
    return 0;
}
static int mulib_dbus_derive_(const mulib_dbus_value_t *values, size_t count, char **signature, mulib_dbus_result *error) {
    mulib_strbuf_t b;
    if (!signature || (!values && count)) return 0;
    mulib_strbuf_init(&b);
    for (size_t i = 0; i < count; ++i) if (!mulib_dbus_infer_(&b, &values[i], 0, 0, error)) goto fail;
    if (!mulib_dbus_sigput_(&b, "", 0, error) || !mulib_dbus_signature_valid(b.data, b.length)) goto fail;
    *signature = b.data; return 1;
fail:
    mulib_strbuf_free(&b); return 0;
}
int mulib_dbus_signature(const mulib_dbus_value_t *values, size_t count, char **signature) {
    mulib_dbus_result error = MULIB_DBUS_INVALID;
    return mulib_dbus_derive_(values, count, signature, &error);
}
void mulib_dbus_value_free(mulib_dbus_value_t *v) {
    if (!v) return;
    for (size_t i = 0; i < v->count; ++i) mulib_dbus_value_free(&v->children[i]);
    free(v->children); free(v->text); free(v->signature); memset(v, 0, sizeof(*v));
}
void mulib_dbus_message_free(mulib_dbus_message_t *m) {
    if (!m) return;
    free(m->path); free(m->interface); free(m->member); free(m->error_name);
    free(m->destination); free(m->sender); free(m->signature);
    for (size_t i = 0; i < m->count; ++i) mulib_dbus_value_free(&m->body[i]);
    free(m->body); memset(m, 0, sizeof(*m));
}
static mulib_dbus_limits_t mulib_dbus_limits_(const mulib_dbus_limits_t *l) {
    mulib_dbus_limits_t v = { 16u*1024u*1024u, 65536, 32 };
    if (l) { if (l->max_bytes) v.max_bytes = l->max_bytes;
        if (l->max_values) v.max_values = l->max_values;
        if (l->max_depth && l->max_depth < 32) v.max_depth = l->max_depth; }
    if (v.max_bytes > 128u*1024u*1024u) v.max_bytes = 128u*1024u*1024u;
    return v;
}
static size_t mulib_dbus_align_(char c) {
    if (c == 'y' || c == 'g' || c == 'v') return 1;
    if (c == 'n' || c == 'q') return 2;
    if (c == 'x' || c == 't' || c == 'd' || c == '(' || c == '{') return 8;
    return 4;
}
static uint32_t mulib_dbus_load32_(const unsigned char *p, int little) {
    return little ? mulib_endian_load_le32(p) : mulib_endian_load_be32(p);
}
static void mulib_dbus_store32_(unsigned char *p, uint32_t v, int little) {
    if (little) mulib_endian_store_le32(p, v); else mulib_endian_store_be32(p, v);
}
static int mulib_dbus_letter_(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int mulib_dbus_digit_(unsigned char c) { return c >= '0' && c <= '9'; }
static int mulib_dbus_path_(const char *s, size_t n) {
    if (!n || s[0] != '/') return 0;
    if (n == 1) return 1;
    for (size_t i = 1; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '/') { if (s[i-1] == '/' || i+1 == n) return 0; }
        else if (!mulib_dbus_letter_(c) && !mulib_dbus_digit_(c)) return 0;
    }
    return 1;
}
/* kind 0 member, 1 interface/error, 2 bus name */
static int mulib_dbus_name_(const char *s, int kind) {
    size_t n, start = 0; unsigned dots = 0; int unique = 0;
    if (!s || !(n = strlen(s)) || n > 255) return 0;
    if (kind == 2 && s[0] == ':') { unique = 1; start = 1; if (start == n) return 0; }
    for (size_t i = start; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '.' && kind != 0) { if (i == start || s[i-1] == '.' || i+1 == n) return 0; ++dots; }
        else {
            int first = i == start || s[i-1] == '.';
            if (!mulib_dbus_letter_(c) && !(kind == 2 && c == '-') && !((!first || unique) && mulib_dbus_digit_(c))) return 0;
        }
    }
    return kind == 0 || dots != 0;
}
static int mulib_dbus_message_valid_(const mulib_dbus_message_t *m) {
    if ((m->endian != 'l' && m->endian != 'B') || m->type < 1 || m->type > 4 || (m->flags & ~7u) || !m->serial || (!m->body && m->count)) return 0;
    if ((m->path && !mulib_dbus_path_(m->path, strlen(m->path))) ||
        (m->interface && !mulib_dbus_name_(m->interface, 1)) || (m->member && !mulib_dbus_name_(m->member, 0)) ||
        (m->error_name && !mulib_dbus_name_(m->error_name, 1)) || (m->destination && !mulib_dbus_name_(m->destination, 2)) ||
        (m->sender && !mulib_dbus_name_(m->sender, 2))) return 0;
    if ((m->path && !strcmp(m->path, "/org/freedesktop/DBus/Local")) ||
        (m->interface && !strcmp(m->interface, "org.freedesktop.DBus.Local"))) return 0;
    if (m->type == 1 && (!m->path || !m->member)) return 0;
    if ((m->type == 2 || m->type == 3) && !m->reply_serial) return 0;
    if (m->type == 3 && !m->error_name) return 0;
    if (m->type == 4 && (!m->path || !m->interface || !m->member)) return 0;
    return 1;
}
typedef struct mulib_dbus_writer_ {
    mulib_buffer_t *b; int little; mulib_dbus_result error;
    mulib_dbus_limits_t limits; size_t nodes;
} mulib_dbus_writer_;
static int mulib_dbus_put_(mulib_dbus_writer_ *w, const void *p, size_t n) {
    if (n > w->limits.max_bytes || w->b->length > w->limits.max_bytes-n) { w->error = MULIB_DBUS_LIMIT; return 0; }
    if (!mulib_buffer_append(w->b, p, n)) { w->error = MULIB_DBUS_NOMEM; return 0; }
    return 1;
}
static int mulib_dbus_padw_(mulib_dbus_writer_ *w, size_t align) {
    static const unsigned char zero[8] = {0};
    return mulib_dbus_put_(w, zero, (align-w->b->length%align)%align);
}
static int mulib_dbus_uintw_(mulib_dbus_writer_ *w, uint64_t v, size_t width) {
    unsigned char p[8];
    if (width == 4) mulib_dbus_store32_(p, (uint32_t)v, w->little);
    else if (width == 8) {
        mulib_dbus_store32_(p + (w->little ? 0 : 4), (uint32_t)v, w->little);
        mulib_dbus_store32_(p + (w->little ? 4 : 0), (uint32_t)(v >> 32), w->little);
    } else for (size_t i = 0; i < width; ++i) p[w->little ? i : width-1-i] = (unsigned char)(v >> (i*8));
    return mulib_dbus_put_(w, p, width);
}
static int mulib_dbus_text_valid_(char type, const char *s, size_t n) {
    if ((!s && n) || (n && memchr(s, 0, n))) return 0;
    if (type == 'g') return mulib_dbus_signature_valid(s, n);
    if (type == 'o') return s && mulib_dbus_path_(s, n);
    return mulib_utf8_validate((const unsigned char *)s, n);
}
static int mulib_dbus_textw_(mulib_dbus_writer_ *w, char type, const char *s, size_t n) {
    static const unsigned char zero = 0;
    if (n > w->limits.max_bytes) { w->error = MULIB_DBUS_LIMIT; return 0; }
    if (n > (type == 'g' ? 255u : UINT32_MAX) || !mulib_dbus_text_valid_(type, s, n)) { w->error = MULIB_DBUS_INVALID; return 0; }
    return mulib_dbus_uintw_(w, (uint64_t)n, type == 'g' ? 1 : 4) && mulib_dbus_put_(w, s, n) && mulib_dbus_put_(w, &zero, 1);
}
static int mulib_dbus_write_(mulib_dbus_writer_ *w, const char *s, size_t n, size_t *p, const mulib_dbus_value_t *v, size_t depth) {
    char c; size_t start, stop;
    if (depth > w->limits.max_depth || w->nodes >= w->limits.max_values) { w->error = MULIB_DBUS_LIMIT; return 0; }
    ++w->nodes;
    if (*p >= n || !v || (c = s[(*p)++]) != v->type || (!v->children && v->count)) goto invalid;
    if (!mulib_dbus_padw_(w, mulib_dbus_align_(c))) return 0;
    switch (c) {
        case 'y': if (v->u64 > UINT8_MAX) goto invalid; return mulib_dbus_uintw_(w, v->u64, 1);
        case 'b': if (v->u64 > 1) goto invalid; return mulib_dbus_uintw_(w, v->u64, 4);
        case 'q': if (v->u64 > UINT16_MAX) goto invalid; return mulib_dbus_uintw_(w, v->u64, 2);
        case 'u': if (v->u64 > UINT32_MAX) goto invalid; return mulib_dbus_uintw_(w, v->u64, 4);
        case 't': return mulib_dbus_uintw_(w, v->u64, 8);
        case 'n': if (v->i64 < INT16_MIN || v->i64 > INT16_MAX) goto invalid; return mulib_dbus_uintw_(w, (uint64_t)v->i64, 2);
        case 'i': if (v->i64 < INT32_MIN || v->i64 > INT32_MAX) goto invalid; return mulib_dbus_uintw_(w, (uint64_t)v->i64, 4);
        case 'x': return mulib_dbus_uintw_(w, (uint64_t)v->i64, 8);
        case 'd': {
            uint64_t bits;
            if (sizeof(double) != 8 || DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024) goto invalid;
            memcpy(&bits, &v->real, 8); return mulib_dbus_uintw_(w, bits, 8);
        }
        case 's': case 'o': case 'g': return mulib_dbus_textw_(w, c, v->text, v->length);
        case 'a': {
            size_t length_pos, data_start;
            start = *p; stop = start;
            if (!mulib_dbus_sig_one_(s, n, &stop, 0, 1)) goto invalid;
            length_pos = w->b->length;
            if (!mulib_dbus_uintw_(w, 0, 4) || !mulib_dbus_padw_(w, mulib_dbus_align_(s[start]))) return 0;
            data_start = w->b->length;
            for (size_t i = 0; i < v->count; ++i) { size_t q = start; if (!mulib_dbus_write_(w, s, stop, &q, &v->children[i], depth+1) || q != stop) return 0; }
            if (w->b->length-data_start > 64u*1024u*1024u) { w->error = MULIB_DBUS_LIMIT; return 0; }
            mulib_dbus_store32_(w->b->data+length_pos, (uint32_t)(w->b->length-data_start), w->little);
            *p = stop; return 1;
        }
        case '(': case '{': {
            size_t i = 0; char close = c == '(' ? ')' : '}';
            while (*p < n && s[*p] != close) {
                if (i == v->count || !mulib_dbus_write_(w, s, n, p, &v->children[i++], depth+1)) return 0;
            }
            if (*p == n || i != v->count) goto invalid;
            ++*p; return 1;
        }
        case 'v': {
            size_t q = 0;
            if (v->count != 1 || !mulib_dbus_single_(v->signature)) goto invalid;
            if (!mulib_dbus_textw_(w, 'g', v->signature, strlen(v->signature))) return 0;
            return mulib_dbus_write_(w, v->signature, strlen(v->signature), &q, &v->children[0], depth+1);
        }
        default: break;
    }
invalid:
    w->error = MULIB_DBUS_INVALID; return 0;
}
typedef struct mulib_dbus_reader_ {
    const unsigned char *s; size_t n, p; int little; mulib_dbus_result error;
    mulib_dbus_limits_t limits; size_t nodes;
} mulib_dbus_reader_;
static int mulib_dbus_padr_(mulib_dbus_reader_ *r, size_t align) {
    size_t pad = (align-r->p%align)%align;
    if (pad > r->n-r->p) { r->error = MULIB_DBUS_INVALID; return 0; }
    while (pad--) if (r->s[r->p++] != 0) { r->error = MULIB_DBUS_INVALID; return 0; }
    return 1;
}
static int mulib_dbus_uintr_(mulib_dbus_reader_ *r, size_t width, uint64_t *out) {
    uint64_t v = 0;
    if (width > r->n-r->p) { r->error = MULIB_DBUS_INVALID; return 0; }
    if (width == 4) v = mulib_dbus_load32_(r->s+r->p, r->little);
    else if (width == 8) v = (uint64_t)mulib_dbus_load32_(r->s+r->p+(r->little ? 0 : 4), r->little) | ((uint64_t)mulib_dbus_load32_(r->s+r->p+(r->little ? 4 : 0), r->little) << 32);
    else for (size_t i = 0; i < width; ++i) v |= (uint64_t)r->s[r->p+(r->little ? i : width-1-i)] << (8*i);
    r->p += width; *out = v; return 1;
}
static char *mulib_dbus_copy_(const char *s, size_t n) {
    mulib_strbuf_t b; mulib_strbuf_init(&b);
    if (!mulib_strbuf_append_n(&b, s, n)) return NULL;
    return b.data;
}
static int mulib_dbus_textr_(mulib_dbus_reader_ *r, char c, char **text, size_t *length) {
    uint64_t n;
    if (!mulib_dbus_uintr_(r, c == 'g' ? 1 : 4, &n)) return 0;
    if (n >= r->n-r->p || r->s[r->p+(size_t)n] != 0 || !mulib_dbus_text_valid_(c, (const char *)r->s+r->p, (size_t)n)) { r->error = MULIB_DBUS_INVALID; return 0; }
    *text = mulib_dbus_copy_((const char *)r->s+r->p, (size_t)n);
    if (!*text) { r->error = MULIB_DBUS_NOMEM; return 0; }
    *length = (size_t)n; r->p += (size_t)n+1; return 1;
}
static mulib_dbus_value_t *mulib_dbus_child_(mulib_dbus_reader_ *r, mulib_dbus_value_t *v, size_t *cap) {
    if (v->count >= r->limits.max_values || v->count == SIZE_MAX/sizeof(*v->children)) { r->error = MULIB_DBUS_LIMIT; return NULL; }
    if (v->count == *cap) {
        size_t next = *cap ? *cap*2 : 4; mulib_dbus_value_t *p;
        if (*cap > SIZE_MAX/2 || next > SIZE_MAX/sizeof(*p)) { r->error = MULIB_DBUS_LIMIT; return NULL; }
        p = (mulib_dbus_value_t *)realloc(v->children, next*sizeof(*p));
        if (!p) { r->error = MULIB_DBUS_NOMEM; return NULL; }
        v->children = p; *cap = next;
    }
    memset(&v->children[v->count], 0, sizeof(*v->children));
    return &v->children[v->count++];
}
static int mulib_dbus_read_(mulib_dbus_reader_ *r, const char *s, size_t n, size_t *p, mulib_dbus_value_t *v, size_t depth) {
    char c; uint64_t u; size_t start, stop, cap = 0;
    if (depth > r->limits.max_depth || r->nodes >= r->limits.max_values) { r->error = MULIB_DBUS_LIMIT; return 0; }
    ++r->nodes;
    if (*p >= n) goto invalid;
    v->type = c = s[(*p)++];
    if (!mulib_dbus_padr_(r, mulib_dbus_align_(c))) return 0;
    switch (c) {
        case 'y': case 'b': case 'q': case 'u': case 't':
            if (!mulib_dbus_uintr_(r, c == 'y' ? 1 : c == 'q' ? 2 : c == 't' ? 8 : 4, &v->u64)) return 0;
            if (c == 'b' && v->u64 > 1) goto invalid;
            return 1;
        case 'n': case 'i': case 'x':
            if (!mulib_dbus_uintr_(r, c == 'n' ? 2 : c == 'i' ? 4 : 8, &u)) return 0;
            if (c == 'n') v->i64 = u <= INT16_MAX ? (int64_t)u : (int64_t)u-65536;
            else if (c == 'i') v->i64 = u <= INT32_MAX ? (int64_t)u : (int64_t)u-INT64_C(4294967296);
            else v->i64 = u <= INT64_MAX ? (int64_t)u : -1-(int64_t)(UINT64_MAX-u);
            return 1;
        case 'd':
            if (sizeof(double) != 8 || DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024) goto invalid;
            if (!mulib_dbus_uintr_(r, 8, &u)) return 0;
            memcpy(&v->real, &u, 8); return 1;
        case 's': case 'o': case 'g': return mulib_dbus_textr_(r, c, &v->text, &v->length);
        case 'a': {
            size_t data_end, old_n; uint64_t bytes;
            start = *p; stop = start;
            if (!mulib_dbus_sig_one_(s, n, &stop, 0, 1)) goto invalid;
            v->signature = mulib_dbus_copy_(s+start, stop-start);
            if (!v->signature) { r->error = MULIB_DBUS_NOMEM; return 0; }
            if (!mulib_dbus_uintr_(r, 4, &bytes) || !mulib_dbus_padr_(r, mulib_dbus_align_(s[start]))) return 0;
            if (bytes > 64u*1024u*1024u) { r->error = MULIB_DBUS_LIMIT; return 0; }
            if (bytes > r->n-r->p) goto invalid;
            data_end = r->p+(size_t)bytes; old_n = r->n; r->n = data_end;
            while (r->p < data_end) {
                size_t q = start; mulib_dbus_value_t *child = mulib_dbus_child_(r, v, &cap);
                if (!child || !mulib_dbus_read_(r, s, stop, &q, child, depth+1)) { r->n = old_n; return 0; }
            }
            r->n = old_n; *p = stop; return 1;
        }
        case '(': case '{': {
            char close = c == '(' ? ')' : '}';
            while (*p < n && s[*p] != close) {
                mulib_dbus_value_t *child = mulib_dbus_child_(r, v, &cap);
                if (!child || !mulib_dbus_read_(r, s, n, p, child, depth+1)) return 0;
            }
            if (*p == n) goto invalid;
            ++*p; return 1;
        }
        case 'v': {
            size_t length, q = 0; mulib_dbus_value_t *child;
            if (!mulib_dbus_textr_(r, 'g', &v->signature, &length)) return 0;
            if (!mulib_dbus_single_(v->signature)) goto invalid;
            child = mulib_dbus_child_(r, v, &cap);
            return child && mulib_dbus_read_(r, v->signature, length, &q, child, depth+1);
        }
        default: break;
    }
invalid:
    r->error = MULIB_DBUS_INVALID; return 0;
}
static int mulib_dbus_fieldw_(mulib_dbus_writer_ *w, unsigned code, char type, const char *text, uint32_t number) {
    mulib_dbus_value_t field, parts[2], value; size_t p = 0; char sig[2];
    memset(&field, 0, sizeof(field)); memset(parts, 0, sizeof(parts)); memset(&value, 0, sizeof(value));
    sig[0] = type; sig[1] = 0;
    field.type = '('; field.children = parts; field.count = 2;
    parts[0].type = 'y'; parts[0].u64 = code;
    parts[1].type = 'v'; parts[1].signature = sig; parts[1].children = &value; parts[1].count = 1;
    value.type = type; value.u64 = number; value.text = (char *)text; value.length = text ? strlen(text) : 0;
    return mulib_dbus_write_(w, "(yv)", 4, &p, &field, 0);
}
mulib_dbus_result mulib_dbus_encode(const mulib_dbus_message_t *m,
    const mulib_dbus_limits_t *limits, unsigned char **bytes, size_t *length) {
    mulib_buffer_t b, body; mulib_dbus_writer_ w; char *signature = NULL;
    size_t p = 0, header_length; static const unsigned char zero[16] = {0};
    mulib_dbus_result signature_error = MULIB_DBUS_INVALID;
    if (!m || !bytes || !length || !mulib_dbus_message_valid_(m)) return MULIB_DBUS_INVALID;
    if (!mulib_dbus_derive_(m->body, m->count, &signature, &signature_error)) return signature_error;
    if (m->signature && strcmp(m->signature, signature)) { free(signature); return MULIB_DBUS_INVALID; }
    mulib_buffer_init(&b); mulib_buffer_init(&body);
    memset(&w, 0, sizeof(w)); w.b = &b; w.little = m->endian == 'l'; w.limits = mulib_dbus_limits_(limits); w.error = MULIB_DBUS_INVALID;
    if (!mulib_dbus_put_(&w, zero, 16)) goto fail;
    if ((m->path && !mulib_dbus_fieldw_(&w, 1, 'o', m->path, 0)) ||
        (m->interface && !mulib_dbus_fieldw_(&w, 2, 's', m->interface, 0)) ||
        (m->member && !mulib_dbus_fieldw_(&w, 3, 's', m->member, 0)) ||
        (m->error_name && !mulib_dbus_fieldw_(&w, 4, 's', m->error_name, 0)) ||
        (m->reply_serial && !mulib_dbus_fieldw_(&w, 5, 'u', NULL, m->reply_serial)) ||
        (m->destination && !mulib_dbus_fieldw_(&w, 6, 's', m->destination, 0)) ||
        (m->sender && !mulib_dbus_fieldw_(&w, 7, 's', m->sender, 0)) ||
        (*signature && !mulib_dbus_fieldw_(&w, 8, 'g', signature, 0))) goto fail;
    header_length = b.length-16;
    if (header_length > 64u*1024u*1024u) { w.error = MULIB_DBUS_LIMIT; goto fail; }
    if (!mulib_dbus_padw_(&w, 8)) goto fail;
    w.b = &body;
    for (size_t i = 0; i < m->count; ++i) if (!mulib_dbus_write_(&w, signature, strlen(signature), &p, &m->body[i], 0)) goto fail;
    if (body.length > UINT32_MAX) { w.error = MULIB_DBUS_LIMIT; goto fail; }
    w.b = &b;
    if (!mulib_dbus_put_(&w, body.data, body.length)) goto fail;
    b.data[0] = (unsigned char)m->endian; b.data[1] = m->type; b.data[2] = m->flags; b.data[3] = 1;
    mulib_dbus_store32_(b.data+4, (uint32_t)body.length, w.little);
    mulib_dbus_store32_(b.data+8, m->serial, w.little);
    mulib_dbus_store32_(b.data+12, (uint32_t)header_length, w.little);
    mulib_buffer_free(&body); free(signature); *bytes = b.data; *length = b.length; return MULIB_DBUS_OK;
fail:
    mulib_buffer_free(&b); mulib_buffer_free(&body); free(signature); return w.error;
}
mulib_dbus_result mulib_dbus_decode(const void *bytes, size_t n,
    const mulib_dbus_limits_t *limits, mulib_dbus_message_t *out, size_t *consumed) {
    const unsigned char *s = (const unsigned char *)bytes; mulib_dbus_message_t m;
    mulib_dbus_reader_ r; mulib_dbus_value_t values; unsigned char seen[256] = {0};
    size_t hlen, body_start, total, cap = 0, p = 0; uint32_t blen;
    if (consumed) *consumed = 0;
    if ((!s && n) || !out) return MULIB_DBUS_INVALID;
    if (n < 16) return MULIB_DBUS_MORE;
    if ((s[0] != 'l' && s[0] != 'B') || s[1] < 1 || s[1] > 4 || (s[2] & ~7u) || s[3] != 1) return MULIB_DBUS_INVALID;
    memset(&m, 0, sizeof(m)); memset(&r, 0, sizeof(r)); memset(&values, 0, sizeof(values));
    r.s = s; r.little = s[0] == 'l'; r.limits = mulib_dbus_limits_(limits); r.error = MULIB_DBUS_INVALID;
    m.endian = (char)s[0]; m.type = s[1]; m.flags = s[2]; m.serial = mulib_dbus_load32_(s+8, r.little);
    if (!m.serial) return MULIB_DBUS_INVALID;
    hlen = mulib_dbus_load32_(s+12, r.little); blen = mulib_dbus_load32_(s+4, r.little);
    if (hlen > 64u*1024u*1024u || hlen > SIZE_MAX-23) return MULIB_DBUS_LIMIT;
    body_start = (16+hlen+7)&~(size_t)7;
    if (body_start > r.limits.max_bytes || blen > r.limits.max_bytes-body_start || blen > SIZE_MAX-body_start) return MULIB_DBUS_LIMIT;
    total = body_start+blen;
    if (n < total) return MULIB_DBUS_MORE;
    r.n = 16+hlen; r.p = 16;
    while (r.p < r.n) {
        mulib_dbus_value_t field; mulib_dbus_value_t *variant, *v; unsigned code; char **dest = NULL;
        memset(&field, 0, sizeof(field)); p = 0;
        if (!mulib_dbus_read_(&r, "(yv)", 4, &p, &field, 0)) { mulib_dbus_value_free(&field); goto fail; }
        code = (unsigned)field.children[0].u64; variant = &field.children[1]; v = &variant->children[0];
        if (!code || seen[code]) { mulib_dbus_value_free(&field); goto fail; }
        seen[code] = 1;
        switch (code) {
            case 1: dest = &m.path; break; case 2: dest = &m.interface; break;
            case 3: dest = &m.member; break; case 4: dest = &m.error_name; break;
            case 6: dest = &m.destination; break; case 7: dest = &m.sender; break;
            case 8: dest = &m.signature; break;
            case 5: case 9:
                if (v->type != 'u' || (code == 5 && !v->u64) || (code == 9 && v->u64)) { mulib_dbus_value_free(&field); goto fail; }
                if (code == 5) m.reply_serial = (uint32_t)v->u64;
                break;
            default: break;
        }
        if (dest) {
            char expected = code == 1 ? 'o' : code == 8 ? 'g' : 's';
            if (v->type != expected) { mulib_dbus_value_free(&field); goto fail; }
            *dest = v->text; v->text = NULL;
        }
        mulib_dbus_value_free(&field);
    }
    r.n = total;
    if (!mulib_dbus_padr_(&r, 8) || r.p != body_start) goto fail;
    if (!m.signature) {
        m.signature = mulib_dbus_copy_("", 0);
        if (!m.signature) { r.error = MULIB_DBUS_NOMEM; goto fail; }
    }
    p = 0;
    while (p < strlen(m.signature)) {
        mulib_dbus_value_t *v = mulib_dbus_child_(&r, &values, &cap);
        if (!v || !mulib_dbus_read_(&r, m.signature, strlen(m.signature), &p, v, 0)) goto fail;
    }
    m.body = values.children; m.count = values.count; values.children = NULL; values.count = 0;
    if (r.p != total || !mulib_dbus_message_valid_(&m)) goto fail;
    *out = m; if (consumed) *consumed = total; return MULIB_DBUS_OK;
fail:
    mulib_dbus_value_free(&values); mulib_dbus_message_free(&m); return r.error;
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
