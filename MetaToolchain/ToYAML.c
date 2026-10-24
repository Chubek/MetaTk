/*
 * ToYAML.c — YAML codec for the MetaArch document tree, built on libyaml
 * (https://github.com/yaml/libyaml), the reference C implementation used by
 * PyYAML and Ruby's Psych.
 *
 * Reading uses libyaml's event parser and resolves untagged plain scalars
 * with the YAML 1.2 *core schema* (null / bool / int / float / str), so
 * `0x90` is an integer and `sse4.1` a string, exactly as the schema expects.
 * Anchors and aliases are supported; multiple documents are rejected.
 *
 * Writing uses libyaml's emitter, which owns quoting/escaping. Style is
 * chosen to match the hand-written specs: block style for the document and
 * record lists, flow style for small records (`- {name: eax, ...}`), literal
 * blocks (`|`) for multi-line strings such as encoder bodies, and quoting for
 * any string the core schema would otherwise read back as a non-string.
 */
#include "DSymspec.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

/* ======================================================================== */
/* YAML 1.2 core schema resolution                                          */
/* ======================================================================== */

typedef enum Resolved { R_STR, R_NULL, R_BOOL, R_INT, R_UINT, R_REAL } Resolved;

static bool all_digits(const char *s, size_t n, int base) {
    if (n == 0) return false;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        bool ok = base == 16 ? ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                                (c >= 'A' && c <= 'F'))
                  : base == 8 ? (c >= '0' && c <= '7')
                  : base == 2 ? (c == '0' || c == '1')
                              : (c >= '0' && c <= '9');
        if (!ok) return false;
    }
    return true;
}

static bool eq3(const char *s, size_t n, const char *a, const char *b,
                const char *c) {
    return (strlen(a) == n && !memcmp(s, a, n)) ||
           (strlen(b) == n && !memcmp(s, b, n)) ||
           (strlen(c) == n && !memcmp(s, c, n));
}

/* Returns the resolved type and fills the value. */
static Resolved resolve_plain(const char *s, size_t n, bool *b, int64_t *i,
                              uint64_t *u, double *r) {
    if (n == 0 || (n == 1 && s[0] == '~') || eq3(s, n, "null", "Null", "NULL"))
        return R_NULL;
    if (eq3(s, n, "true", "True", "TRUE")) return *b = true, R_BOOL;
    if (eq3(s, n, "false", "False", "FALSE")) return *b = false, R_BOOL;

    /* integers: [-+]?[0-9]+ | 0o[0-7]+ | 0x[0-9a-fA-F]+ (+ 0b as extension) */
    {
        const char *p = s;
        size_t m = n;
        bool neg = false;
        int base = 10;
        if (m && (*p == '-' || *p == '+')) neg = *p == '-', p++, m--;
        if (m > 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'o' || p[1] == 'b') &&
            p == s) { /* prefixed forms are unsigned in the core schema */
            base = p[1] == 'x' ? 16 : p[1] == 'o' ? 8 : 2;
            p += 2;
            m -= 2;
        }
        if (all_digits(p, m, base)) {
            errno = 0;
            char tmp[80];
            if (m < sizeof tmp) {
                memcpy(tmp, p, m);
                tmp[m] = 0;
                unsigned long long v = strtoull(tmp, NULL, base);
                if (errno == 0) {
                    if (!neg) {
                        if (v <= (unsigned long long)INT64_MAX) return *i = (int64_t)v, R_INT;
                        return *u = v, R_UINT;
                    }
                    if (v <= (unsigned long long)INT64_MAX) return *i = -(int64_t)v, R_INT;
                    if (v == (unsigned long long)INT64_MAX + 1ull) return *i = INT64_MIN, R_INT;
                }
            }
            return R_STR; /* out of range: keep as text */
        }
    }

    /* floats: [-+]?(\.[0-9]+|[0-9]+(\.[0-9]*)?)([eE][-+]?[0-9]+)? */
    {
        const char *p = s, *e = s + n;
        if (p < e && (*p == '-' || *p == '+')) p++;
        if (e - p == 4 && p[0] == '.' && eq3(p + 1, 3, "inf", "Inf", "INF"))
            return *r = (s[0] == '-' ? -INFINITY : INFINITY), R_REAL;
        if (n == 4 && s[0] == '.' && eq3(s + 1, 3, "nan", "NaN", "NAN"))
            return *r = NAN, R_REAL;
        size_t intd = 0, fracd = 0;
        while (p < e && *p >= '0' && *p <= '9') p++, intd++;
        bool dot = false;
        if (p < e && *p == '.') {
            dot = true;
            p++;
            while (p < e && *p >= '0' && *p <= '9') p++, fracd++;
        }
        bool ok = intd > 0 || (dot && fracd > 0);
        bool expo = false;
        if (ok && p < e && (*p == 'e' || *p == 'E')) {
            expo = true;
            p++;
            if (p < e && (*p == '-' || *p == '+')) p++;
            size_t ed = 0;
            while (p < e && *p >= '0' && *p <= '9') p++, ed++;
            ok = ed > 0;
        }
        if (ok && p == e && (dot || expo)) {
            char tmp[128];
            if (n < sizeof tmp) {
                memcpy(tmp, s, n);
                tmp[n] = 0;
                return *r = strtod(tmp, NULL), R_REAL;
            }
        }
    }
    return R_STR;
}

