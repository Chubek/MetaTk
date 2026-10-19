/*
 * ToSExpr.c — S-expression codec for the MetaArch document tree.
 *
 * There is no de-facto C library for data S-expressions, so this is a small
 * hand-written, bounds-checked reader and a width-aware pretty printer. The
 * notation follows Scheme (R7RS) lexical conventions where they exist:
 *
 *   mapping   (:key value :other value)   keyword plist; (:) is empty
 *             keys that are not plain symbols use |pipe quoting|: :|a key|
 *   sequence  (v1 v2 v3)                  () is the empty sequence
 *   string    "text"   escapes: \" \\ \n \t \r \a \b \xHH;
 *   integer   42  -7
 *   real      1.5  6.02e23  +inf.0  -inf.0  +nan.0
 *   boolean   #t  #f
 *   null      #nil
 *   comment   ; to end of line, #| block |#, #; datum
 *
 * A list is a mapping iff its first element is a keyword; strings are always
 * quoted, so a sequence can never start with a keyword. Bare symbols are
 * rejected to keep the format unambiguous.
 */
#include "DSymspec.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 100

/* ======================================================================== */
/* Writer                                                                   */
/* ======================================================================== */

static bool sym_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           (c && strchr("_.+-*/<>=!?$%&~^@", c));
}

static bool plain_key(const char *k, size_t len) {
    if (!len) return false;
    for (size_t i = 0; i < len; i++)
        if (!sym_char((unsigned char)k[i])) return false;
    return true;
}

static void put_key(DsymBuf *b, const char *k, size_t len) {
    dsym_buf_putc(b, ':');
    if (plain_key(k, len)) {
        dsym_buf_append(b, k, len);
        return;
    }
    dsym_buf_putc(b, '|');
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)k[i];
        if (c == '|' || c == '\\') dsym_buf_putc(b, '\\'), dsym_buf_putc(b, (char)c);
        else if (c < 0x20 || c == 0x7F) dsym_buf_printf(b, "\\x%02x;", c);
        else dsym_buf_putc(b, (char)c);
    }
    dsym_buf_putc(b, '|');
}

static void put_string(DsymBuf *b, const char *s, size_t len) {
    dsym_buf_putc(b, '"');
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"': dsym_buf_puts(b, "\\\""); break;
        case '\\': dsym_buf_puts(b, "\\\\"); break;
        case '\n': dsym_buf_puts(b, "\\n"); break;
        case '\t': dsym_buf_puts(b, "\\t"); break;
        case '\r': dsym_buf_puts(b, "\\r"); break;
        default:
            if (c < 0x20 || c == 0x7F) dsym_buf_printf(b, "\\x%02x;", c);
            else dsym_buf_putc(b, (char)c);
        }
    }
    dsym_buf_putc(b, '"');
}

static void put_real(DsymBuf *b, double r) {
    if (isnan(r)) {
        dsym_buf_puts(b, "+nan.0");
        return;
    }
    if (isinf(r)) {
        dsym_buf_puts(b, r < 0 ? "-inf.0" : "+inf.0");
        return;
    }
    char buf[40];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, r);
        if (strtod(buf, NULL) == r) break;
    }
    if (!strpbrk(buf, ".eE")) strcat(buf, ".0");
    dsym_buf_puts(b, buf);
}

static void put_atom(DsymBuf *b, const DsymNode *n) {
    switch (n->kind) {
    case DSYM_NODE_NULL: dsym_buf_puts(b, "#nil"); break;
    case DSYM_NODE_BOOL: dsym_buf_puts(b, n->v.b ? "#t" : "#f"); break;
    case DSYM_NODE_INT: dsym_buf_printf(b, "%" PRId64, n->v.i); break;
    case DSYM_NODE_UINT: dsym_buf_printf(b, "%" PRIu64, n->v.u); break;
    case DSYM_NODE_REAL: put_real(b, n->v.r); break;
    case DSYM_NODE_STR: put_string(b, n->v.str.ptr, n->v.str.len); break;
    default: break;
    }
}

