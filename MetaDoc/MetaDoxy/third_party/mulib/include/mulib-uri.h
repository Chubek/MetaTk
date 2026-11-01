/**
 * @file mulib-uri.h
 * @brief RFC 3986 ASCII URI references, composition, and relative resolution.
 * Parsing borrows the input, preserves escapes/case, and never decodes delimiters.
 * Define MULIB_IMPL_URI once; also implement the mulib-strbuf dependency.
 */
#ifndef MULIB_URI_H
#define MULIB_URI_H
#include <stddef.h>
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief A borrowed component (not necessarily NUL terminated). */
typedef struct mulib_uri_slice { const char *data; size_t length; } mulib_uri_slice_t;
/** @brief Parsed reference; has_* distinguish absent from empty components. */
typedef struct mulib_uri {
    mulib_uri_slice_t scheme, userinfo, host, port, path, query, fragment;
    int has_authority, has_userinfo, has_port, has_query, has_fragment;
} mulib_uri_t;
/** @brief Parse exactly length ASCII bytes. Return 1, or 0 (output unchanged). */
int mulib_uri_parse(const char *text, size_t length, mulib_uri_t *out);
/** @brief Validate and compose; return malloc-owned text, or NULL.
 * A no-authority path beginning // is protected with /. to preserve its meaning. */
char *mulib_uri_compose(const mulib_uri_t *uri);
/** @brief Resolve a reference against an absolute base per RFC 3986 section 5.2.
 * Return malloc-owned text, or NULL. No percent-decoding or case normalization. */
char *mulib_uri_resolve(const mulib_uri_t *base, const mulib_uri_t *reference);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_URI_H */