/* ======================================================================== */
/* Reader                                                                   */
/* ======================================================================== */

typedef struct Anchor {
    char *name;
    DsymNode *node;
} Anchor;

typedef struct Frame {
    DsymNode *node;
    const char *anchor;
    /* maps: pending key */
    char *key;
    size_t key_len;
    bool have_key;
} Frame;

typedef struct Reader {
    DsymArena *a;
    DsymDiags *diags;
    const char *file;
    Frame *stack;
    size_t depth, cap;
    Anchor *anchors;
    size_t nanchors, capanchors;
    DsymNode *root;
    int docs;
    bool failed;
} Reader;

static void rd_err(Reader *r, const yaml_mark_t *m, const char *fmt,
                   const char *arg) {
    dsym_diag_add(r->diags, DSYM_ERROR, r->file, m ? (uint32_t)m->line + 1 : 0,
                  m ? (uint32_t)m->column + 1 : 0, NULL, fmt, arg ? arg : "");
    r->failed = true;
}

static bool add_anchor(Reader *r, const char *name, DsymNode *n) {
    if (!name) return true;
    if (r->nanchors == r->capanchors) {
        size_t nc = r->capanchors ? r->capanchors * 2 : 8;
        Anchor *p = realloc(r->anchors, nc * sizeof *p);
        if (!p) return false;
        r->anchors = p;
        r->capanchors = nc;
    }
    /* later anchors with the same name override earlier ones (YAML spec) */
    r->anchors[r->nanchors].name = dsym_arena_strdup(r->a, name);
    r->anchors[r->nanchors].node = n;
    r->nanchors++;
    return true;
}

static DsymNode *find_anchor(Reader *r, const char *name) {
    for (size_t i = r->nanchors; i-- > 0;)
        if (strcmp(r->anchors[i].name, name) == 0) return r->anchors[i].node;
    return NULL;
}

static bool map_put_ok(Reader *r, Frame *f, DsymNode *n);

/* Attach a finished node to the current container (or make it the root). */
static bool attach(Reader *r, DsymNode *n, const yaml_mark_t *m) {
    if (r->depth == 0) {
        r->root = n;
        return true;
    }
    Frame *f = &r->stack[r->depth - 1];
    if (f->node->kind == DSYM_NODE_SEQ) return dsym_seq_push(r->a, f->node, n);
    if (!f->have_key) {
        if (n->kind != DSYM_NODE_STR && !n->text) {
            rd_err(r, m, "YAML: mapping keys must be scalars%s", NULL);
            return false;
        }
        const char *ks = n->kind == DSYM_NODE_STR ? n->v.str.ptr : n->text;
        size_t kl = n->kind == DSYM_NODE_STR ? n->v.str.len : strlen(n->text);
        for (size_t i = 0; i < f->node->v.map.count; i++)
            if (f->node->v.map.items[i].key_len == kl &&
                memcmp(f->node->v.map.items[i].key, ks, kl) == 0) {
                char tmp[160];
                snprintf(tmp, sizeof tmp, "%.*s", (int)(kl > 120 ? 120 : kl), ks);
                rd_err(r, m, "YAML: duplicate key '%s'", tmp);
                return false;
            }
        f->key = (char *)ks;
        f->key_len = kl;
        f->have_key = true;
        return true;
    }
    f->have_key = false;
    return map_put_ok(r, f, n);
}

