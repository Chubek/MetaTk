/**
 * @file mulib-http.h
 * @brief Bounded, length-aware HTTP/1.1 parsing and serialization (no sockets).
 * Define MULIB_IMPL_HTTP once, plus URI, STRBUF and BUFFER implementations.
 * See the manual for the deliberately strict framing policy.
 */
#ifndef MULIB_HTTP_H
#define MULIB_HTTP_H
#include <stddef.h>
#include "mulib-uri.h"
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Parser/serializer result. MORE means retain all bytes and retry. */
typedef enum mulib_http_result {
    MULIB_HTTP_MORE = 0, MULIB_HTTP_OK = 1, MULIB_HTTP_INVALID = -1,
    MULIB_HTTP_LIMIT = -2, MULIB_HTTP_NOMEM = -3
} mulib_http_result;
/** @brief EOF completes close-delimited responses; HEAD/CONNECT describe request context. */
enum { MULIB_HTTP_EOF = 1, MULIB_HTTP_HEAD_RESPONSE = 2, MULIB_HTTP_CONNECT_RESPONSE = 4 };
/** @brief Zero limits select 32768 header bytes, 100 headers, 16 MiB body. */
typedef struct mulib_http_options {
    size_t max_header_bytes, max_headers, max_body_bytes;
    unsigned flags;
} mulib_http_options_t;
/** @brief Owned name/value pair, with surrounding value whitespace removed. */
typedef struct mulib_http_header { char *name, *value; } mulib_http_header_t;
/** @brief Owned parsed message. Encoder borrows the same structure. */
typedef struct mulib_http_message {
    int is_response;
    char *method, *target;       /* request only */
    unsigned status;
    char *reason;                /* response only */
    mulib_http_header_t *headers;
    size_t header_count;
    unsigned char *body;         /* decoded chunk data, binary-safe */
    size_t body_length;
    int chunked, close_delimited;
} mulib_http_message_t;
/** @brief Free a parsed message; zero-initialized messages and NULL are safe. */
void mulib_http_message_free(mulib_http_message_t *message);
/** @brief Borrow the first case-insensitive matching header, or NULL. */
const char *mulib_http_header(const mulib_http_message_t *message, const char *name);
/** @brief Parse one message. response is 0 for requests, 1 for responses.
 * On OK, out owns allocations and consumed identifies the exact frame boundary.
 * On failure/MORE, out is unchanged and consumed is zero. EOF makes truncation invalid.
 */
mulib_http_result mulib_http_parse(const void *bytes, size_t length, int response,
    const mulib_http_options_t *options, mulib_http_message_t *out, size_t *consumed);
/** @brief Serialize to malloc-owned bytes (with an extra NUL not counted in length).
 * Existing framing headers must match the body. Missing framing is generated;
 * chunked selects canonical chunked output. Source fields are validated before
 * interpolation; generated wire and original start-line identity are revalidated.
 * On failure both output arguments are unchanged.
 */
