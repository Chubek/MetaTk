/*
 * ToXML.c — XML codec for the MetaArch document tree, built on libxml2
 * (https://gitlab.gnome.org/GNOME/libxml2): xmlTextWriter for output (correct
 * escaping and encoding by construction) and the DOM parser for input.
 *
 * Vocabulary: W3C XPath and XQuery Functions 3.1, §17.5 "json-to-xml"
 * (namespace http://www.w3.org/2005/xpath-functions). Example:
 *
 *   <map xmlns="http://www.w3.org/2005/xpath-functions">
 *     <number key="schema">1</number>
 *     <array key="targets">
 *       <map><string key="arch">x86</string> ...</map>
 *     </array>
 *   </map>
 *
 * The mapping is lossless and standard: XSLT 3.0's fn:xml-to-json() turns
 * the output into the JSON form directly. Strings containing characters XML
 * 1.0 cannot carry (most C0 controls) use escaped="true" with JSON-style
 * backslash escapes, exactly as the specification prescribes; the same goes
 * for keys via escaped-key="true".
 */
#include "DSymspec.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <libxml/xmlversion.h>

#define FN_NS "http://www.w3.org/2005/xpath-functions"

/* ======================================================================== */
/* Character checks                                                         */
/* ======================================================================== */

/* Validate UTF-8 and XML 1.0 Char production; returns false if escaping is
   required. */
static bool xml_safe(const char *s, size_t len) {
    const unsigned char *p = (const unsigned char *)s, *e = p + len;
    while (p < e) {
        unsigned c = *p;
        if (c < 0x80) {
            if (c < 0x20 && c != '\t' && c != '\n') return false; /* \r is
                normalised away by parsers, so escape it too */
            p++;
            continue;
        }
        int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : -1;
        if (n < 0 || c >= 0xF8 || e - p <= n) return false;
        uint32_t cp = c & (0x3F >> n);
        for (int k = 1; k <= n; k++) {
            if ((p[k] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (p[k] & 0x3F);
        }
        if ((n == 1 && cp < 0x80) || (n == 2 && cp < 0x800) || (n == 3 && cp < 0x10000))
            return false; /* overlong */
        if ((cp >= 0xD800 && cp <= 0xDFFF) || cp == 0xFFFE || cp == 0xFFFF || cp > 0x10FFFF)
            return false;
        p += n + 1;
    }
    return true;
}

/* JSON-style escape for escaped="true" content. Backslash is the only
   special character; invalid bytes become \u00XX of their byte value. */
static char *json_escape(const char *s, size_t len) {
    DsymBuf b;
    dsym_buf_init(&b);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\') dsym_buf_puts(&b, "\\\\");
        else if (c == '\n') dsym_buf_puts(&b, "\\n");
        else if (c == '\r') dsym_buf_puts(&b, "\\r");
        else if (c == '\t') dsym_buf_puts(&b, "\\t");
        else if (c < 0x20 || c == 0x7F) dsym_buf_printf(&b, "\\u%04x", c);
        else if (c >= 0x80) {
            /* copy valid multibyte sequences verbatim */
            size_t n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 0;
            if (n && i + n <= len && xml_safe(s + i, n)) {
                dsym_buf_append(&b, s + i, n);
                i += n - 1;
            } else {
                dsym_buf_printf(&b, "\\u%04x", c); /* lossy only for invalid UTF-8 */
            }
        } else dsym_buf_putc(&b, (char)c);
    }
    if (!b.data) dsym_buf_reserve(&b, 0), b.data[0] = 0;
    return b.data;
}

static size_t utf8_put(char *o, uint32_t cp) {
    if (cp < 0x80) return o[0] = (char)cp, 1;
    if (cp < 0x800) return o[0] = (char)(0xC0 | cp >> 6), o[1] = (char)(0x80 | (cp & 0x3F)), 2;
    if (cp < 0x10000)
        return o[0] = (char)(0xE0 | cp >> 12), o[1] = (char)(0x80 | ((cp >> 6) & 0x3F)),
               o[2] = (char)(0x80 | (cp & 0x3F)), 3;
    o[0] = (char)(0xF0 | cp >> 18);
    o[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    o[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    o[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Inverse of json_escape; returns length or (size_t)-1 on a bad escape. */
static size_t json_unescape(const char *s, size_t len, char *out) {
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] != '\\') {
            out[o++] = s[i];
            continue;
        }
        if (++i >= len) return (size_t)-1;
        switch (s[i]) {
        case '\\': out[o++] = '\\'; break;
        case '"': out[o++] = '"'; break;
        case '/': out[o++] = '/'; break;
        case 'n': out[o++] = '\n'; break;
        case 'r': out[o++] = '\r'; break;
        case 't': out[o++] = '\t'; break;
        case 'b': out[o++] = '\b'; break;
        case 'f': out[o++] = '\f'; break;
        case 'u': {
            uint32_t cp = 0;
            for (int k = 1; k <= 4; k++) {
                int h = i + (size_t)k < len ? hexval(s[i + k]) : -1;
                if (h < 0) return (size_t)-1;
                cp = cp << 4 | (uint32_t)h;
            }
            i += 4;
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < len && s[i + 1] == '\\' &&
                s[i + 2] == 'u') {
                uint32_t lo = 0;
                bool ok = true;
                for (int k = 3; k <= 6; k++) {
                    int h = i + (size_t)k < len ? hexval(s[i + k]) : -1;
                    if (h < 0) ok = false;
                    else lo = lo << 4 | (uint32_t)h;
                }
                if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    i += 6;
                }
            }
            if (cp < 0x100 && cp >= 0x80) out[o++] = (char)cp; /* raw byte (see writer) */
            else o += utf8_put(out + o, cp);
            break;
        }
        default: return (size_t)-1;
        }
    }
    out[o] = 0;
    return o;
}