static bool map_put_ok(Reader *r, Frame *f, DsymNode *n) {
    /* key already lives in the arena */
    DsymPair *items = f->node->v.map.items;
    if (f->node->v.map.count == f->node->v.map.cap) {
        size_t nc = f->node->v.map.cap ? f->node->v.map.cap * 2 : 4;
        DsymPair *p = dsym_arena_calloc(r->a, nc, sizeof *p);
        if (!p) return false;
        if (items) memcpy(p, items, f->node->v.map.count * sizeof *p);
        f->node->v.map.items = p;
        f->node->v.map.cap = nc;
    }
    DsymPair *p = &f->node->v.map.items[f->node->v.map.count++];
    p->key = f->key;
    p->key_len = f->key_len;
    p->value = n;
    return true;
}

static DsymNode *make_scalar(Reader *r, const yaml_event_t *ev) {
    const char *val = (const char *)ev->data.scalar.value;
    size_t len = ev->data.scalar.length;
    const char *tag = (const char *)ev->data.scalar.tag;
    bool plain = ev->data.scalar.style == YAML_PLAIN_SCALAR_STYLE;
    bool b = false;
    int64_t i = 0;
    uint64_t u = 0;
    double d = 0;
    Resolved res = R_STR;

    if (tag && strcmp(tag, "!") != 0) {
        if (!strcmp(tag, YAML_STR_TAG)) res = R_STR;
        else if (!strcmp(tag, YAML_NULL_TAG) || !strcmp(tag, YAML_BOOL_TAG) ||
                 !strcmp(tag, YAML_INT_TAG) || !strcmp(tag, YAML_FLOAT_TAG)) {
            res = resolve_plain(val, len, &b, &i, &u, &d);
            const char *want = res == R_NULL ? YAML_NULL_TAG
                               : res == R_BOOL ? YAML_BOOL_TAG
                               : (res == R_INT || res == R_UINT) ? YAML_INT_TAG
                               : res == R_REAL ? YAML_FLOAT_TAG
                                               : "";
            if (strcmp(tag, want) != 0 &&
                !(res == R_INT && !strcmp(tag, YAML_FLOAT_TAG))) {
                rd_err(r, &ev->start_mark, "YAML: value does not match tag %s", tag);
                return NULL;
            }
            if (res == R_INT && !strcmp(tag, YAML_FLOAT_TAG)) res = R_REAL, d = (double)i;
        } else {
            /* unknown local tags: keep the scalar as an untagged value */
            if (plain) res = resolve_plain(val, len, &b, &i, &u, &d);
        }
    } else if (plain && !tag) {
        res = resolve_plain(val, len, &b, &i, &u, &d);
    }

    DsymNode *n;
    switch (res) {
    case R_NULL: n = dsym_node_null(r->a); break;
    case R_BOOL: n = dsym_node_bool(r->a, b); break;
    case R_INT: n = dsym_node_int(r->a, i); break;
    case R_UINT: n = dsym_node_uint(r->a, u); break;
    case R_REAL: n = dsym_node_real(r->a, d); break;
    default: n = dsym_node_strn(r->a, val, len); break;
    }
    if (!n) return NULL;
    if (plain) {
        n->hints |= DSYM_HINT_PLAIN;
        if (res != R_STR) n->text = dsym_arena_strndup(r->a, val, len);
        if ((res == R_INT || res == R_UINT) && len > 2 && val[0] == '0' && val[1] == 'x')
            n->hints |= DSYM_HINT_HEX;
    } else if (ev->data.scalar.style == YAML_LITERAL_SCALAR_STYLE) {
        n->hints |= DSYM_HINT_LITERAL;
    } else {
        n->hints |= DSYM_HINT_QUOTED;
    }
    n->line = (uint32_t)ev->start_mark.line + 1;
    n->col = (uint32_t)ev->start_mark.column + 1;
    return n;
}

static bool push_frame(Reader *r, DsymNode *n, const char *anchor) {
    if (r->depth == r->cap) {
        size_t nc = r->cap ? r->cap * 2 : 16;
        Frame *p = realloc(r->stack, nc * sizeof *p);
        if (!p) return false;
        r->stack = p;
        r->cap = nc;
    }
    Frame *f = &r->stack[r->depth++];
    memset(f, 0, sizeof *f);
    f->node = n;
    f->anchor = anchor ? dsym_arena_strdup(r->a, anchor) : NULL;
    return true;
}