mulib_http_result mulib_http_serialize(const mulib_http_message_t *message,
    const mulib_http_options_t *options, unsigned char **bytes, size_t *length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTTP_H */

#if defined(MULIB_IMPL_HTTP) && !defined(MULIB_HTTP_IMPLEMENTED)
#define MULIB_HTTP_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
static unsigned char mulib_http_lower_(unsigned char c) { return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a'-'A')) : c; }
static int mulib_http_equal_n_(const char *a, size_t n, const char *b, size_t m) {
    if (n != m) return 0;
    for (size_t i = 0; i < n; ++i) if (mulib_http_lower_((unsigned char)a[i]) != mulib_http_lower_((unsigned char)b[i])) return 0;
    return 1;
}
static int mulib_http_equal_(const char *a, const char *b) { return a && b && mulib_http_equal_n_(a, strlen(a), b, strlen(b)); }
static int mulib_http_token_(const char *s, size_t n) {
    if (!n) return 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            (c && strchr("!#$%&'*+-.^_`|~", c)))) return 0;
    }
    return 1;
}
static int mulib_http_value_(const char *s, size_t n) {
    for (size_t i = 0; i < n; ++i) if (((unsigned char)s[i] < 32 && s[i] != '\t') || (unsigned char)s[i] == 127) return 0;
    return 1;
}
static char *mulib_http_copy_(const char *s, size_t n) {
    char *p;
    if (n == SIZE_MAX || !(p = (char *)malloc(n+1))) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = 0; return p;
}
static mulib_http_options_t mulib_http_options_(const mulib_http_options_t *o) {
    mulib_http_options_t v = { 32768, 100, 16u * 1024u * 1024u, 0 };
    if (o) { if (o->max_header_bytes) v.max_header_bytes = o->max_header_bytes;
        if (o->max_headers) v.max_headers = o->max_headers;
        if (o->max_body_bytes) v.max_body_bytes = o->max_body_bytes;
        v.flags = o->flags; }
    return v;
}
void mulib_http_message_free(mulib_http_message_t *m) {
    if (!m) return;
    free(m->method); free(m->target); free(m->reason); free(m->body);
    for (size_t i = 0; i < m->header_count; ++i) { free(m->headers[i].name); free(m->headers[i].value); }
    free(m->headers); memset(m, 0, sizeof(*m));
}
const char *mulib_http_header(const mulib_http_message_t *m, const char *name) {
    if (!m || !name) return NULL;
    for (size_t i = 0; i < m->header_count; ++i) if (mulib_http_equal_(m->headers[i].name, name)) return m->headers[i].value;
    return NULL;
}
/* limit is an absolute end offset, not a limit on the whole supplied pipeline. */
static mulib_http_result mulib_http_line_(const char *s, size_t n, size_t start, size_t limit, size_t *end) {
    size_t stop = n < limit ? n : limit;
    for (size_t i = start; i < stop; ++i) {
        if (s[i] == '\n' || s[i] == 0) return MULIB_HTTP_INVALID;
        if (s[i] == '\r') {
            if (i + 1 >= stop) return n >= limit ? MULIB_HTTP_LIMIT : MULIB_HTTP_MORE;
            if (s[i+1] != '\n') return MULIB_HTTP_INVALID;
            *end = i; return MULIB_HTTP_OK;
        }
    }
    return n >= limit ? MULIB_HTTP_LIMIT : MULIB_HTTP_MORE;
}
static int mulib_http_decimal_(const char *s, size_t *out) {
    size_t v = 0;
    if (!s || !*s) return 0;
    for (; *s; ++s) {
        unsigned d = (unsigned char)*s - (unsigned)'0';
        if (d > 9 || v > (SIZE_MAX-d)/10) return 0;
        v = v*10 + d;
    }
    *out = v; return 1;
}
static int mulib_http_authority_(const char *s, size_t n, mulib_uri_t *u, char **storage) {
    mulib_strbuf_t b; mulib_strbuf_init(&b);
    if (!s) return 0;
    if (!mulib_strbuf_append(&b, "//") || !mulib_strbuf_append_n(&b, s, n)) { mulib_strbuf_free(&b); return -1; }
    if (!mulib_uri_parse(b.data, b.length, u) || !u->has_authority || !u->host.length || u->has_userinfo ||
        u->path.length || u->has_query || u->has_fragment || (u->has_port && !u->port.length)) { mulib_strbuf_free(&b); return 0; }
    if (u->has_port) {
        size_t port = 0;
        for (size_t i = 0; i < u->port.length; ++i) {
            port = port * 10 + (unsigned)(u->port.data[i] - '0');
            if (port > 65535) { mulib_strbuf_free(&b); return 0; }
        }
    }
    *storage = b.data; return 1;
}
static unsigned mulib_http_port_(const mulib_uri_t *u, unsigned fallback) {
    unsigned v = 0;
    if (!u->has_port) return fallback;
    for (size_t i = 0; i < u->port.length; ++i) { if (v > 65535) return 65536; v = v*10 + (unsigned)(u->port.data[i]-'0'); }
    return v;
}
static int mulib_http_target_(const mulib_http_message_t *m, const char *host) {
    mulib_uri_t h, t; char *hs = NULL, *ts = NULL; int ok = 0, parsed;
    size_t host_start = 0, host_end = host ? strlen(host) : 0;
    /* Validate the original bytes, not just the target extracted from generated
     * wire: otherwise CRLF can terminate the line and introduce new headers. */
    for (const unsigned char *p = (const unsigned char *)m->target; *p; ++p) if (*p <= 32 || *p >= 127) return 0;
    while (host_start < host_end && (host[host_start] == ' ' || host[host_start] == '\t')) ++host_start;
    while (host_end > host_start && (host[host_end-1] == ' ' || host[host_end-1] == '\t')) --host_end;
    parsed = mulib_http_authority_(host ? host+host_start : NULL, host_end-host_start, &h, &hs);
    if (parsed != 1) return parsed;
    if (!strcmp(m->target, "*")) { ok = !strcmp(m->method, "OPTIONS"); goto done; }
    if (!strcmp(m->method, "CONNECT")) {
        parsed = mulib_http_authority_(m->target, strlen(m->target), &t, &ts);
        if (parsed != 1) { ok = parsed; goto done; }
        if (!t.has_port) goto done;
    } else {
        if (!mulib_uri_parse(m->target, strlen(m->target), &t) || t.has_fragment || t.has_userinfo) goto done;
        if (!t.scheme.length) { ok = m->target[0] == '/' && !t.has_authority; goto done; }
        if ((!mulib_http_equal_n_(t.scheme.data, t.scheme.length, "http", 4) && !mulib_http_equal_n_(t.scheme.data, t.scheme.length, "https", 5)) ||
            !t.has_authority || !t.host.length || (t.has_port && !t.port.length)) goto done;
    }
    {
        unsigned def = t.scheme.length == 5 ? 443 : 80;
        unsigned tp = mulib_http_port_(&t, def), hp = mulib_http_port_(&h, def);
        ok = tp <= 65535 && hp <= 65535 && tp == hp && mulib_http_equal_n_(t.host.data, t.host.length, h.host.data, h.host.length);
    }
done:
    free(hs); free(ts); return ok;
}
static int mulib_http_no_body_(const mulib_http_message_t *m, unsigned flags) {
    return m->is_response && ((flags & MULIB_HTTP_HEAD_RESPONSE) || m->status < 200 || m->status == 204 || m->status == 304 ||
        ((flags & MULIB_HTTP_CONNECT_RESPONSE) && m->status >= 200 && m->status < 300));
}
mulib_http_result mulib_http_parse(const void *bytes, size_t n, int response,
    const mulib_http_options_t *options, mulib_http_message_t *out, size_t *consumed) {
    const char *s = (const char *)bytes; mulib_http_message_t m; mulib_buffer_t body;
    mulib_http_options_t o = mulib_http_options_(options);
    mulib_http_result r; size_t end, pos = 0, content_length = 0, cl_count = 0, te_count = 0, hosts = 0;
    const char *host = NULL;
    if (consumed) *consumed = 0;
    if ((!s && n) || !out || (response != 0 && response != 1) || (o.flags & ~7u)) return MULIB_HTTP_INVALID;
    memset(&m, 0, sizeof(m)); mulib_buffer_init(&body); m.is_response = response;
    r = mulib_http_line_(s, n, 0, o.max_header_bytes, &end); if (r != MULIB_HTTP_OK) goto fail;
    if (response) {
        if (end < 13 || memcmp(s, "HTTP/1.1 ", 9) || s[9] < '1' || s[9] > '5' || s[10] < '0' || s[10] > '9' || s[11] < '0' || s[11] > '9' || s[12] != ' ' || !mulib_http_value_(s+13, end-13)) { r = MULIB_HTTP_INVALID; goto fail; }
        m.status = (unsigned)(s[9]-'0')*100 + (unsigned)(s[10]-'0')*10 + (unsigned)(s[11]-'0');
        m.reason = mulib_http_copy_(s+13, end-13); if (!m.reason) { r = MULIB_HTTP_NOMEM; goto fail; }
    } else {
        size_t a = 0, b;
        while (a < end && s[a] != ' ') ++a;
        b = a < end ? a + 1 : end;
        while (b < end && s[b] != ' ') ++b;
        if (!mulib_http_token_(s, a) || a == end || b == a+1 || end-b != 9 || memcmp(s+b, " HTTP/1.1", 9)) { r = MULIB_HTTP_INVALID; goto fail; }
        m.method = mulib_http_copy_(s, a); m.target = mulib_http_copy_(s+a+1, b-a-1);
        if (!m.method || !m.target) { r = MULIB_HTTP_NOMEM; goto fail; }
    }
    pos = end+2;
    for (;;) {
        size_t colon, a, b; mulib_http_header_t h, *next;
        r = mulib_http_line_(s, n, pos, o.max_header_bytes, &end); if (r != MULIB_HTTP_OK) goto fail;
        if (end == pos) { pos = end+2; break; }
        if (m.header_count >= o.max_headers || m.header_count >= SIZE_MAX/sizeof(*next)) { r = MULIB_HTTP_LIMIT; goto fail; }
        for (colon = pos; colon < end && s[colon] != ':'; ++colon) {}
        if (colon == end || !mulib_http_token_(s+pos, colon-pos) || !mulib_http_value_(s+colon+1, end-colon-1)) { r = MULIB_HTTP_INVALID; goto fail; }
        a = colon+1; b = end;
        while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
        while (b > a && (s[b-1] == ' ' || s[b-1] == '\t')) --b;
        h.name = mulib_http_copy_(s+pos, colon-pos); h.value = mulib_http_copy_(s+a, b-a);
        if (!h.name || !h.value) { free(h.name); free(h.value); r = MULIB_HTTP_NOMEM; goto fail; }
        next = (mulib_http_header_t *)realloc(m.headers, (m.header_count+1)*sizeof(*next));
        if (!next) { free(h.name); free(h.value); r = MULIB_HTTP_NOMEM; goto fail; }
        m.headers = next; m.headers[m.header_count++] = h;
        if (mulib_http_equal_(h.name, "Content-Length")) {
            if (++cl_count != 1 || !mulib_http_decimal_(h.value, &content_length)) { r = MULIB_HTTP_INVALID; goto fail; }
        } else if (mulib_http_equal_(h.name, "Transfer-Encoding")) {
            if (++te_count != 1 || !mulib_http_equal_(h.value, "chunked")) { r = MULIB_HTTP_INVALID; goto fail; }
        } else if (mulib_http_equal_(h.name, "Host")) { ++hosts; host = h.value; }
        pos = end+2;
    }
    if ((cl_count && te_count) || hosts > 1 || (!response && hosts != 1)) { r = MULIB_HTTP_INVALID; goto fail; }
    if (!response) {
        int valid_target = mulib_http_target_(&m, host);
        if (valid_target != 1) { r = valid_target < 0 ? MULIB_HTTP_NOMEM : MULIB_HTTP_INVALID; goto fail; }
    }
    m.chunked = te_count != 0;
    if (mulib_http_no_body_(&m, o.flags)) {
        if (te_count || ((m.status < 200 || m.status == 204 || ((o.flags & MULIB_HTTP_CONNECT_RESPONSE) && m.status < 300)) && cl_count)) { r = MULIB_HTTP_INVALID; goto fail; }
    } else if (te_count) {
        for (;;) {
            size_t chunk = 0, line_limit = pos > SIZE_MAX-128 ? SIZE_MAX : pos+128;
            r = mulib_http_line_(s, n, pos, line_limit, &end); if (r != MULIB_HTTP_OK) goto fail;
            if (end == pos) { r = MULIB_HTTP_INVALID; goto fail; }
            for (size_t i = pos; i < end; ++i) {
                unsigned char c = (unsigned char)s[i]; unsigned d;
                if (c >= '0' && c <= '9') d = c-'0';
                else if (c >= 'a' && c <= 'f') d = c-'a'+10;
                else if (c >= 'A' && c <= 'F') d = c-'A'+10;
                else { r = MULIB_HTTP_INVALID; goto fail; }
                if (chunk > (SIZE_MAX-d)/16) { r = MULIB_HTTP_LIMIT; goto fail; }
                chunk = chunk*16+d;
            }
            pos = end+2;
            if (!chunk) {
                if (n-pos < 2) { r = MULIB_HTTP_MORE; goto fail; }
                if (s[pos] != '\r' || s[pos+1] != '\n') { r = MULIB_HTTP_INVALID; goto fail; }
                pos += 2; break;
            }
            if (chunk > o.max_body_bytes-body.length) { r = MULIB_HTTP_LIMIT; goto fail; }
            if (chunk > n-pos || n-pos-chunk < 2) { r = MULIB_HTTP_MORE; goto fail; }
            if (s[pos+chunk] != '\r' || s[pos+chunk+1] != '\n') { r = MULIB_HTTP_INVALID; goto fail; }
            if (!mulib_buffer_append(&body, s+pos, chunk)) { r = MULIB_HTTP_NOMEM; goto fail; }
            pos += chunk+2;
        }
    } else if (cl_count) {
        if (content_length > o.max_body_bytes) { r = MULIB_HTTP_LIMIT; goto fail; }
        if (content_length > n-pos) { r = MULIB_HTTP_MORE; goto fail; }
        if (!mulib_buffer_append(&body, s+pos, content_length)) { r = MULIB_HTTP_NOMEM; goto fail; }
        pos += content_length;
    } else if (response) {
        m.close_delimited = 1;
        if (n-pos > o.max_body_bytes) { r = MULIB_HTTP_LIMIT; goto fail; }
        if (!(o.flags & MULIB_HTTP_EOF)) { r = MULIB_HTTP_MORE; goto fail; }
        if (!mulib_buffer_append(&body, s+pos, n-pos)) { r = MULIB_HTTP_NOMEM; goto fail; }
        pos = n;
    }
    m.body = body.data; m.body_length = body.length;
    *out = m; if (consumed) *consumed = pos; return MULIB_HTTP_OK;
fail:
    mulib_http_message_free(&m); mulib_buffer_free(&body);
    return r == MULIB_HTTP_MORE && (o.flags & MULIB_HTTP_EOF) ? MULIB_HTTP_INVALID : r;
}
mulib_http_result mulib_http_serialize(const mulib_http_message_t *m,
    const mulib_http_options_t *options, unsigned char **bytes, size_t *length) {
    mulib_http_options_t o = mulib_http_options_(options); mulib_strbuf_t b;
    mulib_http_message_t check; mulib_http_result r = MULIB_HTTP_NOMEM;
    char number[64]; size_t used = 0; int has_cl = 0, has_te = 0, no_body;
    if (!m || !bytes || !length || (m->is_response != 0 && m->is_response != 1) ||
        (!m->body && m->body_length) || (!m->headers && m->header_count) || (o.flags & ~7u)) return MULIB_HTTP_INVALID;
    if (m->body_length > o.max_body_bytes || m->header_count > o.max_headers) return MULIB_HTTP_LIMIT;
    /* Check caller-supplied fields before using them in a start line or header.
     * In particular Host must be bounded before target/authority validation. */
    for (size_t i = 0; i < m->header_count; ++i) {
        const char *name = m->headers[i].name, *value = m->headers[i].value;
        if (!name || !value || !mulib_http_token_(name, strlen(name)) || !mulib_http_value_(value, strlen(value))) return MULIB_HTTP_INVALID;
        if (strlen(name) > o.max_header_bytes || strlen(value) > o.max_header_bytes) return MULIB_HTTP_LIMIT;
    }
    mulib_strbuf_init(&b); memset(&check, 0, sizeof(check));
    if (m->is_response) {
        if (m->status < 100 || m->status > 599 || !m->reason || !mulib_http_value_(m->reason, strlen(m->reason))) { r = MULIB_HTTP_INVALID; goto fail; }
        if (strlen(m->reason) > o.max_header_bytes) { r = MULIB_HTTP_LIMIT; goto fail; }
        (void)snprintf(number, sizeof(number), "HTTP/1.1 %u ", m->status);
        if (!mulib_strbuf_append(&b, number) || !mulib_strbuf_append(&b, m->reason)) goto fail;
    } else {
        int valid_target;
        if (!m->method || !m->target || !mulib_http_token_(m->method, strlen(m->method))) { r = MULIB_HTTP_INVALID; goto fail; }
        if (strlen(m->method) > o.max_header_bytes || strlen(m->target) > o.max_header_bytes) { r = MULIB_HTTP_LIMIT; goto fail; }
        valid_target = mulib_http_target_(m, mulib_http_header(m, "Host"));
        if (valid_target != 1) { r = valid_target < 0 ? MULIB_HTTP_NOMEM : MULIB_HTTP_INVALID; goto fail; }
        if (!mulib_strbuf_append(&b, m->method) || !mulib_strbuf_putc(&b, ' ') || !mulib_strbuf_append(&b, m->target) || !mulib_strbuf_append(&b, " HTTP/1.1")) goto fail;
    }
    if (!mulib_strbuf_append(&b, "\r\n")) goto fail;
    for (size_t i = 0; i < m->header_count; ++i) {
        const char *name = m->headers[i].name, *value = m->headers[i].value;
        if (mulib_http_equal_(name, "Content-Length")) has_cl = 1;
        if (mulib_http_equal_(name, "Transfer-Encoding")) has_te = 1;
        if (!mulib_strbuf_append(&b, name) || !mulib_strbuf_append(&b, ": ") || !mulib_strbuf_append(&b, value) || !mulib_strbuf_append(&b, "\r\n")) goto fail;
        if (b.length > o.max_header_bytes) { r = MULIB_HTTP_LIMIT; goto fail; }
    }
    no_body = mulib_http_no_body_(m, o.flags);
    if ((no_body && (m->body_length || m->chunked)) || (has_te && !m->chunked) || (has_cl && m->chunked)) { r = MULIB_HTTP_INVALID; goto fail; }
    if (!has_cl && !has_te && !no_body) {
        if (m->chunked) { if (!mulib_strbuf_append(&b, "Transfer-Encoding: chunked\r\n")) goto fail; }
        else {
            (void)snprintf(number, sizeof(number), "Content-Length: %zu\r\n", m->body_length);
            if (!mulib_strbuf_append(&b, number)) goto fail;
        }
    }
    if (!mulib_strbuf_append(&b, "\r\n")) goto fail;
    if (b.length > o.max_header_bytes) { r = MULIB_HTTP_LIMIT; goto fail; }
    if (!no_body && m->chunked) {
        if (m->body_length) {
            (void)snprintf(number, sizeof(number), "%zx\r\n", m->body_length);
            if (!mulib_strbuf_append(&b, number) || !mulib_strbuf_append_n(&b, (const char *)m->body, m->body_length) || !mulib_strbuf_append(&b, "\r\n")) goto fail;
        }
        if (!mulib_strbuf_append(&b, "0\r\n\r\n")) goto fail;
    } else if (!no_body && !mulib_strbuf_append_n(&b, (const char *)m->body, m->body_length)) goto fail;
    o.flags |= MULIB_HTTP_EOF;
    r = mulib_http_parse(b.data, b.length, m->is_response, &o, &check, &used);
    if (r != MULIB_HTTP_OK) goto fail;
    if (used != b.length || check.body_length != m->body_length ||
        (!m->is_response && (strcmp(check.method, m->method) || strcmp(check.target, m->target))) ||
        (m->is_response && (check.status != m->status || strcmp(check.reason, m->reason))) ||
        (m->body_length && memcmp(check.body, m->body, m->body_length))) { r = MULIB_HTTP_INVALID; goto fail; }
    mulib_http_message_free(&check); *bytes = (unsigned char *)b.data; *length = b.length; return MULIB_HTTP_OK;
fail:
    mulib_http_message_free(&check); mulib_strbuf_free(&b); return r;
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