#if defined(MULIB_IMPL_URI) && !defined(MULIB_URI_IMPLEMENTED)
#define MULIB_URI_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_uri_alpha_(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int mulib_uri_digit_(unsigned char c) { return c >= '0' && c <= '9'; }
static int mulib_uri_hex_(unsigned char c) {
    return mulib_uri_digit_(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
static int mulib_uri_unreserved_(unsigned char c) {
    return mulib_uri_alpha_(c) || mulib_uri_digit_(c) || c == '-' || c == '.' || c == '_' || c == '~';
}
static int mulib_uri_sub_(unsigned char c) {
    return c != 0 && strchr("!$&'()*+,;=", c) != NULL;
}
static int mulib_uri_component_(const char *s, size_t n, const char *extra) {
    size_t i;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '%') {
            if (n - i < 3 || !mulib_uri_hex_((unsigned char)s[i+1]) || !mulib_uri_hex_((unsigned char)s[i+2])) return 0;
            i += 2;
        } else if (!mulib_uri_unreserved_(c) && !mulib_uri_sub_(c) && !(c && strchr(extra, c))) return 0;
    }
    return 1;
}
static int mulib_uri_ipv4_(const char *s, size_t n) {
    size_t i = 0; unsigned parts = 0;
    while (i < n) {
        size_t start = i; unsigned v = 0;
        while (i < n && mulib_uri_digit_((unsigned char)s[i])) {
            if (i - start == 3) return 0;
            v = v * 10 + (unsigned)(s[i++] - '0');
        }
        if (i == start || v > 255 || (i - start > 1 && s[start] == '0')) return 0;
        ++parts;
        if (i == n) break;
        if (s[i++] != '.' || i == n) return 0;
    }
    return parts == 4;
}
static int mulib_uri_literal_(const char *s, size_t n) {
    size_t i = 0; unsigned groups = 0; int compressed = 0;
    if (n && (s[0] == 'v' || s[0] == 'V')) {
        i = 1;
        while (i < n && mulib_uri_hex_((unsigned char)s[i])) ++i;
        if (i == 1 || i == n || s[i++] != '.' || i == n) return 0;
        /* IPvFuture does not permit percent escapes. */
        for (; i < n; ++i) if (!mulib_uri_unreserved_((unsigned char)s[i]) && !mulib_uri_sub_((unsigned char)s[i]) && s[i] != ':') return 0;
        return 1;
    }
    if (n < 2) return 0;
    if (s[0] == ':') {
        if (s[1] != ':') return 0;
        compressed = 1; i = 2;
    }
    while (i < n) {
        size_t start = i;
        while (i < n && s[i] != ':') ++i;
        if (memchr(s + start, '.', i - start)) {
            if (i != n || !mulib_uri_ipv4_(s + start, i - start)) return 0;
            groups += 2; break;
        }
        if (i == start || i - start > 4) return 0;
        for (size_t j = start; j < i; ++j) if (!mulib_uri_hex_((unsigned char)s[j])) return 0;
        if (++groups > 8) return 0;
        if (i < n) {
            ++i;
            if (i < n && s[i] == ':') {
                if (compressed) return 0;
                compressed = 1; ++i;
            } else if (i == n) return 0;
        }
    }
    return compressed ? groups < 8 : groups == 8;
}
static mulib_uri_slice_t mulib_uri_slice_(const char *s, size_t n) {
    mulib_uri_slice_t v; v.data = s; v.length = n; return v;
}
int mulib_uri_parse(const char *text, size_t length, mulib_uri_t *out) {
    mulib_uri_t u; size_t i = 0, end = length, q, p;
    if (!text || !out) return 0;
    memset(&u, 0, sizeof(u));
    for (p = 0; p < length; ++p) if ((unsigned char)text[p] <= 32 || (unsigned char)text[p] >= 127) return 0;
    for (p = 0; p < length && text[p] != '/' && text[p] != '?' && text[p] != '#'; ++p) {
        if (text[p] == ':') {
            if (!p || !mulib_uri_alpha_((unsigned char)text[0])) return 0;
            for (q = 1; q < p; ++q) if (!mulib_uri_alpha_((unsigned char)text[q]) && !mulib_uri_digit_((unsigned char)text[q]) && !strchr("+-.", text[q])) return 0;
            u.scheme = mulib_uri_slice_(text, p); i = p + 1; break;
        }
    }
    for (p = i; p < length; ++p) if (text[p] == '#') {
        u.has_fragment = 1; u.fragment = mulib_uri_slice_(text + p + 1, length - p - 1); end = p; break;
    }
    for (p = i; p < end; ++p) if (text[p] == '?') {
        u.has_query = 1; u.query = mulib_uri_slice_(text + p + 1, end - p - 1); end = p; break;
    }
    if (end - i >= 2 && text[i] == '/' && text[i+1] == '/') {
        size_t a, ae, host_start;
        u.has_authority = 1; a = i + 2;
        for (ae = a; ae < end && text[ae] != '/'; ++ae) {}
        host_start = a;
        for (p = a; p < ae; ++p) if (text[p] == '@') {
            if (u.has_userinfo) return 0;
            u.has_userinfo = 1; u.userinfo = mulib_uri_slice_(text + a, p - a); host_start = p + 1;
        }
        p = host_start;
        if (p < ae && text[p] == '[') {
            for (q = p + 1; q < ae && text[q] != ']'; ++q) {}
            if (q == ae || !mulib_uri_literal_(text + p + 1, q - p - 1)) return 0;
            u.host = mulib_uri_slice_(text + p, q - p + 1); p = q + 1;
            if (p < ae && text[p] != ':') return 0;
        } else {
            for (; p < ae && text[p] != ':'; ++p) {}
            u.host = mulib_uri_slice_(text + host_start, p - host_start);
            if (!mulib_uri_component_(u.host.data, u.host.length, "")) return 0;
        }
        if (p < ae) {
            u.has_port = 1; u.port = mulib_uri_slice_(text + p + 1, ae - p - 1);
            for (++p; p < ae; ++p) if (!mulib_uri_digit_((unsigned char)text[p])) return 0;
        }
        i = ae;
    }
    u.path = mulib_uri_slice_(text + i, end - i);
    if (!mulib_uri_component_(u.userinfo.data, u.userinfo.length, ":") ||
        !mulib_uri_component_(u.path.data, u.path.length, ":@/") ||
        !mulib_uri_component_(u.query.data, u.query.length, ":@/?") ||
        !mulib_uri_component_(u.fragment.data, u.fragment.length, ":@/?")) return 0;
    *out = u; return 1;
}
static int mulib_uri_put_(mulib_strbuf_t *b, mulib_uri_slice_t s) {
    return (!s.length || s.data) && mulib_strbuf_append_n(b, s.data, s.length);
}
char *mulib_uri_compose(const mulib_uri_t *u) {
    mulib_strbuf_t b; mulib_uri_t check; size_t path_prefix;
    if (!u || (u->scheme.length && !u->scheme.data) ||
        (u->path.length && !u->path.data) ||
        (!u->has_authority && (u->has_userinfo || u->has_port || u->host.length || u->userinfo.length || u->port.length)) ||
        (!u->has_userinfo && u->userinfo.length) || (!u->has_port && u->port.length) ||
        (!u->has_query && u->query.length) || (!u->has_fragment && u->fragment.length)) return NULL;
    /* Dot-segment removal can expose // without an authority (e.g. /a/..//c).
     * Serialize that path as /.//c, not //c, which would introduce an authority. */
    path_prefix = !u->has_authority && u->path.length >= 2 && u->path.data[0] == '/' && u->path.data[1] == '/' ? 2 : 0;
    mulib_strbuf_init(&b);
    if (u->scheme.length && (!mulib_uri_put_(&b, u->scheme) || !mulib_strbuf_putc(&b, ':'))) goto fail;
    if (u->has_authority) {
        if (!mulib_strbuf_append(&b, "//") ||
            (u->has_userinfo && (!mulib_uri_put_(&b, u->userinfo) || !mulib_strbuf_putc(&b, '@'))) ||
            !mulib_uri_put_(&b, u->host) ||
            (u->has_port && (!mulib_strbuf_putc(&b, ':') || !mulib_uri_put_(&b, u->port)))) goto fail;
    }
    if ((path_prefix && !mulib_strbuf_append(&b, "/.")) || !mulib_uri_put_(&b, u->path) ||
        (u->has_query && (!mulib_strbuf_putc(&b, '?') || !mulib_uri_put_(&b, u->query))) ||
        (u->has_fragment && (!mulib_strbuf_putc(&b, '#') || !mulib_uri_put_(&b, u->fragment)))) goto fail;
    if (!mulib_uri_parse(b.data, b.length, &check)) goto fail;
    /* Delimiters in components must not change the supplied component layout. */
    if (check.scheme.length != u->scheme.length || check.has_authority != !!u->has_authority ||
        check.has_userinfo != !!u->has_userinfo || check.has_port != !!u->has_port ||
        check.has_query != !!u->has_query || check.has_fragment != !!u->has_fragment ||
        check.userinfo.length != u->userinfo.length || check.host.length != u->host.length ||
        check.port.length != u->port.length || check.path.length < path_prefix || check.path.length - path_prefix != u->path.length ||
        check.query.length != u->query.length || check.fragment.length != u->fragment.length) goto fail;
    return b.data;
fail:
    mulib_strbuf_free(&b); return NULL;
}
static void mulib_uri_pop_(mulib_strbuf_t *out) {
    size_t n = out->length;
    while (n && out->data[n-1] != '/') --n;
    out->length = n ? n - 1 : 0;
    if (out->data) out->data[out->length] = 0;
}
static char *mulib_uri_dots_(const char *s, size_t n) {
    mulib_strbuf_t b; size_t i = 0;
    mulib_strbuf_init(&b);
    while (i < n) {
        size_t left = n - i;
        if (left >= 3 && !memcmp(s+i, "../", 3)) i += 3;
        else if (left >= 2 && !memcmp(s+i, "./", 2)) i += 2;
        else if (left >= 3 && !memcmp(s+i, "/./", 3)) i += 2;
        else if (left == 2 && !memcmp(s+i, "/.", 2)) { i += 2; if (!mulib_strbuf_putc(&b, '/')) goto fail; }
        else if (left >= 4 && !memcmp(s+i, "/../", 4)) { i += 3; mulib_uri_pop_(&b); }
        else if (left == 3 && !memcmp(s+i, "/..", 3)) { i += 3; mulib_uri_pop_(&b); if (!mulib_strbuf_putc(&b, '/')) goto fail; }
        else if ((left == 1 && s[i] == '.') || (left == 2 && !memcmp(s+i, "..", 2))) i = n;
        else {
            size_t start = i++;
            while (i < n && s[i] != '/') ++i;
            if (!mulib_strbuf_append_n(&b, s + start, i - start)) goto fail;
        }
    }
    if (!mulib_strbuf_append_n(&b, "", 0)) goto fail;
    return b.data;
fail:
    mulib_strbuf_free(&b); return NULL;
}
char *mulib_uri_resolve(const mulib_uri_t *base, const mulib_uri_t *ref) {
    mulib_uri_t t; char *bcheck, *rcheck, *path = NULL, *result;
    mulib_strbuf_t merged;
    if (!base || !ref || !base->scheme.length) return NULL;
    bcheck = mulib_uri_compose(base); rcheck = mulib_uri_compose(ref);
    if (!bcheck || !rcheck) { free(bcheck); free(rcheck); return NULL; }
    free(bcheck); free(rcheck); t = *ref;
    mulib_strbuf_init(&merged);
    if (!ref->scheme.length) {
        t.scheme = base->scheme;
        if (!ref->has_authority) {
            t.has_authority = base->has_authority; t.has_userinfo = base->has_userinfo;
            t.has_port = base->has_port; t.userinfo = base->userinfo; t.host = base->host; t.port = base->port;
            if (!ref->path.length) {
                t.path = base->path;
                if (!ref->has_query) { t.has_query = base->has_query; t.query = base->query; }
                goto compose;
            }
            if (ref->path.data[0] != '/') {
                size_t prefix = base->path.length;
                while (prefix && base->path.data[prefix-1] != '/') --prefix;
                if (base->has_authority && !base->path.length && !mulib_strbuf_putc(&merged, '/')) goto fail;
                if (!mulib_strbuf_append_n(&merged, base->path.data, prefix) || !mulib_uri_put_(&merged, ref->path)) goto fail;
                path = mulib_uri_dots_(merged.data, merged.length); goto setpath;
            }
        }
    }
    path = mulib_uri_dots_(ref->path.data, ref->path.length);
setpath:
    if (!path) goto fail;
    t.path = mulib_uri_slice_(path, strlen(path));
compose:
    result = mulib_uri_compose(&t); free(path); mulib_strbuf_free(&merged); return result;
fail:
    free(path); mulib_strbuf_free(&merged); return NULL;
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