DsymNode *dsym_yaml_read(DsymArena *a, const char *data, size_t len,
                         const char *file, DsymDiags *diags) {
    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
        dsym_diag_add(diags, DSYM_ERROR, file, 0, 0, NULL, "YAML: cannot initialise parser");
        return NULL;
    }
    yaml_parser_set_input_string(&parser, (const unsigned char *)data, len);
    Reader R = {.a = a, .diags = diags, .file = file};
    Reader *r = &R;
    bool done = false;
    while (!done && !r->failed) {
        yaml_event_t ev;
        if (!yaml_parser_parse(&parser, &ev)) {
            yaml_mark_t m = parser.problem_mark;
            char msg[256];
            snprintf(msg, sizeof msg, "%s%s%s", parser.problem ? parser.problem : "syntax error",
                     parser.context ? " " : "", parser.context ? parser.context : "");
            rd_err(r, &m, "YAML: %s", msg);
            break;
        }
        switch (ev.type) {
        case YAML_STREAM_END_EVENT: done = true; break;
        case YAML_DOCUMENT_START_EVENT:
            if (++r->docs > 1)
                rd_err(r, &ev.start_mark, "YAML: only one document per file is supported%s", NULL);
            break;
        case YAML_SCALAR_EVENT: {
            DsymNode *n = make_scalar(r, &ev);
            if (!n || !add_anchor(r, (const char *)ev.data.scalar.anchor, n) ||
                !attach(r, n, &ev.start_mark))
                r->failed = true;
            break;
        }
        case YAML_ALIAS_EVENT: {
            DsymNode *n = find_anchor(r, (const char *)ev.data.alias.anchor);
            if (!n)
                rd_err(r, &ev.start_mark, "YAML: unknown alias '*%s'",
                       (const char *)ev.data.alias.anchor);
            else if (!attach(r, n, &ev.start_mark))
                r->failed = true;
            break;
        }
        case YAML_SEQUENCE_START_EVENT:
        case YAML_MAPPING_START_EVENT: {
            bool seq = ev.type == YAML_SEQUENCE_START_EVENT;
            DsymNode *n = seq ? dsym_node_seq(a) : dsym_node_map(a);
            if (!n) {
                r->failed = true;
                break;
            }
            n->line = (uint32_t)ev.start_mark.line + 1;
            n->col = (uint32_t)ev.start_mark.column + 1;
            int style = seq ? ev.data.sequence_start.style : ev.data.mapping_start.style;
            n->hints |= style == YAML_FLOW_SEQUENCE_STYLE || style == YAML_FLOW_MAPPING_STYLE
                            ? DSYM_HINT_FLOW
                            : DSYM_HINT_BLOCK;
            const char *anchor = (const char *)(seq ? ev.data.sequence_start.anchor
                                                    : ev.data.mapping_start.anchor);
            if (r->depth > 4096) {
                rd_err(r, &ev.start_mark, "YAML: nesting too deep%s", NULL);
                break;
            }
            if (!push_frame(r, n, anchor)) r->failed = true;
            break;
        }
        case YAML_SEQUENCE_END_EVENT:
        case YAML_MAPPING_END_EVENT: {
            Frame f = r->stack[--r->depth];
            if (!add_anchor(r, f.anchor, f.node) || !attach(r, f.node, &ev.start_mark))
                r->failed = true;
            break;
        }
        default: break;
        }
        yaml_event_delete(&ev);
    }
    yaml_parser_delete(&parser);
    free(r->stack);
    free(r->anchors);
    if (r->failed) return NULL;
    if (!r->root) {
        dsym_diag_add(diags, DSYM_ERROR, file, 0, 0, NULL, "YAML: empty document");
        return NULL;
    }
    return r->root;
}

/* ======================================================================== */
/* Writer                                                                   */
/* ======================================================================== */

typedef struct Writer {
    yaml_emitter_t em;
    DsymBuf *out;
    DsymDiags *diags;
    bool failed;
} Writer;

static int yaml_out(void *ctx, unsigned char *buf, size_t size) {
    return dsym_buf_append((DsymBuf *)ctx, buf, size) ? 1 : 0;
}

static bool emit(Writer *w, yaml_event_t *ev) {
    if (w->failed) return false;
    if (!yaml_emitter_emit(&w->em, ev)) {
        dsym_diag_add(w->diags, DSYM_ERROR, NULL, 0, 0, NULL, "YAML: %s",
                      w->em.problem ? w->em.problem : "emitter error");
        w->failed = true;
        return false;
    }
    return true;
}