/* Width of the single-line rendering, giving up once it exceeds `limit`. */
static size_t flat_width(const DsymNode *n, size_t limit) {
    if (n->kind == DSYM_NODE_SEQ || n->kind == DSYM_NODE_MAP) {
        bool map = n->kind == DSYM_NODE_MAP;
        size_t cnt = map ? n->v.map.count : n->v.seq.count;
        size_t w = 2 + (map && cnt == 0); /* "(:)" */
        for (size_t i = 0; i < cnt && w <= limit; i++) {
            if (i) w++;
            if (map) {
                const DsymPair *p = &n->v.map.items[i];
                w += p->key_len + 2 + (plain_key(p->key, p->key_len) ? 0 : 2);
                w += flat_width(p->value, limit - (w < limit ? w : limit));
            } else {
                w += flat_width(n->v.seq.items[i], limit - (w < limit ? w : limit));
            }
        }
        return w;
    }
    if (n->kind == DSYM_NODE_STR) {
        size_t w = 2;
        for (size_t i = 0; i < n->v.str.len && w <= limit; i++) {
            unsigned char c = (unsigned char)n->v.str.ptr[i];
            w += (c == '"' || c == '\\' || c == '\n' || c == '\t' || c == '\r') ? 2
                 : (c < 0x20 || c == 0x7F)                                       ? 5
                                                                                 : 1;
        }
        return w;
    }
    return n->kind == DSYM_NODE_REAL ? 24 : n->kind == DSYM_NODE_NULL ? 4 : 20;
}

static void indent(DsymBuf *b, size_t col) {
    dsym_buf_putc(b, '\n');
    for (size_t i = 0; i < col; i++) dsym_buf_putc(b, ' ');
}

static void put_flat(DsymBuf *b, const DsymNode *n) {
    if (n->kind == DSYM_NODE_SEQ) {
        dsym_buf_putc(b, '(');
        for (size_t i = 0; i < n->v.seq.count; i++) {
            if (i) dsym_buf_putc(b, ' ');
            put_flat(b, n->v.seq.items[i]);
        }
        dsym_buf_putc(b, ')');
    } else if (n->kind == DSYM_NODE_MAP) {
        dsym_buf_putc(b, '(');
        if (!n->v.map.count) dsym_buf_putc(b, ':');
        for (size_t i = 0; i < n->v.map.count; i++) {
            if (i) dsym_buf_putc(b, ' ');
            put_key(b, n->v.map.items[i].key, n->v.map.items[i].key_len);
            dsym_buf_putc(b, ' ');
            put_flat(b, n->v.map.items[i].value);
        }
        dsym_buf_putc(b, ')');
    } else {
        put_atom(b, n);
    }
}

/* Pretty printer: a node is printed on one line when it fits in the
   remaining width; otherwise its elements go one per line, indented one
   column past the opening parenthesis (map values follow their key). */
static void put_pretty(DsymBuf *b, const DsymNode *n, size_t col) {
    size_t avail = col < WIDTH ? WIDTH - col : 20;
    if ((n->kind != DSYM_NODE_SEQ && n->kind != DSYM_NODE_MAP) ||
        flat_width(n, avail) <= avail) {
        put_flat(b, n);
        return;
    }
    dsym_buf_putc(b, '(');
    if (n->kind == DSYM_NODE_SEQ) {
        for (size_t i = 0; i < n->v.seq.count; i++) {
            if (i) indent(b, col + 1);
            put_pretty(b, n->v.seq.items[i], col + 1);
        }
    } else {
        for (size_t i = 0; i < n->v.map.count; i++) {
            const DsymPair *p = &n->v.map.items[i];
            if (i) indent(b, col + 1);
            size_t start = b->len;
            put_key(b, p->key, p->key_len);
            size_t kw = b->len - start;
            const DsymNode *v = p->value;
            bool compound = v->kind == DSYM_NODE_SEQ || v->kind == DSYM_NODE_MAP;
            size_t vcol = col + 1 + kw + 1;
            if (compound && vcol + 8 > WIDTH / 2 + 20 &&
                flat_width(v, WIDTH - vcol) > WIDTH - vcol) {
                indent(b, col + 3); /* value too wide to hang: break line */
                put_pretty(b, v, col + 3);
            } else {
                dsym_buf_putc(b, ' ');
                put_pretty(b, v, vcol);
            }
        }
    }
    dsym_buf_putc(b, ')');
}