/* ======================================================================== */
/* Writer                                                                   */
/* ======================================================================== */

typedef struct XW {
    xmlTextWriterPtr w;
    bool failed;
} XW;

#define XCHK(x)                                                                \
    do {                                                                       \
        if ((x) < 0) {                                                         \
            xw->failed = true;                                                 \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool write_key(XW *xw, const DsymPair *p) {
    if (!p) return true;
    if (xml_safe(p->key, p->key_len)) {
        char *k = strndup(p->key, p->key_len);
        if (!k) return false;
        int rc = xmlTextWriterWriteAttribute(xw->w, BAD_CAST "key", BAD_CAST k);
        free(k);
        XCHK(rc);
    } else {
        char *k = json_escape(p->key, p->key_len);
        if (!k) return false;
        int rc = xmlTextWriterWriteAttribute(xw->w, BAD_CAST "escaped-key", BAD_CAST "true");
        if (rc >= 0) rc = xmlTextWriterWriteAttribute(xw->w, BAD_CAST "key", BAD_CAST k);
        free(k);
        XCHK(rc);
    }
    return true;
}

static void fmt_number(const DsymNode *n, char *buf, size_t cap) {
    if (n->kind == DSYM_NODE_INT) {
        snprintf(buf, cap, "%" PRId64, n->v.i);
    } else if (n->kind == DSYM_NODE_UINT) {
        snprintf(buf, cap, "%" PRIu64, n->v.u);
    } else if (isnan(n->v.r)) {
        snprintf(buf, cap, "NaN"); /* xs:double lexical space */
    } else if (isinf(n->v.r)) {
        snprintf(buf, cap, n->v.r < 0 ? "-INF" : "INF");
    } else {
        for (int prec = 1; prec <= 17; prec++) {
            snprintf(buf, cap, "%.*g", prec, n->v.r);
            if (strtod(buf, NULL) == n->v.r) break;
        }
        if (!strpbrk(buf, ".eE")) strncat(buf, ".0", cap - strlen(buf) - 1);
    }
}

static bool write_node(XW *xw, const DsymNode *n, const DsymPair *key, bool root) {
    static const char *const el[] = {"null", "boolean", "number", "number",
                                     "number", "string", "array", "map"};
    XCHK(xmlTextWriterStartElement(xw->w, BAD_CAST el[n->kind]));
    if (root) XCHK(xmlTextWriterWriteAttribute(xw->w, BAD_CAST "xmlns", BAD_CAST FN_NS));
    if (!write_key(xw, key)) return false;
    char buf[64];
    switch (n->kind) {
    case DSYM_NODE_NULL: break;
    case DSYM_NODE_BOOL:
        XCHK(xmlTextWriterWriteString(xw->w, BAD_CAST(n->v.b ? "true" : "false")));
        break;
    case DSYM_NODE_INT:
    case DSYM_NODE_UINT:
    case DSYM_NODE_REAL:
        fmt_number(n, buf, sizeof buf);
        XCHK(xmlTextWriterWriteString(xw->w, BAD_CAST buf));
        break;
    case DSYM_NODE_STR: {
        bool safe = xml_safe(n->v.str.ptr, n->v.str.len) &&
                    strlen(n->v.str.ptr) == n->v.str.len;
        char *s = safe ? strndup(n->v.str.ptr, n->v.str.len)
                       : json_escape(n->v.str.ptr, n->v.str.len);
        if (!s) return false;
        int rc = 0;
        if (!safe) rc = xmlTextWriterWriteAttribute(xw->w, BAD_CAST "escaped", BAD_CAST "true");
        if (rc >= 0 && *s) rc = xmlTextWriterWriteString(xw->w, BAD_CAST s);
        free(s);
        XCHK(rc);
        break;
    }
    case DSYM_NODE_SEQ:
        for (size_t i = 0; i < n->v.seq.count; i++)
            if (!write_node(xw, n->v.seq.items[i], NULL, false)) return false;
        break;
    case DSYM_NODE_MAP:
        for (size_t i = 0; i < n->v.map.count; i++)
            if (!write_node(xw, n->v.map.items[i].value, &n->v.map.items[i], false))
                return false;
        break;
    }
    /* Full end tag for empty strings keeps <string key="x"></string> explicit;
       other empty elements self-close. */
    XCHK(n->kind == DSYM_NODE_STR ? xmlTextWriterFullEndElement(xw->w)
                                  : xmlTextWriterEndElement(xw->w));
    return true;
}

bool dsym_xml_write(const DsymNode *root, DsymBuf *out, uint32_t flags,
                    DsymDiags *diags) {
    xmlBufferPtr xb = xmlBufferCreate();
    if (!xb) return false;
    xmlTextWriterPtr w = xmlNewTextWriterMemory(xb, 0);
    if (!w) {
        xmlBufferFree(xb);
        return false;
    }
    XW X = {w, false};
    XW *xw = &X;
    bool ok = true;
    if (!(flags & DSYM_WRITE_COMPACT)) {
        xmlTextWriterSetIndent(w, 1);
        xmlTextWriterSetIndentString(w, BAD_CAST "  ");
    }
    if (xmlTextWriterStartDocument(w, NULL, "UTF-8", NULL) < 0) ok = false;
    if (ok && !(flags & DSYM_WRITE_NO_HEADER))
        ok = xmlTextWriterWriteComment(
                 w, BAD_CAST " Generated by MetaArch; vocabulary: W3C XPath 3.1 json-to-xml ") >= 0;
    if (ok) ok = write_node(xw, root, NULL, true);
    if (ok) ok = xmlTextWriterEndDocument(w) >= 0;
    xmlFreeTextWriter(w); /* flushes into xb */
    if (ok) ok = dsym_buf_append(out, xmlBufferContent(xb), (size_t)xmlBufferLength(xb));
    else dsym_diag_add(diags, DSYM_ERROR, NULL, 0, 0, NULL, "XML: writer error");
    xmlBufferFree(xb);
    return ok;
}

/* ======================================================================== */
/* Reader                                                                   */
/* ======================================================================== */

typedef struct XR {
    DsymArena *a;
    DsymDiags *diags;
    const char *file;
    bool failed;
} XR;

static void xr_err(XR *r, xmlNodePtr n, const char *fmt, const char *arg) {
    dsym_diag_add(r->diags, DSYM_ERROR, r->file, n ? (uint32_t)xmlGetLineNo(n) : 0, 0, NULL,
                  fmt, arg ? arg : "");
    r->failed = true;
}

static bool is_fn(xmlNodePtr n) {
    return n->type == XML_ELEMENT_NODE &&
           (!n->ns || !n->ns->href || strcmp((const char *)n->ns->href, FN_NS) == 0);
}

static char *node_text(xmlNodePtr n) {
    xmlChar *t = xmlNodeGetContent(n);
    if (!t) return strdup("");
    char *s = strdup((const char *)t);
    xmlFree(t);
    return s;
}

static bool attr_true(xmlNodePtr n, const char *name) {
    xmlChar *v = xmlGetProp(n, BAD_CAST name);
    bool t = v && (strcmp((const char *)v, "true") == 0 || strcmp((const char *)v, "1") == 0);
    if (v) xmlFree(v);
    return t;
}

static bool decode_maybe_escaped(XR *r, xmlNodePtr n, const char *s, bool esc,
                                 const char **out, size_t *len) {
    size_t L = strlen(s);
    if (!esc) {
        *out = dsym_arena_strndup(r->a, s, L);
        *len = L;
        return *out != NULL;
    }
    char *buf = dsym_arena_alloc(r->a, L * 2 + 1);
    if (!buf) return false;
    size_t o = json_unescape(s, L, buf);
    if (o == (size_t)-1) {
        xr_err(r, n, "XML: invalid escape sequence in '%s'", s);
        return false;
    }
    *out = buf;
    *len = o;
    return true;
}

static DsymNode *read_el(XR *r, xmlNodePtr n, int depth) {
    if (depth > 4096) {
        xr_err(r, n, "XML: nesting too deep%s", NULL);
        return NULL;
    }
    const char *nm = (const char *)n->name;
    DsymNode *out = NULL;
    if (!strcmp(nm, "map") || !strcmp(nm, "array")) {
        bool map = nm[0] == 'm';
        out = map ? dsym_node_map(r->a) : dsym_node_seq(r->a);
        if (!out) return NULL;
        for (xmlNodePtr c = n->children; c; c = c->next) {
            if (c->type == XML_COMMENT_NODE || c->type == XML_PI_NODE) continue;
            if (c->type == XML_TEXT_NODE || c->type == XML_CDATA_SECTION_NODE) {
                const xmlChar *t = c->content;
                for (; t && *t; t++)
                    if (*t != ' ' && *t != '\t' && *t != '\n' && *t != '\r') {
                        xr_err(r, c, "XML: unexpected text inside <%s>", nm);
                        return NULL;
                    }
                continue;
            }
            if (!is_fn(c)) {
                xr_err(r, c, "XML: element <%s> is not in the xpath-functions namespace",
                       (const char *)c->name);
                return NULL;
            }
            DsymNode *v = read_el(r, c, depth + 1);
            if (!v) return NULL;
            if (map) {
                xmlChar *k = xmlGetProp(c, BAD_CAST "key");
                if (!k) {
                    xr_err(r, c, "XML: map member <%s> lacks a key attribute",
                           (const char *)c->name);
                    return NULL;
                }
                const char *ks;
                size_t kl;
                bool ok = decode_maybe_escaped(r, c, (const char *)k, attr_true(c, "escaped-key"),
                                               &ks, &kl);
                xmlFree(k);
                if (!ok) return NULL;
                for (size_t i = 0; i < out->v.map.count; i++)
                    if (out->v.map.items[i].key_len == kl &&
                        !memcmp(out->v.map.items[i].key, ks, kl)) {
                        xr_err(r, c, "XML: duplicate key '%s'", ks);
                        return NULL;
                    }
                if (!dsym_map_put_ref(r->a, out, ks, v)) return NULL;
                out->v.map.items[out->v.map.count - 1].key_len = kl;
            } else {
                if (!dsym_seq_push(r->a, out, v)) return NULL;
            }
        }
    } else if (!strcmp(nm, "string")) {
        char *t = node_text(n);
        if (!t) return NULL;
        const char *s;
        size_t len;
        bool ok = decode_maybe_escaped(r, n, t, attr_true(n, "escaped"), &s, &len);
        free(t);
        if (!ok) return NULL;
        out = dsym_node_new(r->a, DSYM_NODE_STR);
        if (!out) return NULL;
        out->v.str.ptr = s;
        out->v.str.len = len;
        out->hints |= DSYM_HINT_QUOTED;
    } else if (!strcmp(nm, "number")) {
        char *t = node_text(n);
        if (!t) return NULL;
        char *s = t;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        char *e = s + strlen(s);
        while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
        char *end = NULL;
        bool intlike = *s && strspn(s + (*s == '-'), "0123456789") == strlen(s + (*s == '-')) &&
                       s[*s == '-'] != 0;
        if (intlike) {
            errno = 0;
            if (*s == '-') {
                long long v = strtoll(s, &end, 10);
                if (!errno) out = dsym_node_int(r->a, v);
            } else {
                unsigned long long v = strtoull(s, &end, 10);
                if (!errno) out = dsym_node_uint(r->a, v);
            }
        }
        if (!out) {
            double d;
            if (!strcmp(s, "INF") || !strcmp(s, "+INF")) d = INFINITY;
            else if (!strcmp(s, "-INF")) d = -INFINITY;
            else if (!strcmp(s, "NaN")) d = NAN;
            else {
                d = strtod(s, &end);
                if (!*s || (end && *end)) {
                    xr_err(r, n, "XML: '%s' is not a number", s);
                    free(t);
                    return NULL;
                }
            }
            out = dsym_node_real(r->a, d);
        }
        free(t);
    } else if (!strcmp(nm, "boolean")) {
        char *t = node_text(n);
        if (!t) return NULL;
        char *s = t;
        while (*s == ' ' || *s == '\n' || *s == '\t' || *s == '\r') s++;
        size_t L = strcspn(s, " \t\r\n");
        s[L] = 0;
        if (!strcmp(s, "true") || !strcmp(s, "1")) out = dsym_node_bool(r->a, true);
        else if (!strcmp(s, "false") || !strcmp(s, "0")) out = dsym_node_bool(r->a, false);
        else xr_err(r, n, "XML: '%s' is not a boolean", s);
        free(t);
        if (!out) return NULL;
    } else if (!strcmp(nm, "null")) {
        out = dsym_node_null(r->a);
    } else {
        xr_err(r, n, "XML: unknown element <%s>", nm);
        return NULL;
    }
    if (out) out->line = (uint32_t)xmlGetLineNo(n);
    return out;
}

static void xml_silent(void *ctx, const xmlError *err) {
    XR *r = ctx;
    if (!err || err->level < XML_ERR_ERROR) return;
    char msg[512];
    snprintf(msg, sizeof msg, "%s", err->message ? err->message : "parse error");
    size_t L = strlen(msg);
    while (L && (msg[L - 1] == '\n' || msg[L - 1] == ' ')) msg[--L] = 0;
    dsym_diag_add(r->diags, DSYM_ERROR, r->file, (uint32_t)(err->line > 0 ? err->line : 0),
                  (uint32_t)(err->int2 > 0 ? err->int2 : 0), NULL, "XML: %s", msg);
    r->failed = true;
}

DsymNode *dsym_xml_read(DsymArena *a, const char *data, size_t len,
                        const char *file, DsymDiags *diags) {
    XR R = {a, diags, file, false};
    if (len > INT32_MAX) {
        dsym_diag_add(diags, DSYM_ERROR, file, 0, 0, NULL, "XML: document too large");
        return NULL;
    }
    int opts = XML_PARSE_NONET | XML_PARSE_NOCDATA | XML_PARSE_HUGE;
#if LIBXML_VERSION >= 21300
    opts |= XML_PARSE_NO_XXE;
#endif
    xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
    if (!ctxt) return NULL;
#if LIBXML_VERSION >= 21300
    xmlCtxtSetErrorHandler(ctxt, xml_silent, &R);
#else
    opts |= XML_PARSE_NOERROR | XML_PARSE_NOWARNING;
#endif
    xmlDocPtr doc = xmlCtxtReadMemory(ctxt, data, (int)len, file, NULL, opts);
    DsymNode *root = NULL;
    if (!doc) {
        if (!R.failed)
            dsym_diag_add(diags, DSYM_ERROR, file, 0, 0, NULL, "XML: document is not well-formed");
    } else {
        xmlNodePtr top = xmlDocGetRootElement(doc);
        if (!top) xr_err(&R, NULL, "XML: no root element%s", NULL);
        else if (!is_fn(top))
            xr_err(&R, top, "XML: root element is not in the " FN_NS " namespace%s", NULL);
        else root = read_el(&R, top, 0);
        xmlFreeDoc(doc);
    }
    xmlFreeParserCtxt(ctxt);
    return R.failed ? NULL : root;
}