static bool scalar(Writer *w, const char *s, size_t len, yaml_scalar_style_t st) {
    yaml_event_t ev;
    if (!yaml_scalar_event_initialize(&ev, NULL, NULL, (yaml_char_t *)s, (int)len, 1, 1, st)) {
        w->failed = true;
        return false;
    }
    return emit(w, &ev);
}

static bool needs_quotes(const char *s, size_t len) {
    bool b;
    int64_t i;
    uint64_t u;
    double d;
    if (resolve_plain(s, len, &b, &i, &u, &d) != R_STR) return true;
    /* keep "yes"/"no"/"on"/"off" quoted for YAML 1.1 readers (PyYAML) */
    static const char *y11[] = {"y", "Y", "yes", "Yes", "YES", "n", "N", "no", "No",
                                "NO", "on", "On", "ON", "off", "Off", "OFF", "="};
    for (size_t k = 0; k < sizeof y11 / sizeof *y11; k++)
        if (strlen(y11[k]) == len && !memcmp(y11[k], s, len)) return true;
    /* YAML 1.1 sexagesimal / leading-zero octal look-alikes */
    if (len > 1 && s[0] == '0' && all_digits(s, len, 10)) return true;
    if (len && memchr(s, ':', len) && all_digits(s, 1, 10)) return true;
    return false;
}