bool dsym_sexpr_write(const DsymNode *root, DsymBuf *out, uint32_t flags,
                      DsymDiags *diags) {
    (void)diags;
    if (flags & DSYM_WRITE_COMPACT) put_flat(out, root);
    else put_pretty(out, root, 0);
    dsym_buf_putc(out, '\n');
    return !out->oom;
}

/* ======================================================================== */
/* Reader                                                                   */
/* ======================================================================== */

typedef struct SR {
    DsymArena *a;
    DsymDiags *diags;
    const char *file;
    const char *p, *end;
    uint32_t line;
    const char *line_start;
    bool failed;
} SR;

static void sr_err(SR *r, const char *msg) {
    if (r->failed) return;
    dsym_diag_add(r->diags, DSYM_ERROR, r->file, r->line,
                  (uint32_t)(r->p - r->line_start) + 1, NULL, "S-expression: %s", msg);
    r->failed = true;
}

static void advance(SR *r) {
    if (*r->p == '\n') {
        r->line++;
        r->line_start = r->p + 1;
    }
    r->p++;
}

static DsymNode *read_datum(SR *r, int depth);

static void skip_ws(SR *r) {
    while (r->p < r->end && !r->failed) {
        char c = *r->p;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f') {
            advance(r);
        } else if (c == ';') {
            while (r->p < r->end && *r->p != '\n') advance(r);
        } else if (c == '#' && r->p + 1 < r->end && r->p[1] == '|') {
            int nest = 1;
            advance(r), advance(r);
            while (r->p < r->end && nest) {
                if (*r->p == '|' && r->p + 1 < r->end && r->p[1] == '#') nest--, advance(r);
                else if (*r->p == '#' && r->p + 1 < r->end && r->p[1] == '|') nest++, advance(r);
                advance(r);
            }
            if (nest) sr_err(r, "unterminated block comment");
        } else if (c == '#' && r->p + 1 < r->end && r->p[1] == ';') {
            advance(r), advance(r);
            DsymArena *junk = r->a; /* datum comment: parse and drop */
            (void)read_datum(r, 0);
            (void)junk;
        } else {
            break;
        }
    }
}

static bool delim(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '(' ||
           c == ')' || c == '"' || c == ';';
}

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Reads a quoted run ending in `close` ('"' strings, '|' keys). */
static bool read_quoted(SR *r, char close, const char **out, size_t *len) {
    advance(r); /* opening quote */
    DsymBuf b;
    dsym_buf_init(&b);
    while (r->p < r->end && *r->p != close) {
        char c = *r->p;
        if (c != '\\') {
            dsym_buf_putc(&b, c);
            advance(r);
            continue;
        }
        advance(r);
        if (r->p >= r->end) break;
        char e = *r->p;
        advance(r);
        switch (e) {
        case 'n': dsym_buf_putc(&b, '\n'); break;
        case 't': dsym_buf_putc(&b, '\t'); break;
        case 'r': dsym_buf_putc(&b, '\r'); break;
        case 'a': dsym_buf_putc(&b, '\a'); break;
        case 'b': dsym_buf_putc(&b, '\b'); break;
        case '0': dsym_buf_putc(&b, '\0'); break;
        case 'x': {
            uint32_t v = 0;
            int nd = 0, h;
            while (r->p < r->end && (h = hexv(*r->p)) >= 0 && nd < 8) v = v << 4 | (uint32_t)h, nd++, advance(r);
            if (!nd || r->p >= r->end || *r->p != ';') {
                sr_err(r, "malformed \\x escape (expected \\xHH;)");
                dsym_buf_free(&b);
                return false;
            }
            advance(r);
            if (v < 0x100) {
                dsym_buf_putc(&b, (char)v);
            } else { /* code point: encode as UTF-8 */
                char u[4];
                size_t n;
                if (v < 0x800) u[0] = (char)(0xC0 | v >> 6), u[1] = (char)(0x80 | (v & 0x3F)), n = 2;
                else if (v < 0x10000) u[0] = (char)(0xE0 | v >> 12), u[1] = (char)(0x80 | ((v >> 6) & 0x3F)), u[2] = (char)(0x80 | (v & 0x3F)), n = 3;
                else u[0] = (char)(0xF0 | v >> 18), u[1] = (char)(0x80 | ((v >> 12) & 0x3F)), u[2] = (char)(0x80 | ((v >> 6) & 0x3F)), u[3] = (char)(0x80 | (v & 0x3F)), n = 4;
                dsym_buf_append(&b, u, n);
            }
            break;
        }
        case '\n': /* line continuation: skip leading whitespace */
            while (r->p < r->end && (*r->p == ' ' || *r->p == '\t')) advance(r);
            break;
        default: dsym_buf_putc(&b, e); break; /* \" \\ \| */
        }
    }
    if (r->p >= r->end) {
        sr_err(r, close == '"' ? "unterminated string" : "unterminated |symbol|");
        dsym_buf_free(&b);
        return false;
    }
    advance(r); /* closing quote */
    *len = b.len;
    *out = dsym_arena_strndup(r->a, b.data ? b.data : "", b.len);
    dsym_buf_free(&b);
    return *out != NULL;
}

/* keyword after ':' (plain symbol, |quoted| or empty for "(:)") */
static bool read_keyword(SR *r, const char **k, size_t *len, bool *quoted) {
    advance(r); /* ':' */
    *quoted = r->p < r->end && *r->p == '|';
    if (*quoted) return read_quoted(r, '|', k, len);
    const char *s = r->p;
    while (r->p < r->end && !delim(*r->p)) {
        if (!sym_char((unsigned char)*r->p)) {
            sr_err(r, "invalid character in keyword");
            return false;
        }
        advance(r);
    }
    *len = (size_t)(r->p - s);
    *k = dsym_arena_strndup(r->a, s, *len);
    return *k != NULL;
}

static DsymNode *read_atom(SR *r) {
    const char *s = r->p;
    while (r->p < r->end && !delim(*r->p)) advance(r);
    size_t n = (size_t)(r->p - s);
    char tok[128];
    if (n == 0 || n >= sizeof tok) {
        sr_err(r, n ? "token too long" : "unexpected character");
        return NULL;
    }
    memcpy(tok, s, n);
    tok[n] = 0;
    if (!strcmp(tok, "#t") || !strcmp(tok, "#true")) return dsym_node_bool(r->a, true);
    if (!strcmp(tok, "#f") || !strcmp(tok, "#false")) return dsym_node_bool(r->a, false);
    if (!strcmp(tok, "#nil") || !strcmp(tok, "#null")) return dsym_node_null(r->a);
    if (!strcmp(tok, "+inf.0")) return dsym_node_real(r->a, INFINITY);
    if (!strcmp(tok, "-inf.0")) return dsym_node_real(r->a, -INFINITY);
    if (!strcmp(tok, "+nan.0") || !strcmp(tok, "-nan.0")) return dsym_node_real(r->a, NAN);
    const char *d = tok + (tok[0] == '-' || tok[0] == '+');
    if (*d && strspn(d, "0123456789") == strlen(d)) {
        errno = 0;
        if (tok[0] == '-') {
            long long v = strtoll(tok, NULL, 10);
            if (!errno) return dsym_node_int(r->a, v);
        } else {
            unsigned long long v = strtoull(d, NULL, 10);
            if (!errno) return dsym_node_uint(r->a, v);
        }
        sr_err(r, "integer out of range");
        return NULL;
    }
    if ((*d >= '0' && *d <= '9') || (*d == '.' && d[1] >= '0' && d[1] <= '9')) {
        char *end;
        double v = strtod(tok, &end);
        if (!*end) return dsym_node_real(r->a, v);
    }
    sr_err(r, "bare symbols are not allowed (strings must be \"quoted\")");
    return NULL;
}