static bool emit_string(Writer *w, const char *s, size_t len) {
    if (memchr(s, '\n', len)) return scalar(w, s, len, YAML_LITERAL_SCALAR_STYLE);
    if (needs_quotes(s, len)) return scalar(w, s, len, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    return scalar(w, s, len, YAML_ANY_SCALAR_STYLE);
}

static void fmt_real(double r, char *buf, size_t cap) {
    if (isnan(r)) {
        snprintf(buf, cap, ".nan");
        return;
    }
    if (isinf(r)) {
        snprintf(buf, cap, r < 0 ? "-.inf" : ".inf");
        return;
    }
    for (int prec = 1; prec <= 17; prec++) { /* shortest round-trip form */
        snprintf(buf, cap, "%.*g", prec, r);
        if (strtod(buf, NULL) == r) break;
    }
    if (!strpbrk(buf, ".eEn")) strncat(buf, ".0", cap - strlen(buf) - 1);
}

static bool is_simple_scalar(const DsymNode *n) {
    if (n->kind == DSYM_NODE_SEQ || n->kind == DSYM_NODE_MAP) return false;
    if (n->kind == DSYM_NODE_STR && memchr(n->v.str.ptr, '\n', n->v.str.len)) return false;
    return true;
}

static size_t scalar_width(const DsymNode *n) {
    switch (n->kind) {
    case DSYM_NODE_STR: return n->v.str.len + 2;
    case DSYM_NODE_NULL: return 4;
    case DSYM_NODE_BOOL: return 5;
    case DSYM_NODE_INT: {
        char tmp[32];
        return (size_t)snprintf(tmp, sizeof tmp, "%" PRId64, n->v.i);
    }
    default: return 20;
    }
}

/* Flow style for small, flat containers (as the hand-written specs use). */
static bool want_flow(const DsymNode *n, int depth) {
    size_t cnt = n->kind == DSYM_NODE_SEQ ? n->v.seq.count : n->v.map.count;
    if (cnt == 0) return true;
    if (depth == 0) return false;
    size_t width = 0;
    if (n->kind == DSYM_NODE_SEQ) {
        for (size_t i = 0; i < cnt; i++) {
            if (!is_simple_scalar(n->v.seq.items[i])) return false;
            width += scalar_width(n->v.seq.items[i]) + 2;
        }
        return width <= 120;
    }
    for (size_t i = 0; i < cnt; i++) {
        const DsymPair *p = &n->v.map.items[i];
        width += p->key_len + 2;
        if (is_simple_scalar(p->value)) {
            width += scalar_width(p->value) + 2;
        } else if (p->value->kind == DSYM_NODE_SEQ) {
            for (size_t k = 0; k < p->value->v.seq.count; k++) {
                if (!is_simple_scalar(p->value->v.seq.items[k])) return false;
                width += scalar_width(p->value->v.seq.items[k]) + 2;
            }
        } else if (p->value->kind == DSYM_NODE_MAP && p->value->v.map.count == 0) {
            width += 2;
        } else {
            return false;
        }
    }
    return width <= 220;
}

static bool emit_node(Writer *w, const DsymNode *n, int depth, bool in_flow) {
    char buf[64];
    yaml_event_t ev;
    switch (n->kind) {
    case DSYM_NODE_NULL: return scalar(w, "null", 4, YAML_PLAIN_SCALAR_STYLE);
    case DSYM_NODE_BOOL:
        return n->v.b ? scalar(w, "true", 4, YAML_PLAIN_SCALAR_STYLE)
                      : scalar(w, "false", 5, YAML_PLAIN_SCALAR_STYLE);
    case DSYM_NODE_INT:
        if ((n->hints & DSYM_HINT_HEX) && n->v.i >= 0)
            snprintf(buf, sizeof buf, "0x%02" PRIx64, (uint64_t)n->v.i);
        else
            snprintf(buf, sizeof buf, "%" PRId64, n->v.i);
        return scalar(w, buf, strlen(buf), YAML_PLAIN_SCALAR_STYLE);
    case DSYM_NODE_UINT:
        snprintf(buf, sizeof buf, (n->hints & DSYM_HINT_HEX) ? "0x%" PRIx64 : "%" PRIu64, n->v.u);
        return scalar(w, buf, strlen(buf), YAML_PLAIN_SCALAR_STYLE);
    case DSYM_NODE_REAL:
        fmt_real(n->v.r, buf, sizeof buf);
        return scalar(w, buf, strlen(buf), YAML_PLAIN_SCALAR_STYLE);
    case DSYM_NODE_STR:
        if (in_flow && memchr(n->v.str.ptr, '\n', n->v.str.len))
            return scalar(w, n->v.str.ptr, n->v.str.len, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
        return emit_string(w, n->v.str.ptr, n->v.str.len);
    case DSYM_NODE_SEQ: {
        bool flow = in_flow || want_flow(n, depth);
        if (!yaml_sequence_start_event_initialize(&ev, NULL, NULL, 1,
                                                  flow ? YAML_FLOW_SEQUENCE_STYLE
                                                       : YAML_BLOCK_SEQUENCE_STYLE) ||
            !emit(w, &ev))
            return false;
        for (size_t i = 0; i < n->v.seq.count; i++)
            if (!emit_node(w, n->v.seq.items[i], depth + 1, flow)) return false;
        return yaml_sequence_end_event_initialize(&ev) && emit(w, &ev);
    }
    case DSYM_NODE_MAP: {
        bool flow = in_flow || want_flow(n, depth);
        if (!yaml_mapping_start_event_initialize(&ev, NULL, NULL, 1,
                                                 flow ? YAML_FLOW_MAPPING_STYLE
                                                      : YAML_BLOCK_MAPPING_STYLE) ||
            !emit(w, &ev))
            return false;
        for (size_t i = 0; i < n->v.map.count; i++) {
            const DsymPair *p = &n->v.map.items[i];
            if (!emit_string(w, p->key, p->key_len) ||
                !emit_node(w, p->value, depth + 1, flow))
                return false;
        }
        return yaml_mapping_end_event_initialize(&ev) && emit(w, &ev);
    }
    }
    return false;
}

bool dsym_yaml_write(const DsymNode *root, DsymBuf *out, uint32_t flags,
                     DsymDiags *diags) {
    (void)flags;
    Writer W = {.out = out, .diags = diags};
    Writer *w = &W;
    if (!yaml_emitter_initialize(&w->em)) return false;
    yaml_emitter_set_output(&w->em, yaml_out, out);
    yaml_emitter_set_unicode(&w->em, 1);
    yaml_emitter_set_width(&w->em, -1); /* never fold long lines */
    yaml_emitter_set_indent(&w->em, 2);
    yaml_emitter_set_break(&w->em, YAML_LN_BREAK);
    yaml_event_t ev;
    bool ok = yaml_stream_start_event_initialize(&ev, YAML_UTF8_ENCODING) && emit(w, &ev) &&
              yaml_document_start_event_initialize(&ev, NULL, NULL, NULL, 1) && emit(w, &ev) &&
              emit_node(w, root, 0, false) &&
              yaml_document_end_event_initialize(&ev, 1) && emit(w, &ev) &&
              yaml_stream_end_event_initialize(&ev) && emit(w, &ev);
    ok = ok && yaml_emitter_flush(&w->em);
    yaml_emitter_delete(&w->em);
    return ok && !w->failed && !out->oom;
}