static DsymNode *read_datum(SR *r, int depth) {
    skip_ws(r);
    if (r->failed) return NULL;
    if (r->p >= r->end) {
        sr_err(r, "unexpected end of input");
        return NULL;
    }
    if (depth > 4096) {
        sr_err(r, "nesting too deep");
        return NULL;
    }
    uint32_t line = r->line, col = (uint32_t)(r->p - r->line_start) + 1;
    DsymNode *n = NULL;
    char c = *r->p;
    if (c == '(') {
        advance(r);
        skip_ws(r);
        if (r->p < r->end && *r->p == ':') {
            n = dsym_node_map(r->a);
            if (!n) return NULL;
            while (!r->failed) {
                skip_ws(r);
                if (r->p >= r->end) {
                    sr_err(r, "unterminated list");
                    return NULL;
                }
                if (*r->p == ')') {
                    advance(r);
                    break;
                }
                if (*r->p != ':') {
                    sr_err(r, "expected a :keyword in mapping");
                    return NULL;
                }
                const char *k;
                size_t kl;
                bool quoted;
                if (!read_keyword(r, &k, &kl, &quoted)) return NULL;
                skip_ws(r);
                if (kl == 0 && !quoted && n->v.map.count == 0 && r->p < r->end && *r->p == ')') {
                    advance(r); /* (:) */
                    break;
                }
                if (kl == 0 && !quoted) {
                    sr_err(r, "empty keyword");
                    return NULL;
                }
                for (size_t i = 0; i < n->v.map.count; i++)
                    if (n->v.map.items[i].key_len == kl && !memcmp(n->v.map.items[i].key, k, kl)) {
                        sr_err(r, "duplicate key in mapping");
                        return NULL;
                    }
                if (r->p < r->end && *r->p == ')') {
                    sr_err(r, "keyword without a value");
                    return NULL;
                }
                DsymNode *v = read_datum(r, depth + 1);
                if (!v || !dsym_map_put_ref(r->a, n, k, v)) return NULL;
                n->v.map.items[n->v.map.count - 1].key_len = kl;
            }
        } else {
            n = dsym_node_seq(r->a);
            if (!n) return NULL;
            while (!r->failed) {
                skip_ws(r);
                if (r->p >= r->end) {
                    sr_err(r, "unterminated list");
                    return NULL;
                }
                if (*r->p == ')') {
                    advance(r);
                    break;
                }
                if (*r->p == ':') {
                    sr_err(r, "keyword inside a sequence (mappings must start with a keyword)");
                    return NULL;
                }
                DsymNode *v = read_datum(r, depth + 1);
                if (!v || !dsym_seq_push(r->a, n, v)) return NULL;
            }
        }
    } else if (c == ')') {
        sr_err(r, "unbalanced ')'");
        return NULL;
    } else if (c == '"') {
        const char *s;
        size_t len;
        if (!read_quoted(r, '"', &s, &len)) return NULL;
        n = dsym_node_new(r->a, DSYM_NODE_STR);
        if (!n) return NULL;
        n->v.str.ptr = s;
        n->v.str.len = len;
        n->hints |= DSYM_HINT_QUOTED;
    } else if (c == ':') {
        sr_err(r, "keyword outside a mapping");
        return NULL;
    } else {
        n = read_atom(r);
    }
    if (n) {
        n->line = line;
        n->col = col;
    }
    return r->failed ? NULL : n;
}

DsymNode *dsym_sexpr_read(DsymArena *a, const char *data, size_t len,
                          const char *file, DsymDiags *diags) {
    SR R = {a, diags, file, data, data + len, 1, data, false};
    if (len >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
        (unsigned char)data[2] == 0xBF)
        R.p += 3;
    DsymNode *root = read_datum(&R, 0);
    if (!root) return NULL;
    skip_ws(&R);
    if (!R.failed && R.p < R.end) sr_err(&R, "trailing data after the document");
    return R.failed ? NULL : root;
}
