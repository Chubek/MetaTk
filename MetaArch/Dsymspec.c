/*
 * Dsymspec.c — core of the MetaArch architecture database.
 *
 *   - arena / buffer / diagnostics infrastructure
 *   - document tree helpers and format dispatch
 *   - table-driven schema binder: one field-descriptor table per record type
 *     drives both tree -> struct (with full schema validation) and
 *     struct -> tree, so reader and writer cannot drift apart
 *   - cross-reference resolution and semantic checks
 *   - encoder pseudo-code compiler (Pratt parser -> folded RPN) and evaluator
 */
#include "DSymspec.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define ARRLEN(a) (sizeof(a) / sizeof((a)[0]))

/* ======================================================================== */
/* Arena                                                                    */
/* ======================================================================== */

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t used, cap;
    max_align_t data[]; /* flexible, suitably aligned */
} ArenaBlock;

struct DsymArena {
    ArenaBlock *head;
};

#define ARENA_CHUNK (64u * 1024u)
#define ARENA_ALIGN (sizeof(max_align_t))

DsymArena *dsym_arena_new(void) { return calloc(1, sizeof(DsymArena)); }

void dsym_arena_free(DsymArena *a) {
    if (!a) return;
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *n = b->next;
        free(b);
        b = n;
    }
    free(a);
}

void *dsym_arena_alloc(DsymArena *a, size_t size) {
    if (!a) return NULL;
    if (size == 0) size = 1;
    size_t need = (size + ARENA_ALIGN - 1) & ~(ARENA_ALIGN - 1);
    if (need < size) return NULL;
    ArenaBlock *b = a->head;
    if (!b || b->cap - b->used < need) {
        if (need > ARENA_CHUNK / 4) {
            /* Large allocation: dedicated block linked behind the head so the
               head's free space keeps being used. */
            ArenaBlock *nb = malloc(sizeof(ArenaBlock) + need);
            if (!nb) return NULL;
            nb->used = need;
            nb->cap = need;
            if (b) {
                nb->next = b->next;
                b->next = nb;
            } else {
                nb->next = NULL;
                a->head = nb;
            }
            memset(nb->data, 0, need);
            return nb->data;
        }
        ArenaBlock *nb = malloc(sizeof(ArenaBlock) + ARENA_CHUNK);
        if (!nb) return NULL;
        nb->used = 0;
        nb->cap = ARENA_CHUNK;
        nb->next = b;
        a->head = nb;
        b = nb;
    }
    void *p = (char *)b->data + b->used;
    b->used += need;
    memset(p, 0, need);
    return p;
}

void *dsym_arena_calloc(DsymArena *a, size_t n, size_t size) {
    if (size && n > SIZE_MAX / size) return NULL;
    return dsym_arena_alloc(a, n * size);
}

char *dsym_arena_strndup(DsymArena *a, const char *s, size_t len) {
    char *p = dsym_arena_alloc(a, len + 1);
    if (!p) return NULL;
    if (len) memcpy(p, s, len);
    p[len] = 0;
    return p;
}

char *dsym_arena_strdup(DsymArena *a, const char *s) {
    return s ? dsym_arena_strndup(a, s, strlen(s)) : NULL;
}

static char *arena_vprintf(DsymArena *a, const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n < 0) return NULL;
    char *p = dsym_arena_alloc(a, (size_t)n + 1);
    if (p) vsnprintf(p, (size_t)n + 1, fmt, ap);
    return p;
}

char *dsym_arena_printf(DsymArena *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *p = arena_vprintf(a, fmt, ap);
    va_end(ap);
    return p;
}

/* ======================================================================== */
/* Buffer                                                                   */
/* ======================================================================== */

void dsym_buf_init(DsymBuf *b) { memset(b, 0, sizeof *b); }

void dsym_buf_free(DsymBuf *b) {
    free(b->data);
    memset(b, 0, sizeof *b);
}

bool dsym_buf_reserve(DsymBuf *b, size_t extra) {
    if (b->oom) return false;
    if (b->len + extra + 1 <= b->cap) return true;
    size_t nc = b->cap ? b->cap : 256;
    while (nc < b->len + extra + 1) {
        if (nc > SIZE_MAX / 2) {
            b->oom = true;
            return false;
        }
        nc *= 2;
    }
    char *p = realloc(b->data, nc);
    if (!p) {
        b->oom = true;
        return false;
    }
    b->data = p;
    b->cap = nc;
    return true;
}

bool dsym_buf_append(DsymBuf *b, const void *data, size_t len) {
    if (!dsym_buf_reserve(b, len)) return false;
    if (len) memcpy(b->data + b->len, data, len);
    b->len += len;
    b->data[b->len] = 0;
    return true;
}

bool dsym_buf_puts(DsymBuf *b, const char *s) {
    return dsym_buf_append(b, s, strlen(s));
}

bool dsym_buf_putc(DsymBuf *b, char c) { return dsym_buf_append(b, &c, 1); }

bool dsym_buf_printf(DsymBuf *b, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    bool ok = n >= 0 && dsym_buf_reserve(b, (size_t)n);
    if (ok) {
        vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap);
        b->len += (size_t)n;
    }
    va_end(ap);
    return ok;
}

bool dsym_buf_read_file(DsymBuf *b, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char tmp[65536];
    size_t n;
    bool ok = true;
    while ((n = fread(tmp, 1, sizeof tmp, f)) > 0)
        if (!dsym_buf_append(b, tmp, n)) {
            ok = false;
            errno = ENOMEM;
            break;
        }
    if (ferror(f)) ok = false;
    fclose(f);
    if (ok && !b->data) ok = dsym_buf_reserve(b, 0), b->data[0] = 0;
    return ok;
}

bool dsym_buf_write_file(const DsymBuf *b, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(b->data ? b->data : "", 1, b->len, f) == b->len;
    if (fclose(f) != 0) ok = false;
    return ok;
}

/* ======================================================================== */
/* Diagnostics                                                              */
/* ======================================================================== */

void dsym_diags_init(DsymDiags *d) {
    memset(d, 0, sizeof *d);
    d->max_errors = 200;
}

void dsym_diags_free(DsymDiags *d) {
    if (!d) return;
    dsym_arena_free(d->arena);
    free(d->items);
    memset(d, 0, sizeof *d);
}

void dsym_diag_add(DsymDiags *d, DsymSeverity sev, const char *file,
                   uint32_t line, uint32_t col, const char *path,
                   const char *fmt, ...) {
    if (!d) return;
    if (sev == DSYM_ERROR) d->errors++;
    if (sev == DSYM_WARNING) d->warnings++;
    if (d->max_errors && d->count >= d->max_errors) return;
    if (!d->arena && !(d->arena = dsym_arena_new())) return;
    if (d->count == d->cap) {
        size_t nc = d->cap ? d->cap * 2 : 16;
        DsymDiag *p = realloc(d->items, nc * sizeof *p);
        if (!p) return;
        d->items = p;
        d->cap = nc;
    }
    va_list ap;
    va_start(ap, fmt);
    char *msg = arena_vprintf(d->arena, fmt, ap);
    va_end(ap);
    DsymDiag *g = &d->items[d->count++];
    g->severity = sev;
    g->file = file ? dsym_arena_strdup(d->arena, file) : NULL;
    g->line = line;
    g->col = col;
    g->path = path && *path ? dsym_arena_strdup(d->arena, path) : NULL;
    g->msg = msg ? msg : "(out of memory)";
}

void dsym_diags_print(const DsymDiags *d, FILE *out) {
    static const char *sev[] = {"note", "warning", "error"};
    for (size_t i = 0; i < d->count; i++) {
        const DsymDiag *g = &d->items[i];
        if (g->file) fprintf(out, "%s:", g->file);
        if (g->line) fprintf(out, "%" PRIu32 ":", g->line);
        if (g->line && g->col) fprintf(out, "%" PRIu32 ":", g->col);
        if (g->file || g->line) fputc(' ', out);
        fprintf(out, "%s: %s", sev[g->severity], g->msg);
        if (g->path) fprintf(out, " [at %s]", g->path);
        fputc('\n', out);
    }
    size_t shown = 0;
    for (size_t i = 0; i < d->count; i++)
        if (d->items[i].severity == DSYM_ERROR) shown++;
    if (d->errors > shown)
        fprintf(out, "... and %zu more error(s)\n", d->errors - shown);
}

/* ======================================================================== */
/* Document tree                                                            */
/* ======================================================================== */

DsymNode *dsym_node_new(DsymArena *a, DsymNodeKind kind) {
    DsymNode *n = dsym_arena_alloc(a, sizeof *n);
    if (n) n->kind = kind;
    return n;
}

DsymNode *dsym_node_null(DsymArena *a) {
    return dsym_node_new(a, DSYM_NODE_NULL);
}

DsymNode *dsym_node_bool(DsymArena *a, bool b) {
    DsymNode *n = dsym_node_new(a, DSYM_NODE_BOOL);
    if (n) n->v.b = b;
    return n;
}

DsymNode *dsym_node_int(DsymArena *a, int64_t i) {
    DsymNode *n = dsym_node_new(a, DSYM_NODE_INT);
    if (n) n->v.i = i;
    return n;
}

DsymNode *dsym_node_uint(DsymArena *a, uint64_t u) {
    if (u <= (uint64_t)INT64_MAX) return dsym_node_int(a, (int64_t)u);
    DsymNode *n = dsym_node_new(a, DSYM_NODE_UINT);
    if (n) n->v.u = u;
    return n;
}

DsymNode *dsym_node_real(DsymArena *a, double r) {
    DsymNode *n = dsym_node_new(a, DSYM_NODE_REAL);
    if (n) n->v.r = r;
    return n;
}

DsymNode *dsym_node_strn(DsymArena *a, const char *s, size_t len) {
    DsymNode *n = dsym_node_new(a, DSYM_NODE_STR);
    if (!n) return NULL;
    char *p = dsym_arena_strndup(a, s, len);
    if (!p) return NULL;
    n->v.str.ptr = p;
    n->v.str.len = len;
    return n;
}

DsymNode *dsym_node_str(DsymArena *a, const char *s) {
    return dsym_node_strn(a, s, strlen(s));
}

DsymNode *dsym_node_str_ref(DsymArena *a, const char *s) {
    DsymNode *n = dsym_node_new(a, DSYM_NODE_STR);
    if (!n) return NULL;
    n->v.str.ptr = s;
    n->v.str.len = strlen(s);
    return n;
}

DsymNode *dsym_node_seq(DsymArena *a) { return dsym_node_new(a, DSYM_NODE_SEQ); }
DsymNode *dsym_node_map(DsymArena *a) { return dsym_node_new(a, DSYM_NODE_MAP); }

/* Arena-backed growable arrays: old storage is simply abandoned. */
static void *grow(DsymArena *a, void *old, size_t count, size_t *cap,
                  size_t elem) {
    if (count < *cap) return old;
    size_t nc = *cap ? *cap * 2 : 4;
    void *p = dsym_arena_calloc(a, nc, elem);
    if (!p) return NULL;
    if (old && count) memcpy(p, old, count * elem);
    *cap = nc;
    return p;
}

bool dsym_seq_push(DsymArena *a, DsymNode *seq, DsymNode *item) {
    if (!seq || !item || seq->kind != DSYM_NODE_SEQ) return false;
    DsymNode **p = grow(a, seq->v.seq.items, seq->v.seq.count, &seq->v.seq.cap,
                        sizeof *p);
    if (!p) return false;
    seq->v.seq.items = p;
    p[seq->v.seq.count++] = item;
    return true;
}

static bool map_put_impl(DsymArena *a, DsymNode *map, const char *key,
                         size_t key_len, DsymNode *value) {
    if (!map || !value || !key || map->kind != DSYM_NODE_MAP) return false;
    DsymPair *p = grow(a, map->v.map.items, map->v.map.count, &map->v.map.cap,
                       sizeof *p);
    if (!p) return false;
    map->v.map.items = p;
    p[map->v.map.count].key = key;
    p[map->v.map.count].key_len = key_len;
    p[map->v.map.count].value = value;
    map->v.map.count++;
    return true;
}

bool dsym_map_put(DsymArena *a, DsymNode *map, const char *key, size_t key_len,
                  DsymNode *value) {
    char *k = dsym_arena_strndup(a, key, key_len);
    return k && map_put_impl(a, map, k, key_len, value);
}

bool dsym_map_put_ref(DsymArena *a, DsymNode *map, const char *key,
                      DsymNode *value) {
    return map_put_impl(a, map, key, strlen(key), value);
}

DsymNode *dsym_map_get(const DsymNode *map, const char *key) {
    if (!map || map->kind != DSYM_NODE_MAP) return NULL;
    size_t kl = strlen(key);
    for (size_t i = 0; i < map->v.map.count; i++)
        if (map->v.map.items[i].key_len == kl &&
            memcmp(map->v.map.items[i].key, key, kl) == 0)
            return map->v.map.items[i].value;
    return NULL;
}

const char *dsym_node_kind_name(DsymNodeKind k) {
    static const char *names[] = {"null",   "boolean", "integer",  "integer",
                                  "number", "string",  "sequence", "mapping"};
    return (unsigned)k < ARRLEN(names) ? names[k] : "?";
}

bool dsym_node_equal(const DsymNode *a, const DsymNode *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    /* integral reals compare equal to ints (JSON has a single number type) */
    if (a->kind != b->kind) {
        if (a->kind == DSYM_NODE_REAL && b->kind == DSYM_NODE_INT)
            return a->v.r == (double)b->v.i;
        if (b->kind == DSYM_NODE_REAL && a->kind == DSYM_NODE_INT)
            return b->v.r == (double)a->v.i;
        return false;
    }
    switch (a->kind) {
    case DSYM_NODE_NULL: return true;
    case DSYM_NODE_BOOL: return a->v.b == b->v.b;
    case DSYM_NODE_INT: return a->v.i == b->v.i;
    case DSYM_NODE_UINT: return a->v.u == b->v.u;
    case DSYM_NODE_REAL:
        return a->v.r == b->v.r || (a->v.r != a->v.r && b->v.r != b->v.r);
    case DSYM_NODE_STR:
        return a->v.str.len == b->v.str.len &&
               memcmp(a->v.str.ptr, b->v.str.ptr, a->v.str.len) == 0;
    case DSYM_NODE_SEQ:
        if (a->v.seq.count != b->v.seq.count) return false;
        for (size_t i = 0; i < a->v.seq.count; i++)
            if (!dsym_node_equal(a->v.seq.items[i], b->v.seq.items[i]))
                return false;
        return true;
    case DSYM_NODE_MAP:
        if (a->v.map.count != b->v.map.count) return false;
        for (size_t i = 0; i < a->v.map.count; i++) {
            const DsymPair *pa = &a->v.map.items[i], *pb = &b->v.map.items[i];
            if (pa->key_len != pb->key_len ||
                memcmp(pa->key, pb->key, pa->key_len) != 0 ||
                !dsym_node_equal(pa->value, pb->value))
                return false;
        }
        return true;
    }
    return false;
}

DsymNode *dsym_node_clone(DsymArena *a, const DsymNode *n) {
    if (!n) return NULL;
    DsymNode *c = dsym_node_new(a, n->kind);
    if (!c) return NULL;
    c->hints = n->hints;
    c->line = n->line;
    c->col = n->col;
    c->text = n->text ? dsym_arena_strdup(a, n->text) : NULL;
    switch (n->kind) {
    case DSYM_NODE_STR:
        c->v.str.ptr = dsym_arena_strndup(a, n->v.str.ptr, n->v.str.len);
        c->v.str.len = n->v.str.len;
        if (!c->v.str.ptr) return NULL;
        break;
    case DSYM_NODE_SEQ:
        c->v.seq.items = dsym_arena_calloc(a, n->v.seq.count + 1, sizeof(DsymNode *));
        if (!c->v.seq.items) return NULL;
        c->v.seq.cap = n->v.seq.count + 1;
        for (size_t i = 0; i < n->v.seq.count; i++)
            if (!(c->v.seq.items[c->v.seq.count++] =
                      dsym_node_clone(a, n->v.seq.items[i])))
                return NULL;
        break;
    case DSYM_NODE_MAP:
        c->v.map.items = dsym_arena_calloc(a, n->v.map.count + 1, sizeof(DsymPair));
        if (!c->v.map.items) return NULL;
        c->v.map.cap = n->v.map.count + 1;
        for (size_t i = 0; i < n->v.map.count; i++) {
            DsymPair *d = &c->v.map.items[i];
            const DsymPair *s = &n->v.map.items[i];
            d->key = dsym_arena_strndup(a, s->key, s->key_len);
            d->key_len = s->key_len;
            d->value = dsym_node_clone(a, s->value);
            if (!d->key || !d->value) return NULL;
            c->v.map.count++;
        }
        break;
    default: c->v = n->v; break;
    }
    return c;
}

/* ======================================================================== */
/* Format dispatch                                                          */
/* ======================================================================== */

DsymFormat dsym_format_from_name(const char *name) {
    if (!name) return DSYM_FORMAT_AUTO;
    if (!strcasecmp(name, "yaml") || !strcasecmp(name, "yml"))
        return DSYM_FORMAT_YAML;
    if (!strcasecmp(name, "json")) return DSYM_FORMAT_JSON;
    if (!strcasecmp(name, "xml")) return DSYM_FORMAT_XML;
    if (!strcasecmp(name, "sexpr") || !strcasecmp(name, "sexp") ||
        !strcasecmp(name, "lisp") || !strcasecmp(name, "s-expr"))
        return DSYM_FORMAT_SEXPR;
    return DSYM_FORMAT_AUTO;
}

DsymFormat dsym_format_from_path(const char *path) {
    if (!path) return DSYM_FORMAT_AUTO;
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) return DSYM_FORMAT_AUTO;
    return dsym_format_from_name(dot + 1);
}

const char *dsym_format_name(DsymFormat f) {
    switch (f) {
    case DSYM_FORMAT_YAML: return "yaml";
    case DSYM_FORMAT_JSON: return "json";
    case DSYM_FORMAT_XML: return "xml";
    case DSYM_FORMAT_SEXPR: return "sexpr";
    default: return "auto";
    }
}

const char *dsym_format_extension(DsymFormat f) {
    switch (f) {
    case DSYM_FORMAT_YAML: return ".yaml";
    case DSYM_FORMAT_JSON: return ".json";
    case DSYM_FORMAT_XML: return ".xml";
    case DSYM_FORMAT_SEXPR: return ".sexp";
    default: return "";
    }
}

/* Sniff the content when the extension does not tell. */
static DsymFormat sniff_format(const char *data, size_t len) {
    size_t i = 0;
    if (len >= 3 && (unsigned char)data[0] == 0xEF &&
        (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF)
        i = 3;
    for (; i < len; i++) {
        char c = data[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        if (c == ';') { /* sexpr comment */
            return DSYM_FORMAT_SEXPR;
        }
        if (c == '{' || c == '[') return DSYM_FORMAT_JSON;
        if (c == '<') return DSYM_FORMAT_XML;
        if (c == '(') return DSYM_FORMAT_SEXPR;
        break;
    }
    return DSYM_FORMAT_YAML; /* YAML is a superset of JSON anyway */
}

DsymNode *dsym_tree_read(DsymFormat f, DsymArena *a, const char *data,
                         size_t len, const char *file, DsymDiags *diags) {
    if (f == DSYM_FORMAT_AUTO) f = dsym_format_from_path(file);
    if (f == DSYM_FORMAT_AUTO) f = sniff_format(data, len);
    switch (f) {
    case DSYM_FORMAT_YAML: return dsym_yaml_read(a, data, len, file, diags);
    case DSYM_FORMAT_JSON: return dsym_json_read(a, data, len, file, diags);
    case DSYM_FORMAT_XML: return dsym_xml_read(a, data, len, file, diags);
    case DSYM_FORMAT_SEXPR: return dsym_sexpr_read(a, data, len, file, diags);
    default: return NULL;
    }
}

bool dsym_tree_write(DsymFormat f, const DsymNode *root, DsymBuf *out,
                     uint32_t flags, DsymDiags *diags) {
    switch (f) {
    case DSYM_FORMAT_YAML: return dsym_yaml_write(root, out, flags, diags);
    case DSYM_FORMAT_JSON: return dsym_json_write(root, out, flags, diags);
    case DSYM_FORMAT_XML: return dsym_xml_write(root, out, flags, diags);
    case DSYM_FORMAT_SEXPR: return dsym_sexpr_write(root, out, flags, diags);
    default:
        dsym_diag_add(diags, DSYM_ERROR, NULL, 0, 0, NULL,
                      "no output format given");
        return false;
    }
}

/* ======================================================================== */
/* Enum tables                                                              */
/* ======================================================================== */

static const char *const BYTE_ORDER_N[] = {"little", "le", "big", "be"};
static const char *const REG_CLASS_N[] = {"gpr",  "seg", "ctrl",
                                          "debug", "vec", "mask",
                                          "fp",   "status", "special"};
static const int64_t REG_WIDTH_V[] = {8, 16, 32, 64, 80, 128, 256, 512};
static const char *const REG_FLAG_N[] = {"readable", "writable", "imp_used",
                                         "imp_mod", "modedep"};
static const char *const FORM_FLAG_N[] = {
    "privileged", "branch",     "cond_branch", "return",
    "terminator", "may_trap",   "reads_mem",   "writes_mem",
    "delayed",    "implicit_ops", "reloc",     "aliases"};
static const char *const OPERAND_KIND_N[] = {"reg", "imm",  "disp", "mem",
                                             "abs", "rel",  "sym",  "cond",
                                             "reglist", "implicit"};
static const char *const ACCESS_N[] = {"read", "write", "readwrite"};
static const int64_t ORDER_V[] = {0, 1};
static const char *const SEC_FLAG_N[] = {
    "exec",  "write",   "alloc", "readonly", "zerofill", "merge",
    "discard", "tls",   "debug", "code",     "data",     "meta"};
static const char *const CONTENT_N[] = {"unknown", "code", "data", "meta"};
static const char *const BINDING_N[] = {"global", "local", "weak", "tls"};
static const char *const VIS_N[] = {"default", "hidden", "protected",
                                    "internal"};
static const char *const SYMTYPE_N[] = {"func", "object",  "none",
                                        "notype", "section", "file"};
static const char *const OVERFLOW_N[] = {"error", "truncate"};
static const char *const ADDEND_N[] = {"explicit", "infield", "implied"};
static const char *const DEBUG_KIND_N[] = {
    "target", "register", "feature", "mnemonic", "form",  "operand",
    "encoder", "syntax",  "section", "linkage",  "reloc"};
static const char *const ASM_FORM_N[] = {
    "Fixed",         "Relative",         "Indirect",     "OpcodeRegister",
    "Unary",         "RegisterRegister", "Load",         "Store",
    "MoveImmediate", "ShiftCL",          "ShiftImmediate",
    "ArithmeticImmediate"};
static const int64_t ASM_WIDTH_V[] = {8, 32, 64};
static const int64_t ASM_REGOP_V[] = {0, 1};
static const int64_t ASM_RELOC_V[] = {0, 2, 4};
static const char *const SEMANTIC_N[] = {"Add", "Sub", "Mul",
                                         "And", "Or",  "Xor"};

#define ENUM_NAME_FN(fn, T, tbl)                                               \
    const char *fn(T v) {                                                      \
        return (unsigned)v < ARRLEN(tbl) ? tbl[v] : NULL;                      \
    }
ENUM_NAME_FN(dsym_byte_order_name, DsymByteOrder, BYTE_ORDER_N)
ENUM_NAME_FN(dsym_reg_class_name, DsymRegClass, REG_CLASS_N)
ENUM_NAME_FN(dsym_operand_kind_name, DsymOperandKind, OPERAND_KIND_N)
ENUM_NAME_FN(dsym_access_name, DsymAccess, ACCESS_N)
ENUM_NAME_FN(dsym_content_name, DsymContent, CONTENT_N)
ENUM_NAME_FN(dsym_binding_name, DsymBinding, BINDING_N)
ENUM_NAME_FN(dsym_visibility_name, DsymVisibility, VIS_N)
ENUM_NAME_FN(dsym_symtype_name, DsymSymType, SYMTYPE_N)
ENUM_NAME_FN(dsym_overflow_name, DsymOverflow, OVERFLOW_N)
ENUM_NAME_FN(dsym_addend_name, DsymAddend, ADDEND_N)
ENUM_NAME_FN(dsym_debug_kind_name, DsymDebugKind, DEBUG_KIND_N)
ENUM_NAME_FN(dsym_asm_form_name, DsymAsmForm, ASM_FORM_N)
ENUM_NAME_FN(dsym_semantic_name, DsymSemantic, SEMANTIC_N)
ENUM_NAME_FN(dsym_reg_flag_name, unsigned, REG_FLAG_N)
ENUM_NAME_FN(dsym_form_flag_name, unsigned, FORM_FLAG_N)
ENUM_NAME_FN(dsym_section_flag_name, unsigned, SEC_FLAG_N)

bool dsym_target_is_big_endian(const DsymTarget *t) {
    return t && (t->byte_order == DSYM_BO_BIG || t->byte_order == DSYM_BO_BE);
}

/* ======================================================================== */
/* Field descriptors                                                        */
/* ======================================================================== */

typedef enum FieldType {
    FT_STR,      /* const char* */
    FT_INT,      /* int64_t */
    FT_BOOL,     /* bool */
    FT_ENUM,     /* int-sized enum, string spelling */
    FT_IENUM,    /* int64_t restricted to a set of integers */
    FT_FLAGS,    /* uint32_t bitmask from a list of enum strings */
    FT_STRLIST,  /* DsymStrList */
    FT_INTLIST,  /* DsymIntList */
    FT_RECARRAY, /* T* + size_t count */
    FT_RECPTR,   /* T* (optional object) */
    FT_BACKEND   /* DsymBackend* (special) */
} FieldType;

typedef enum StrPattern {
    PAT_ANY = 0,
    PAT_NAME,    /* ^[!-~]+$, maxLength 128 */
    PAT_IDENT,   /* ^[a-z][a-z0-9_]*$ */
    PAT_JCOND,   /* ^j[a-z]+$ */
    PAT_NONEMPTY /* minLength 1 */
} StrPattern;

enum {
    O_REQ = 1u << 0,
    O_MIN = 1u << 1,
    O_MAX = 1u << 2,
    O_UNIQUE = 1u << 3
};

typedef struct Rec Rec;
typedef struct Field {
    const char *key;
    uint8_t type;
    uint8_t pat;
    uint16_t opts;
    uint32_t off;
    int32_t has_off; /* bool member, -1 = none */
    int32_t cnt_off; /* size_t member for FT_RECARRAY */
    int64_t min, max;
    const char *const *names;
    const int64_t *ivals;
    uint32_t nnames;
    uint32_t min_items, max_items; /* max 0 = unlimited */
    const Rec *sub;
} Field;

struct Rec {
    const char *name;
    size_t size;
    const Field *fields;
    size_t nfields;
};

#define HAS(T, m) ((int32_t)offsetof(T, m))
#define NOHAS (-1)

#define F_STR(T, k, m, o, p)                                                   \
    {.key = k, .type = FT_STR, .pat = p, .opts = o, .off = offsetof(T, m),     \
     .has_off = NOHAS}
#define F_INT(T, k, m, h, o, mn, mx)                                           \
    {.key = k, .type = FT_INT, .opts = o, .off = offsetof(T, m),               \
     .has_off = h, .min = mn, .max = mx}
#define F_BOOL(T, k, m, h)                                                     \
    {.key = k, .type = FT_BOOL, .off = offsetof(T, m), .has_off = h}
#define F_ENUM(T, k, m, h, o, tbl)                                             \
    {.key = k, .type = FT_ENUM, .opts = o, .off = offsetof(T, m),              \
     .has_off = h, .names = tbl, .nnames = ARRLEN(tbl)}
#define F_IENUM(T, k, m, h, o, tbl)                                            \
    {.key = k, .type = FT_IENUM, .opts = o, .off = offsetof(T, m),             \
     .has_off = h, .ivals = tbl, .nnames = ARRLEN(tbl)}
#define F_FLAGS(T, k, m, h, tbl)                                               \
    {.key = k, .type = FT_FLAGS, .off = offsetof(T, m), .has_off = h,          \
     .names = tbl, .nnames = ARRLEN(tbl)}
#define F_STRLIST(T, k, m, o, p, mi)                                           \
    {.key = k, .type = FT_STRLIST, .pat = p, .opts = o, .off = offsetof(T, m), \
     .has_off = NOHAS, .min_items = mi}
#define F_RECARRAY(T, k, m, c, h, o, sub_, mi)                                 \
    {.key = k, .type = FT_RECARRAY, .opts = o, .off = offsetof(T, m),          \
     .has_off = h, .cnt_off = (int32_t)offsetof(T, c), .sub = &sub_,           \
     .min_items = mi}
#define F_RECPTR(T, k, m, o, sub_)                                             \
    {.key = k, .type = FT_RECPTR, .opts = o, .off = offsetof(T, m),            \
     .has_off = NOHAS, .sub = &sub_}
#define REC(T, name, tbl) {name, sizeof(T), tbl, ARRLEN(tbl)}

static const Field TARGET_F[] = {
    F_STR(DsymTarget, "arch", arch, O_REQ, PAT_NAME),
    F_STR(DsymTarget, "subarch", subarch, 0, PAT_ANY),
    F_INT(DsymTarget, "addr_size", addr_size, NOHAS, O_REQ | O_MIN, 1, 0),
    F_INT(DsymTarget, "op_size", op_size, HAS(DsymTarget, has_op_size), O_MIN, 1, 0),
    F_INT(DsymTarget, "addr_def", addr_def, HAS(DsymTarget, has_addr_def), O_MIN, 1, 0),
    F_INT(DsymTarget, "align", align, HAS(DsymTarget, has_align), O_MIN, 1, 0),
    F_ENUM(DsymTarget, "byte_order", byte_order, NOHAS, O_REQ, BYTE_ORDER_N),
    F_STR(DsymTarget, "obj_format", obj_format, 0, PAT_ANY),
    F_STR(DsymTarget, "abi", abi, 0, PAT_ANY),
    F_STR(DsymTarget, "calling_conv", calling_conv, 0, PAT_ANY),
    F_STRLIST(DsymTarget, "features", features, O_UNIQUE, PAT_NAME, 0),
};
static const Rec TARGET_R = REC(DsymTarget, "target", TARGET_F);

static const Field FEATURE_F[] = {
    F_STR(DsymFeature, "name", name, O_REQ, PAT_NAME),
    F_STR(DsymFeature, "desc", desc, 0, PAT_ANY),
};
static const Rec FEATURE_R = REC(DsymFeature, "feature", FEATURE_F);

static const Field REGISTER_F[] = {
    F_STR(DsymRegister, "name", name, O_REQ, PAT_NAME),
    F_ENUM(DsymRegister, "class", reg_class, NOHAS, O_REQ, REG_CLASS_N),
    F_IENUM(DsymRegister, "width", width, NOHAS, O_REQ, REG_WIDTH_V),
    F_INT(DsymRegister, "enc", enc, NOHAS, O_REQ | O_MIN, 0, 0),
    F_STR(DsymRegister, "parent", parent, 0, PAT_NAME),
    F_INT(DsymRegister, "sub_lo", sub_lo, HAS(DsymRegister, has_sub_lo), O_MIN, 0, 0),
    F_INT(DsymRegister, "sub_hi", sub_hi, HAS(DsymRegister, has_sub_hi), O_MIN, 0, 0),
    F_FLAGS(DsymRegister, "flags", flags, HAS(DsymRegister, has_flags), REG_FLAG_N),
};
static const Rec REGISTER_R = REC(DsymRegister, "register", REGISTER_F);

static const Field MNEMONIC_F[] = {
    F_STR(DsymMnemonic, "name", name, O_REQ, PAT_NAME),
    F_STRLIST(DsymMnemonic, "aliases", aliases, O_UNIQUE, PAT_NAME, 0),
    F_BOOL(DsymMnemonic, "ci", ci, HAS(DsymMnemonic, has_ci)),
};
static const Rec MNEMONIC_R = REC(DsymMnemonic, "mnemonic", MNEMONIC_F);

static const Field ENCODER_F[] = {
    F_STR(DsymEncoder, "name", name, O_REQ, PAT_NAME),
    F_INT(DsymEncoder, "max_bytes", max_bytes, HAS(DsymEncoder, has_max_bytes), O_MIN, 0, 0),
    F_INT(DsymEncoder, "max_relocs", max_relocs, HAS(DsymEncoder, has_max_relocs), O_MIN, 0, 0),
    F_STR(DsymEncoder, "body", body, O_REQ, PAT_NONEMPTY),
};
static const Rec ENCODER_R = REC(DsymEncoder, "encoder", ENCODER_F);

static const Field OPERAND_F[] = {
    F_ENUM(DsymOperand, "kind", kind, NOHAS, O_REQ, OPERAND_KIND_N),
    F_INT(DsymOperand, "width", width, HAS(DsymOperand, has_width), O_MIN, 1, 0),
    F_ENUM(DsymOperand, "class", reg_class, HAS(DsymOperand, has_reg_class), 0, REG_CLASS_N),
    F_INT(DsymOperand, "imm_min", imm_min, HAS(DsymOperand, has_imm_min), 0, 0, 0),
    F_INT(DsymOperand, "imm_max", imm_max, HAS(DsymOperand, has_imm_max), 0, 0, 0),
    F_BOOL(DsymOperand, "signed", is_signed, HAS(DsymOperand, has_signed)),
    F_INT(DsymOperand, "role", role, HAS(DsymOperand, has_role), O_MIN, 0, 0),
    F_ENUM(DsymOperand, "access", access, HAS(DsymOperand, has_access), 0, ACCESS_N),
    F_BOOL(DsymOperand, "hidden", hidden, HAS(DsymOperand, has_hidden)),
    F_STR(DsymOperand, "constraint", constraint, 0, PAT_ANY),
};
static const Rec OPERAND_R = REC(DsymOperand, "operand", OPERAND_F);

static const Field FORM_F[] = {
    F_STR(DsymForm, "mnemonic", mnemonic, O_REQ, PAT_NAME),
    F_STR(DsymForm, "encoder", encoder, O_REQ, PAT_NAME),
    F_INT(DsymForm, "ops", ops, HAS(DsymForm, has_ops), O_MIN, 0, 0),
    F_FLAGS(DsymForm, "flags", flags, HAS(DsymForm, has_flags), FORM_FLAG_N),
    F_STRLIST(DsymForm, "required", required, O_UNIQUE, PAT_NAME, 0),
    F_STRLIST(DsymForm, "forbidden", forbidden, O_UNIQUE, PAT_NAME, 0),
    F_STRLIST(DsymForm, "syntaxes", syntaxes, O_UNIQUE, PAT_NAME, 0),
    F_RECARRAY(DsymForm, "operands", operands, operand_count,
               HAS(DsymForm, has_operands), 0, OPERAND_R, 0),
};
static const Rec FORM_R = REC(DsymForm, "form", FORM_F);

static const Field SYNTAX_F[] = {
    F_STR(DsymSyntax, "name", name, O_REQ, PAT_NAME),
    F_IENUM(DsymSyntax, "order", order, HAS(DsymSyntax, has_order), 0, ORDER_V),
    F_STR(DsymSyntax, "reg_prefix", reg_prefix, 0, PAT_ANY),
    F_STR(DsymSyntax, "imm_prefix", imm_prefix, 0, PAT_ANY),
    F_STR(DsymSyntax, "comment", comment, 0, PAT_ANY),
    F_STR(DsymSyntax, "separator", separator, 0, PAT_ANY),
    F_STR(DsymSyntax, "tmpl", tmpl, 0, PAT_ANY),
    F_BOOL(DsymSyntax, "ci", ci, HAS(DsymSyntax, has_ci)),
};
static const Rec SYNTAX_R = REC(DsymSyntax, "syntax", SYNTAX_F);

static const Field SECTION_F[] = {
    F_STR(DsymSection, "name", name, O_REQ, PAT_NAME),
    F_FLAGS(DsymSection, "flags", flags, HAS(DsymSection, has_flags), SEC_FLAG_N),
    F_INT(DsymSection, "align", align, HAS(DsymSection, has_align), O_MIN, 1, 0),
    F_INT(DsymSection, "addrspace", addrspace, HAS(DsymSection, has_addrspace), O_MIN, 0, 0),
    F_ENUM(DsymSection, "content", content, HAS(DsymSection, has_content), 0, CONTENT_N),
};
static const Rec SECTION_R = REC(DsymSection, "section", SECTION_F);

static const Field LINKAGE_F[] = {
    F_STR(DsymLinkage, "name", name, O_REQ, PAT_NAME),
    F_ENUM(DsymLinkage, "binding", binding, HAS(DsymLinkage, has_binding), 0, BINDING_N),
    F_ENUM(DsymLinkage, "visibility", visibility, HAS(DsymLinkage, has_visibility), 0, VIS_N),
    F_ENUM(DsymLinkage, "symtype", symtype, HAS(DsymLinkage, has_symtype), 0, SYMTYPE_N),
};
static const Rec LINKAGE_R = REC(DsymLinkage, "linkage", LINKAGE_F);

static const Field RELOC_F[] = {
    F_STR(DsymReloc, "name", name, O_REQ, PAT_NAME),
    F_INT(DsymReloc, "width", width, NOHAS, O_REQ | O_MIN, 1, 0),
    F_INT(DsymReloc, "offset", offset, HAS(DsymReloc, has_offset), O_MIN, 0, 0),
    F_BOOL(DsymReloc, "signed", is_signed, HAS(DsymReloc, has_signed)),
    F_BOOL(DsymReloc, "pcrel", pcrel, HAS(DsymReloc, has_pcrel)),
    F_ENUM(DsymReloc, "overflow", overflow, HAS(DsymReloc, has_overflow), 0, OVERFLOW_N),
    F_ENUM(DsymReloc, "addend", addend, HAS(DsymReloc, has_addend), 0, ADDEND_N),
    F_STR(DsymReloc, "expr", expr, 0, PAT_ANY),
};
static const Rec RELOC_R = REC(DsymReloc, "reloc", RELOC_F);

static const Field DEBUG_F[] = {
    F_ENUM(DsymDebug, "kind", kind, NOHAS, O_REQ, DEBUG_KIND_N),
    F_STR(DsymDebug, "msg", msg, O_REQ, PAT_ANY),
    F_STR(DsymDebug, "ref", ref, 0, PAT_ANY),
};
static const Rec DEBUG_R = REC(DsymDebug, "debug", DEBUG_F);

static const Field ASMENC_F[] = {
    F_STR(DsymAsmEncoding, "mnemonic", mnemonic, O_REQ, PAT_IDENT),
    F_ENUM(DsymAsmEncoding, "form", form, NOHAS, O_REQ, ASM_FORM_N),
    {.key = "widths", .type = FT_INTLIST, .opts = O_REQ | O_UNIQUE,
     .off = offsetof(DsymAsmEncoding, widths), .has_off = NOHAS,
     .ivals = ASM_WIDTH_V, .nnames = ARRLEN(ASM_WIDTH_V)},
    {.key = "opcode", .type = FT_INTLIST, .opts = O_MIN | O_MAX,
     .off = offsetof(DsymAsmEncoding, opcode), .has_off = NOHAS, .min = 0,
     .max = 255, .min_items = 1, .max_items = 3},
    F_STR(DsymAsmEncoding, "encoder", encoder, 0, PAT_ANY),
    F_INT(DsymAsmEncoding, "group", group, HAS(DsymAsmEncoding, has_group), O_MIN | O_MAX, 0, 7),
    F_IENUM(DsymAsmEncoding, "reg_operand", reg_operand, HAS(DsymAsmEncoding, has_reg_operand), 0, ASM_REGOP_V),
    F_IENUM(DsymAsmEncoding, "relocation", relocation, HAS(DsymAsmEncoding, has_relocation), 0, ASM_RELOC_V),
    F_BOOL(DsymAsmEncoding, "rex_w", rex_w, HAS(DsymAsmEncoding, has_rex_w)),
    F_STR(DsymAsmEncoding, "implicit_register", implicit_register, 0, PAT_ANY),
};
static const Rec ASMENC_R = REC(DsymAsmEncoding, "asmtk encoding", ASMENC_F);

static const Field ASMCOND_F[] = {
    F_INT(DsymAsmCondition, "code", code, NOHAS, O_REQ | O_MIN | O_MAX, 0, 15),
    F_STRLIST(DsymAsmCondition, "names", names, O_REQ | O_UNIQUE, PAT_JCOND, 1),
};
static const Rec ASMCOND_R = REC(DsymAsmCondition, "asmtk condition", ASMCOND_F);

static const Field ASMTK_F[] = {
    F_INT(DsymAsmtk, "schema", schema, NOHAS, O_REQ | O_MIN | O_MAX, 1, 1),
    F_RECARRAY(DsymAsmtk, "encodings", encodings, encoding_count, NOHAS, O_REQ, ASMENC_R, 1),
    {.key = "conditions", .type = FT_RECARRAY, .opts = O_REQ,
     .off = offsetof(DsymAsmtk, conditions), .has_off = NOHAS,
     .cnt_off = (int32_t)offsetof(DsymAsmtk, condition_count),
     .sub = &ASMCOND_R, .max_items = 16},
};
static const Rec ASMTK_R = REC(DsymAsmtk, "asmtk", ASMTK_F);

static const Field LOWERING_F[] = {
    F_STR(DsymLowering, "copy", copy, O_REQ, PAT_ANY),
    F_STR(DsymLowering, "return", ret, O_REQ, PAT_ANY),
    F_STR(DsymLowering, "stack_pointer", stack_pointer, O_REQ, PAT_ANY),
    F_STR(DsymLowering, "accumulator", accumulator, O_REQ, PAT_ANY),
    F_STR(DsymLowering, "scratch", scratch, O_REQ, PAT_ANY),
};
static const Rec LOWERING_R = REC(DsymLowering, "lowering", LOWERING_F);

static const Field ALLOCATION_F[] = {
    F_STRLIST(DsymAllocation, "reserved", reserved, O_REQ | O_UNIQUE, PAT_NONEMPTY, 0),
    F_STRLIST(DsymAllocation, "callee_saved", callee_saved, O_REQ | O_UNIQUE, PAT_NONEMPTY, 0),
    F_STRLIST(DsymAllocation, "arguments", arguments, O_REQ | O_UNIQUE, PAT_NONEMPTY, 0),
    F_STRLIST(DsymAllocation, "results", results, O_REQ | O_UNIQUE, PAT_NONEMPTY, 0),
    F_INT(DsymAllocation, "stack_alignment", stack_alignment, NOHAS, O_REQ | O_MIN, 1, 0),
};
static const Rec ALLOCATION_R = REC(DsymAllocation, "allocation", ALLOCATION_F);

static const Field VALUE_F[] = {
    F_STR(DsymValue, "symbol", symbol, O_REQ, PAT_IDENT),
    F_ENUM(DsymValue, "semantic", semantic, NOHAS, O_REQ, SEMANTIC_N),
    F_STR(DsymValue, "mnemonic", mnemonic, O_REQ, PAT_ANY),
    F_STR(DsymValue, "instruction", instruction, O_REQ, PAT_ANY),
    F_STR(DsymValue, "pattern", pattern, O_REQ, PAT_ANY),
};
static const Rec VALUE_R = REC(DsymValue, "metachain value", VALUE_F);

static const Field METACHAIN_F[] = {
    F_INT(DsymMetachain, "schema", schema, NOHAS, O_REQ | O_MIN | O_MAX, 1, 1),
    F_STR(DsymMetachain, "scheduling_model", scheduling_model, O_REQ, PAT_ANY),
    F_STR(DsymMetachain, "register_class", register_class, O_REQ, PAT_ANY),
    F_RECPTR(DsymMetachain, "lowering", lowering, O_REQ, LOWERING_R),
    F_RECPTR(DsymMetachain, "allocation", allocation, O_REQ, ALLOCATION_R),
    F_RECARRAY(DsymMetachain, "values", values, value_count, NOHAS, O_REQ, VALUE_R, 1),
};
static const Rec METACHAIN_R = REC(DsymMetachain, "metachain", METACHAIN_F);

static const Field SPEC_F[] = {
    F_INT(DsymSpec, "schema", schema, NOHAS, O_REQ | O_MIN | O_MAX, 1, 1),
    F_RECARRAY(DsymSpec, "targets", targets, target_count, NOHAS, O_REQ, TARGET_R, 1),
    F_RECARRAY(DsymSpec, "features", features, feature_count, HAS(DsymSpec, has_features), 0, FEATURE_R, 0),
    F_RECARRAY(DsymSpec, "registers", registers, register_count, HAS(DsymSpec, has_registers), 0, REGISTER_R, 0),
    F_RECARRAY(DsymSpec, "mnemonics", mnemonics, mnemonic_count, HAS(DsymSpec, has_mnemonics), 0, MNEMONIC_R, 0),
    F_RECARRAY(DsymSpec, "encoders", encoders, encoder_count, HAS(DsymSpec, has_encoders), 0, ENCODER_R, 0),
    F_RECARRAY(DsymSpec, "forms", forms, form_count, HAS(DsymSpec, has_forms), 0, FORM_R, 0),
    F_RECARRAY(DsymSpec, "syntaxes", syntaxes, syntax_count, HAS(DsymSpec, has_syntaxes), 0, SYNTAX_R, 0),
    F_RECARRAY(DsymSpec, "sections", sections, section_count, HAS(DsymSpec, has_sections), 0, SECTION_R, 0),
    F_RECARRAY(DsymSpec, "linkages", linkages, linkage_count, HAS(DsymSpec, has_linkages), 0, LINKAGE_R, 0),
    F_RECARRAY(DsymSpec, "relocs", relocs, reloc_count, HAS(DsymSpec, has_relocs), 0, RELOC_R, 0),
    F_RECARRAY(DsymSpec, "debug", debug, debug_count, HAS(DsymSpec, has_debug), 0, DEBUG_R, 0),
    F_RECPTR(DsymSpec, "asmtk", asmtk, 0, ASMTK_R),
    F_RECPTR(DsymSpec, "metachain", metachain, 0, METACHAIN_R),
    {.key = "backend", .type = FT_BACKEND, .off = offsetof(DsymSpec, backend),
     .has_off = NOHAS},
};
static const Rec SPEC_R = REC(DsymSpec, "document", SPEC_F);

/* ======================================================================== */
/* Binder: tree -> struct                                                   */
/* ======================================================================== */

typedef struct Binder {
    DsymArena *arena; /* destination (spec) arena */
    DsymDiags *diags;
    const char *file;
    char path[512];
    size_t plen;
    bool failed;
} Binder;

static size_t path_push(Binder *b, const char *seg) {
    size_t old = b->plen;
    int n = snprintf(b->path + b->plen, sizeof b->path - b->plen, "/%s", seg);
    if (n > 0) b->plen = b->plen + (size_t)n < sizeof b->path ? b->plen + (size_t)n
                                                              : sizeof b->path - 1;
    return old;
}

static size_t path_push_idx(Binder *b, size_t i) {
    char tmp[24];
    snprintf(tmp, sizeof tmp, "%zu", i);
    return path_push(b, tmp);
}

static void path_pop(Binder *b, size_t old) {
    b->plen = old;
    b->path[old] = 0;
}

#if defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
static void berr(Binder *b, const DsymNode *n, const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    dsym_diag_add(b->diags, DSYM_ERROR, b->file, n ? n->line : 0,
                  n ? n->col : 0, b->path, "%s", msg);
    b->failed = true;
}

/* String view of a scalar for string-typed fields. YAML plain scalars that
   resolved to numbers/bools keep their source text and are accepted. */
static bool scalar_text(const DsymNode *n, const char **s, size_t *len) {
    if (n->kind == DSYM_NODE_STR) {
        *s = n->v.str.ptr;
        *len = n->v.str.len;
        return true;
    }
    if (n->text && n->kind != DSYM_NODE_SEQ && n->kind != DSYM_NODE_MAP) {
        *s = n->text;
        *len = strlen(n->text);
        return true;
    }
    return false;
}

static bool check_pattern(Binder *b, const DsymNode *n, const char *s,
                          size_t len, StrPattern p) {
    switch (p) {
    case PAT_ANY: return true;
    case PAT_NONEMPTY:
        if (len == 0) {
            berr(b, n, "string must not be empty");
            return false;
        }
        return true;
    case PAT_NAME:
        if (len == 0 || len > DSYM_NAME_MAX) {
            berr(b, n, "name must be 1..%d characters (got %zu)",
                 DSYM_NAME_MAX, len);
            return false;
        }
        for (size_t i = 0; i < len; i++)
            if ((unsigned char)s[i] < '!' || (unsigned char)s[i] > '~') {
                berr(b, n, "name '%.*s' must be printable ASCII without spaces",
                     (int)len, s);
                return false;
            }
        return true;
    case PAT_IDENT:
        if (len == 0 || !(s[0] >= 'a' && s[0] <= 'z')) goto bad_ident;
        for (size_t i = 1; i < len; i++)
            if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') ||
                  s[i] == '_'))
                goto bad_ident;
        return true;
    bad_ident:
        berr(b, n, "'%.*s' must match ^[a-z][a-z0-9_]*$", (int)len, s);
        return false;
    case PAT_JCOND:
        if (len < 2 || s[0] != 'j') goto bad_j;
        for (size_t i = 1; i < len; i++)
            if (!(s[i] >= 'a' && s[i] <= 'z')) goto bad_j;
        return true;
    bad_j:
        berr(b, n, "'%.*s' must match ^j[a-z]+$", (int)len, s);
        return false;
    }
    return true;
}

static bool bind_str_value(Binder *b, const DsymNode *n, StrPattern p,
                           const char **out) {
    const char *s;
    size_t len;
    if (!scalar_text(n, &s, &len)) {
        berr(b, n, "expected a string, got %s", dsym_node_kind_name(n->kind));
        return false;
    }
    if (!check_pattern(b, n, s, len, p)) return false;
    *out = dsym_arena_strndup(b->arena, s, len);
    return *out != NULL;
}

static bool node_int(const DsymNode *n, int64_t *out) {
    if (n->kind == DSYM_NODE_INT) {
        *out = n->v.i;
        return true;
    }
    if (n->kind == DSYM_NODE_REAL && n->v.r == (double)(int64_t)n->v.r &&
        n->v.r >= -9.2e18 && n->v.r <= 9.2e18) {
        *out = (int64_t)n->v.r; /* JSON "integer" accepts 3.0 */
        return true;
    }
    return false;
}

static bool bind_int_value(Binder *b, const Field *f, const DsymNode *n,
                           int64_t *out) {
    if (!node_int(n, out)) {
        if (n->kind == DSYM_NODE_UINT)
            berr(b, n, "integer %" PRIu64 " out of range", n->v.u);
        else
            berr(b, n, "expected an integer, got %s",
                 dsym_node_kind_name(n->kind));
        return false;
    }
    if ((f->opts & O_MIN) && (f->opts & O_MAX) && f->min == f->max &&
        *out != f->min) {
        berr(b, n, "must be %" PRId64 " (got %" PRId64 ")", f->min, *out);
        return false;
    }
    if ((f->opts & O_MIN) && *out < f->min) {
        berr(b, n, "%" PRId64 " is below the minimum %" PRId64, *out, f->min);
        return false;
    }
    if ((f->opts & O_MAX) && *out > f->max) {
        berr(b, n, "%" PRId64 " is above the maximum %" PRId64, *out, f->max);
        return false;
    }
    return true;
}

static int enum_lookup(const Field *f, const char *s, size_t len) {
    for (uint32_t i = 0; i < f->nnames; i++)
        if (strlen(f->names[i]) == len && memcmp(f->names[i], s, len) == 0)
            return (int)i;
    return -1;
}

static void enum_expected(const Field *f, char *buf, size_t cap) {
    size_t o = 0;
    buf[0] = 0;
    for (uint32_t i = 0; i < f->nnames && o + 2 < cap; i++) {
        int n;
        if (f->names)
            n = snprintf(buf + o, cap - o, "%s%s", i ? ", " : "", f->names[i]);
        else
            n = snprintf(buf + o, cap - o, "%s%" PRId64, i ? ", " : "",
                         f->ivals[i]);
        if (n < 0) break;
        o += (size_t)n;
    }
}

static bool bind_enum_value(Binder *b, const Field *f, const DsymNode *n,
                            int *out) {
    const char *s;
    size_t len;
    char exp[256];
    if (!scalar_text(n, &s, &len)) {
        enum_expected(f, exp, sizeof exp);
        berr(b, n, "expected one of [%s], got %s", exp,
             dsym_node_kind_name(n->kind));
        return false;
    }
    int v = enum_lookup(f, s, len);
    if (v < 0) {
        enum_expected(f, exp, sizeof exp);
        berr(b, n, "'%.*s' is not one of [%s]", (int)len, s, exp);
        return false;
    }
    *out = v;
    return true;
}

static void *fld(void *rec, uint32_t off) { return (char *)rec + off; }

static bool bind_record(Binder *b, const Rec *r, const DsymNode *n, void *out);

static bool bind_field(Binder *b, const Field *f, const DsymNode *n,
                       void *rec) {
    void *p = fld(rec, f->off);
    switch ((FieldType)f->type) {
    case FT_STR: return bind_str_value(b, n, (StrPattern)f->pat, (const char **)p);
    case FT_INT: return bind_int_value(b, f, n, (int64_t *)p);
    case FT_BOOL:
        if (n->kind != DSYM_NODE_BOOL) {
            berr(b, n, "expected a boolean, got %s", dsym_node_kind_name(n->kind));
            return false;
        }
        *(bool *)p = n->v.b;
        return true;
    case FT_ENUM: {
        int v;
        if (!bind_enum_value(b, f, n, &v)) return false;
        memcpy(p, &v, sizeof v);
        return true;
    }
    case FT_IENUM: {
        int64_t v;
        char exp[256];
        if (!node_int(n, &v)) {
            enum_expected(f, exp, sizeof exp);
            berr(b, n, "expected one of [%s], got %s", exp,
                 dsym_node_kind_name(n->kind));
            return false;
        }
        for (uint32_t i = 0; i < f->nnames; i++)
            if (f->ivals[i] == v) {
                *(int64_t *)p = v;
                return true;
            }
        enum_expected(f, exp, sizeof exp);
        berr(b, n, "%" PRId64 " is not one of [%s]", v, exp);
        return false;
    }
    case FT_FLAGS: {
        if (n->kind != DSYM_NODE_SEQ) {
            berr(b, n, "expected a list of flags, got %s",
                 dsym_node_kind_name(n->kind));
            return false;
        }
        uint32_t mask = 0;
        bool ok = true;
        for (size_t i = 0; i < n->v.seq.count; i++) {
            size_t old = path_push_idx(b, i);
            int v;
            if (bind_enum_value(b, f, n->v.seq.items[i], &v)) {
                if (mask & (1u << v)) {
                    berr(b, n->v.seq.items[i], "duplicate flag '%s'", f->names[v]);
                    ok = false;
                }
                mask |= 1u << v;
            } else {
                ok = false;
            }
            path_pop(b, old);
        }
        *(uint32_t *)p = mask;
        return ok;
    }
    case FT_STRLIST: {
        DsymStrList *l = p;
        if (n->kind != DSYM_NODE_SEQ) {
            berr(b, n, "expected a list of strings, got %s",
                 dsym_node_kind_name(n->kind));
            return false;
        }
        l->present = true;
        l->count = 0;
        l->items = dsym_arena_calloc(b->arena, n->v.seq.count + 1, sizeof(char *));
        if (!l->items) return false;
        if (n->v.seq.count < f->min_items) {
            berr(b, n, "needs at least %u item(s)", f->min_items);
            return false;
        }
        bool ok = true;
        for (size_t i = 0; i < n->v.seq.count; i++) {
            size_t old = path_push_idx(b, i);
            const char *s = NULL;
            if (bind_str_value(b, n->v.seq.items[i], (StrPattern)f->pat, &s)) {
                if (f->opts & O_UNIQUE)
                    for (size_t j = 0; j < l->count; j++)
                        if (strcmp(l->items[j], s) == 0) {
                            berr(b, n->v.seq.items[i], "duplicate item '%s'", s);
                            ok = false;
                            break;
                        }
                l->items[l->count++] = s;
            } else {
                ok = false;
            }
            path_pop(b, old);
        }
        return ok;
    }
    case FT_INTLIST: {
        DsymIntList *l = p;
        if (n->kind != DSYM_NODE_SEQ) {
            berr(b, n, "expected a list of integers, got %s",
                 dsym_node_kind_name(n->kind));
            return false;
        }
        l->present = true;
        l->count = 0;
        l->items = dsym_arena_calloc(b->arena, n->v.seq.count + 1, sizeof(int64_t));
        if (!l->items) return false;
        if (n->v.seq.count < f->min_items ||
            (f->max_items && n->v.seq.count > f->max_items)) {
            if (f->max_items)
                berr(b, n, "needs %u..%u item(s), got %zu", f->min_items,
                     f->max_items, n->v.seq.count);
            else
                berr(b, n, "needs at least %u item(s)", f->min_items);
            return false;
        }
        bool ok = true;
        for (size_t i = 0; i < n->v.seq.count; i++) {
            size_t old = path_push_idx(b, i);
            const DsymNode *it = n->v.seq.items[i];
            int64_t v;
            if (!node_int(it, &v)) {
                berr(b, it, "expected an integer, got %s",
                     dsym_node_kind_name(it->kind));
                ok = false;
            } else if (((f->opts & O_MIN) && v < f->min) ||
                       ((f->opts & O_MAX) && v > f->max)) {
                berr(b, it, "%" PRId64 " is outside %" PRId64 "..%" PRId64, v,
                     f->min, f->max);
                ok = false;
            } else {
                if (f->ivals) {
                    bool found = false;
                    for (uint32_t k = 0; k < f->nnames; k++)
                        found |= f->ivals[k] == v;
                    if (!found) {
                        char exp[128];
                        enum_expected(f, exp, sizeof exp);
                        berr(b, it, "%" PRId64 " is not one of [%s]", v, exp);
                        ok = false;
                    }
                }
                if (f->opts & O_UNIQUE)
                    for (size_t j = 0; j < l->count; j++)
                        if (l->items[j] == v) {
                            berr(b, it, "duplicate item %" PRId64, v);
                            ok = false;
                        }
                l->items[l->count++] = v;
            }
            path_pop(b, old);
        }
        return ok;
    }
    case FT_RECARRAY: {
        if (n->kind != DSYM_NODE_SEQ) {
            berr(b, n, "expected a list of %s records, got %s", f->sub->name,
                 dsym_node_kind_name(n->kind));
            return false;
        }
        size_t cnt = n->v.seq.count;
        if (cnt < f->min_items) {
            berr(b, n, "needs at least %u %s record(s)", f->min_items,
                 f->sub->name);
            return false;
        }
        if (f->max_items && cnt > f->max_items) {
            berr(b, n, "at most %u %s record(s) allowed, got %zu", f->max_items,
                 f->sub->name, cnt);
            return false;
        }
        char *items = dsym_arena_calloc(b->arena, cnt ? cnt : 1, f->sub->size);
        if (!items) return false;
        bool ok = true;
        for (size_t i = 0; i < cnt; i++) {
            size_t old = path_push_idx(b, i);
            ok &= bind_record(b, f->sub, n->v.seq.items[i], items + i * f->sub->size);
            path_pop(b, old);
        }
        *(void **)p = items;
        *(size_t *)fld(rec, (uint32_t)f->cnt_off) = cnt;
        return ok;
    }
    case FT_RECPTR: {
        void *sub = dsym_arena_alloc(b->arena, f->sub->size);
        if (!sub) return false;
        bool ok = bind_record(b, f->sub, n, sub);
        *(void **)p = sub;
        return ok;
    }
    case FT_BACKEND: {
        if (n->kind != DSYM_NODE_MAP) {
            berr(b, n, "backend must be a mapping");
            return false;
        }
        DsymBackend *be = dsym_arena_alloc(b->arena, sizeof *be);
        if (!be) return false;
        *(DsymBackend **)p = be;
        const DsymNode *file = dsym_map_get(n, "file");
        if (file && n->v.map.count == 1) {
            /* reference form: {file: backend/<name>.json} */
            size_t old = path_push(b, "file");
            bool ok = bind_str_value(b, file, PAT_ANY, &be->file);
            if (ok) {
                const char *s = be->file;
                static const char pre[] = "backend/";
                size_t L = strlen(s);
                bool good = L > sizeof pre - 1 + 5 &&
                            memcmp(s, pre, sizeof pre - 1) == 0 &&
                            strcmp(s + L - 5, ".json") == 0;
                for (size_t i = sizeof pre - 1; good && i < L; i++)
                    good = isalnum((unsigned char)s[i]) || s[i] == '_' ||
                           s[i] == '.' || s[i] == '-';
                if (!good) {
                    berr(b, file,
                         "'%s' must match ^backend/[A-Za-z0-9_.\\-]+\\.json$", s);
                    ok = false;
                }
            }
            path_pop(b, old);
            return ok;
        }
        /* embedded form (JSON output merges the backend document) */
        be->document = dsym_node_clone(b->arena, n);
        return be->document != NULL;
    }
    }
    return false;
}

static bool bind_record(Binder *b, const Rec *r, const DsymNode *n, void *out) {
    if (!n || n->kind != DSYM_NODE_MAP) {
        berr(b, n, "%s must be a mapping, got %s", r->name,
             n ? dsym_node_kind_name(n->kind) : "nothing");
        return false;
    }
    bool ok = true;
    /* unknown + duplicate keys */
    for (size_t i = 0; i < n->v.map.count; i++) {
        const DsymPair *pr = &n->v.map.items[i];
        bool known = false;
        for (size_t k = 0; k < r->nfields && !known; k++)
            known = strlen(r->fields[k].key) == pr->key_len &&
                    memcmp(r->fields[k].key, pr->key, pr->key_len) == 0;
        if (!known) {
            berr(b, pr->value, "unknown key '%.*s' in %s", (int)pr->key_len,
                 pr->key, r->name);
            ok = false;
        }
        for (size_t j = 0; j < i; j++)
            if (n->v.map.items[j].key_len == pr->key_len &&
                memcmp(n->v.map.items[j].key, pr->key, pr->key_len) == 0) {
                berr(b, pr->value, "duplicate key '%.*s'", (int)pr->key_len,
                     pr->key);
                ok = false;
            }
    }
    for (size_t k = 0; k < r->nfields; k++) {
        const Field *f = &r->fields[k];
        const DsymNode *v = dsym_map_get(n, f->key);
        if (!v) {
            if (f->opts & O_REQ) {
                berr(b, n, "%s is missing required key '%s'", r->name, f->key);
                ok = false;
            }
            continue;
        }
        size_t old = path_push(b, f->key);
        bool fok = bind_field(b, f, v, out);
        if (fok && f->has_off >= 0) *(bool *)fld(out, (uint32_t)f->has_off) = true;
        ok &= fok;
        path_pop(b, old);
    }
    return ok;
}

/* ======================================================================== */
/* Unbinder: struct -> tree                                                 */
/* ======================================================================== */

typedef struct Unbinder {
    DsymArena *arena;
    uint32_t flags;
    bool embed_backend;
} Unbinder;

static DsymNode *unbind_record(Unbinder *u, const Rec *r, const void *rec);

static bool field_present(const Field *f, const void *rec) {
    const void *p = (const char *)rec + f->off;
    if (f->has_off >= 0) return *(const bool *)((const char *)rec + f->has_off);
    switch ((FieldType)f->type) {
    case FT_STR: return *(const char *const *)p != NULL;
    case FT_STRLIST: return ((const DsymStrList *)p)->present;
    case FT_INTLIST: return ((const DsymIntList *)p)->present;
    case FT_RECPTR:
    case FT_BACKEND: return *(void *const *)p != NULL;
    case FT_RECARRAY:
        return (f->opts & O_REQ) ||
               *(const size_t *)((const char *)rec + f->cnt_off) > 0;
    default: return (f->opts & O_REQ) != 0;
    }
}

static DsymNode *unbind_field(Unbinder *u, const Field *f, const void *rec) {
    const void *p = (const char *)rec + f->off;
    DsymArena *a = u->arena;
    switch ((FieldType)f->type) {
    case FT_STR: return dsym_node_str_ref(a, *(const char *const *)p);
    case FT_INT: return dsym_node_int(a, *(const int64_t *)p);
    case FT_IENUM: return dsym_node_int(a, *(const int64_t *)p);
    case FT_BOOL: return dsym_node_bool(a, *(const bool *)p);
    case FT_ENUM: {
        int v;
        memcpy(&v, p, sizeof v);
        if (v < 0 || (uint32_t)v >= f->nnames) return NULL;
        return dsym_node_str_ref(a, f->names[v]);
    }
    case FT_FLAGS: {
        DsymNode *s = dsym_node_seq(a);
        uint32_t m = *(const uint32_t *)p;
        for (uint32_t i = 0; s && i < f->nnames; i++)
            if (m & (1u << i)) dsym_seq_push(a, s, dsym_node_str_ref(a, f->names[i]));
        return s;
    }
    case FT_STRLIST: {
        const DsymStrList *l = p;
        DsymNode *s = dsym_node_seq(a);
        for (size_t i = 0; s && i < l->count; i++)
            if (!dsym_seq_push(a, s, dsym_node_str_ref(a, l->items[i]))) return NULL;
        return s;
    }
    case FT_INTLIST: {
        const DsymIntList *l = p;
        DsymNode *s = dsym_node_seq(a);
        for (size_t i = 0; s && i < l->count; i++) {
            DsymNode *it = dsym_node_int(a, l->items[i]);
            if (it && f->max == 255) it->hints |= DSYM_HINT_HEX; /* opcodes */
            if (!dsym_seq_push(a, s, it)) return NULL;
        }
        return s;
    }
    case FT_RECARRAY: {
        const char *items = *(const char *const *)p;
        size_t cnt = *(const size_t *)((const char *)rec + f->cnt_off);
        DsymNode *s = dsym_node_seq(a);
        for (size_t i = 0; s && i < cnt; i++)
            if (!dsym_seq_push(a, s, unbind_record(u, f->sub, items + i * f->sub->size)))
                return NULL;
        return s;
    }
    case FT_RECPTR: return unbind_record(u, f->sub, *(const void *const *)p);
    case FT_BACKEND: {
        const DsymBackend *be = *(const DsymBackend *const *)p;
        if (be->document && (u->embed_backend || !be->file)) return be->document;
        if (!be->file) return NULL;
        DsymNode *m = dsym_node_map(a);
        if (!m || !dsym_map_put_ref(a, m, "file", dsym_node_str_ref(a, be->file)))
            return NULL;
        return m;
    }
    }
    return NULL;
}

static DsymNode *unbind_record(Unbinder *u, const Rec *r, const void *rec) {
    if (!rec) return NULL;
    DsymNode *m = dsym_node_map(u->arena);
    if (!m) return NULL;
    for (size_t k = 0; k < r->nfields; k++) {
        const Field *f = &r->fields[k];
        if (!field_present(f, rec)) continue;
        if (f->type == FT_BACKEND) {
            const DsymBackend *be = *(const DsymBackend *const *)((const char *)rec + f->off);
            if (!be->file && !be->document) continue;
        }
        DsymNode *v = unbind_field(u, f, rec);
        if (!v) return NULL;
        if (!dsym_map_put_ref(u->arena, m, f->key, v)) return NULL;
    }
    return m;
}

DsymNode *dsym_spec_to_tree(const DsymSpec *spec, DsymArena *a,
                            uint32_t write_flags) {
    Unbinder u = {a, write_flags,
                  (write_flags & DSYM_WRITE_EMBED_BACKEND) &&
                      !(write_flags & DSYM_WRITE_REF_BACKEND)};
    return unbind_record(&u, &SPEC_R, spec);
}

/* ======================================================================== */
/* Name index                                                               */
/* ======================================================================== */

typedef struct StrMap {
    const char **keys;
    int32_t *vals;
    size_t cap; /* power of two */
    size_t count;
} StrMap;

typedef struct DsymIndex {
    StrMap features, registers, mnemonics, aliases, ci_mnemonics, encoders,
        syntaxes, sections, linkages, relocs;
    int32_t *form_start; /* mnemonic_count + 1 */
    int32_t *form_list;
} DsymIndex;

static uint64_t hash_str(const char *s) {
    uint64_t h = 1469598103934665603ull; /* FNV-1a */
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 1099511628211ull;
    return h;
}

static bool strmap_init(DsymArena *a, StrMap *m, size_t n) {
    size_t cap = 8;
    while (cap < n * 2 + 1) cap *= 2;
    m->keys = dsym_arena_calloc(a, cap, sizeof *m->keys);
    m->vals = dsym_arena_calloc(a, cap, sizeof *m->vals);
    m->cap = cap;
    m->count = 0;
    return m->keys && m->vals;
}

/* Returns the existing value if the key is already present, else -1. */
static int32_t strmap_put(StrMap *m, const char *key, int32_t val) {
    size_t i = (size_t)hash_str(key) & (m->cap - 1);
    while (m->keys[i]) {
        if (strcmp(m->keys[i], key) == 0) return m->vals[i];
        i = (i + 1) & (m->cap - 1);
    }
    m->keys[i] = key;
    m->vals[i] = val;
    m->count++;
    return -1;
}

static int32_t strmap_get(const StrMap *m, const char *key) {
    if (!m->cap || !key) return -1;
    size_t i = (size_t)hash_str(key) & (m->cap - 1);
    while (m->keys[i]) {
        if (strcmp(m->keys[i], key) == 0) return m->vals[i];
        i = (i + 1) & (m->cap - 1);
    }
    return -1;
}

#define FIND_FN(fn, table)                                                     \
    int32_t fn(const DsymSpec *s, const char *name) {                          \
        return s && s->index ? strmap_get(&s->index->table, name) : -1;        \
    }
FIND_FN(dsym_spec_find_feature, features)
FIND_FN(dsym_spec_find_register, registers)
FIND_FN(dsym_spec_find_mnemonic, mnemonics)
FIND_FN(dsym_spec_find_encoder, encoders)
FIND_FN(dsym_spec_find_syntax, syntaxes)
FIND_FN(dsym_spec_find_section, sections)
FIND_FN(dsym_spec_find_linkage, linkages)
FIND_FN(dsym_spec_find_reloc, relocs)

static void lower_copy(char *dst, const char *src, size_t cap) {
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = 0;
}

int32_t dsym_spec_find_mnemonic_or_alias(const DsymSpec *s, const char *name) {
    if (!s || !s->index || !name) return -1;
    int32_t i = strmap_get(&s->index->mnemonics, name);
    if (i >= 0) return i;
    if ((i = strmap_get(&s->index->aliases, name)) >= 0) return i;
    char low[DSYM_NAME_MAX + 1];
    lower_copy(low, name, sizeof low);
    return strmap_get(&s->index->ci_mnemonics, low);
}

size_t dsym_spec_forms_for_mnemonic(const DsymSpec *s, int32_t mi, int32_t *out,
                                    size_t cap) {
    if (!s || !s->index || !s->index->form_start || mi < 0 ||
        (size_t)mi >= s->mnemonic_count)
        return 0;
    size_t b = (size_t)s->index->form_start[mi], e = (size_t)s->index->form_start[mi + 1];
    for (size_t i = b; i < e && i - b < cap; i++) out[i - b] = s->index->form_list[i];
    return e - b;
}

/* ======================================================================== */
/* Resolution & semantic checks                                             */
/* ======================================================================== */

typedef struct Resolver {
    DsymSpec *s;
    DsymDiags *diags;
    uint32_t flags;
    size_t errors_before;
    char path[160];
} Resolver;

#if defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
static void rdiag(Resolver *r, DsymSeverity sev, const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    if (sev == DSYM_WARNING && (r->flags & DSYM_LOAD_STRICT)) sev = DSYM_ERROR;
    dsym_diag_add(r->diags, sev, r->s->source_path, 0, 0, r->path, "%s", msg);
}

#define RPATH(r, ...) snprintf((r)->path, sizeof (r)->path, __VA_ARGS__)

static bool is_pow2(int64_t v) { return v > 0 && (v & (v - 1)) == 0; }

static bool index_names(Resolver *r, StrMap *m, const char *what,
                        const char *table, const void *items, size_t count,
                        size_t elem, size_t name_off) {
    if (!strmap_init(r->s->arena, m, count)) return false;
    for (size_t i = 0; i < count; i++) {
        const char *name = *(const char *const *)((const char *)items + i * elem + name_off);
        if (!name) continue;
        int32_t prev = strmap_put(m, name, (int32_t)i);
        if (prev >= 0) {
            RPATH(r, "/%s/%zu/name", table, i);
            rdiag(r, DSYM_ERROR, "duplicate %s name '%s' (first defined at /%s/%d)",
                  what, name, table, prev);
        }
    }
    return true;
}

static int32_t *resolve_list(Resolver *r, const DsymStrList *l, const StrMap *m,
                             const char *what, const char *pathfmt, size_t idx,
                             const char *key) {
    int32_t *out = dsym_arena_calloc(r->s->arena, l->count + 1, sizeof *out);
    if (!out) return NULL;
    for (size_t i = 0; i < l->count; i++) {
        out[i] = strmap_get(m, l->items[i]);
        if (out[i] < 0) {
            RPATH(r, pathfmt, idx, key, i);
            rdiag(r, DSYM_ERROR, "unknown %s '%s'", what, l->items[i]);
        }
    }
    return out;
}

static int32_t reloc_lookup_cb(void *ctx, const char *name) {
    return strmap_get(&((DsymIndex *)ctx)->relocs, name);
}

/* Match only folded PARAM CONST ADD CONST MOD subexpressions. */
static bool biased_modulo(const DsymProgram *p, DsymExprRef expr) {
    for (size_t j = 0; j + 4 < expr.len; j++) {
        const DsymInsn *c = p->code + expr.start + j;
        if (c[0].op != DSYM_OP_PARAM || c[1].op != DSYM_OP_CONST ||
            c[2].op != DSYM_OP_ADD || c[3].op != DSYM_OP_CONST ||
            c[4].op != DSYM_OP_MOD) continue;
        int64_t modulus = c[3].imm;
        if (modulus > 0 && ((uint64_t)modulus & ((uint64_t)modulus - 1)) == 0 &&
            c[1].imm != 0 && c[1].imm != modulus) return true;
    }
    return false;
}

static bool biased_opt_out(const char *text) {
    const char *comment = strchr(text, '#');
    if (!comment) return false;
    comment++;
    while (*comment == ' ' || *comment == '\t') comment++;
    if (strncmp(comment, "biased", 6)) return false;
    comment += 6;
    while (*comment == ' ' || *comment == '\t' || *comment == '\r') comment++;
    return !*comment;
}

static bool operand_visible(const DsymOperand *o) {
    return !o->hidden && o->kind != DSYM_OK_IMPLICIT;
}

static size_t visible_operand_count(const DsymForm *f) {
    size_t count = 0;
    for (size_t i = 0; i < f->operand_count; i++)
        if (operand_visible(&f->operands[i])) count++;
    return count;
}

static bool param_is_relocated(const DsymProgram *p, size_t param) {
    for (size_t i = 0; i < p->stmt_count; i++)
        if (p->stmts[i].kind == DSYM_ST_RELOC && p->stmts[i].sym_param == param)
            return true;
    return false;
}

/* The relocation must be wholly contained in the most recent integer field.
   Subtraction, not offset + width, keeps hostile INT64_MAX metadata safe. */
static bool reloc_fits_field(const DsymReloc *r, size_t bytes) {
    uint64_t bits = (uint64_t)bytes * 8;
    int64_t offset = r->has_offset ? r->offset : 0;
    return r->width > 0 && offset >= 0 && (uint64_t)r->width <= bits &&
           (uint64_t)offset <= bits - (uint64_t)r->width;
}

bool dsym_spec_resolve(DsymSpec *s, uint32_t flags, DsymDiags *diags) {
    if (!s) return false;
    DsymDiags local;
    bool own = !diags;
    if (own) {
        dsym_diags_init(&local);
        diags = &local;
    }
    Resolver R = {s, diags, flags, diags->errors, ""};
    Resolver *r = &R;
    DsymIndex *ix = dsym_arena_alloc(s->arena, sizeof *ix);
    if (!ix) goto oom;
    s->index = ix;

#define IDX(map, what, tbl, arr, cnt, T)                                       \
    if (!index_names(r, &ix->map, what, tbl, s->arr, s->cnt, sizeof(T),        \
                     offsetof(T, name)))                                       \
        goto oom;
    IDX(features, "feature", "features", features, feature_count, DsymFeature)
    IDX(registers, "register", "registers", registers, register_count, DsymRegister)
    IDX(mnemonics, "mnemonic", "mnemonics", mnemonics, mnemonic_count, DsymMnemonic)
    IDX(encoders, "encoder", "encoders", encoders, encoder_count, DsymEncoder)
    IDX(syntaxes, "syntax", "syntaxes", syntaxes, syntax_count, DsymSyntax)
    IDX(sections, "section", "sections", sections, section_count, DsymSection)
    IDX(linkages, "linkage", "linkages", linkages, linkage_count, DsymLinkage)
    IDX(relocs, "relocation", "relocs", relocs, reloc_count, DsymReloc)
#undef IDX

    /* mnemonic aliases and case-insensitive spellings */
    {
        size_t na = 0, nci = 0;
        for (size_t i = 0; i < s->mnemonic_count; i++) {
            na += s->mnemonics[i].aliases.count;
            if (s->mnemonics[i].ci) nci += 1 + s->mnemonics[i].aliases.count;
        }
        if (!strmap_init(s->arena, &ix->aliases, na) ||
            !strmap_init(s->arena, &ix->ci_mnemonics, nci))
            goto oom;
        for (size_t i = 0; i < s->mnemonic_count; i++) {
            const DsymMnemonic *m = &s->mnemonics[i];
            for (size_t k = 0; k < m->aliases.count; k++) {
                const char *al = m->aliases.items[k];
                RPATH(r, "/mnemonics/%zu/aliases/%zu", i, k);
                int32_t clash = strmap_get(&ix->mnemonics, al);
                if (clash >= 0 && (size_t)clash != i)
                    rdiag(r, DSYM_WARNING, "alias '%s' shadows mnemonic '%s'", al,
                          s->mnemonics[clash].name);
                int32_t prev = strmap_put(&ix->aliases, al, (int32_t)i);
                if (prev >= 0 && (size_t)prev != i)
                    rdiag(r, DSYM_ERROR, "alias '%s' already belongs to '%s'", al,
                          s->mnemonics[prev].name);
            }
            if (m->ci) {
                for (size_t k = 0; k <= m->aliases.count; k++) {
                    const char *nm = k == 0 ? m->name : m->aliases.items[k - 1];
                    char *low = dsym_arena_strdup(s->arena, nm);
                    if (!low) goto oom;
                    lower_copy(low, nm, strlen(nm) + 1);
                    strmap_put(&ix->ci_mnemonics, low, (int32_t)i);
                }
            }
        }
    }

    /* targets */
    for (size_t i = 0; i < s->target_count; i++) {
        DsymTarget *t = &s->targets[i];
        if (t->has_align && !is_pow2(t->align)) {
            RPATH(r, "/targets/%zu/align", i);
            rdiag(r, DSYM_ERROR, "align %" PRId64 " is not a power of two", t->align);
        }
        if (!(t->feature_index = resolve_list(r, &t->features, &ix->features,
                                              "feature", "/targets/%zu/%s/%zu",
                                              i, "features")))
            goto oom;
    }

    /* registers: parent links, sub-ranges, cycles */
    for (size_t i = 0; i < s->register_count; i++) {
        DsymRegister *g = &s->registers[i];
        g->parent_index = -1;
        if (g->parent) {
            g->parent_index = strmap_get(&ix->registers, g->parent);
            RPATH(r, "/registers/%zu/parent", i);
            if (g->parent_index < 0)
                rdiag(r, DSYM_ERROR, "unknown parent register '%s'", g->parent);
            else if ((size_t)g->parent_index == i)
                rdiag(r, DSYM_ERROR, "register '%s' is its own parent", g->name);
        }
        if (g->has_sub_lo != g->has_sub_hi || (g->has_sub_lo && !g->parent)) {
            RPATH(r, "/registers/%zu", i);
            rdiag(r, DSYM_WARNING,
                  "sub_lo/sub_hi should be given together and with a parent");
        }
        if (g->has_sub_lo && g->has_sub_hi) {
            RPATH(r, "/registers/%zu/sub_hi", i);
            if (g->sub_hi <= g->sub_lo)
                rdiag(r, DSYM_ERROR, "sub_hi (%" PRId64 ") must exceed sub_lo (%" PRId64 ")",
                      g->sub_hi, g->sub_lo);
            else if (g->sub_hi - g->sub_lo != g->width && (flags & DSYM_LOAD_LINT))
                rdiag(r, DSYM_NOTE,
                      "sub-range %" PRId64 "..%" PRId64 " does not match width %" PRId64,
                      g->sub_lo, g->sub_hi, g->width);
            if (g->parent_index >= 0 &&
                g->sub_hi > s->registers[g->parent_index].width)
                rdiag(r, DSYM_ERROR,
                      "sub_hi %" PRId64 " exceeds parent '%s' width %" PRId64,
                      g->sub_hi, g->parent, s->registers[g->parent_index].width);
        }
    }
    for (size_t i = 0; i < s->register_count; i++) {
        size_t steps = 0;
        int32_t p = s->registers[i].parent_index;
        while (p >= 0 && steps <= s->register_count) {
            p = s->registers[p].parent_index;
            steps++;
        }
        if (steps > s->register_count) {
            RPATH(r, "/registers/%zu/parent", i);
            rdiag(r, DSYM_ERROR, "parent chain of '%s' contains a cycle",
                  s->registers[i].name);
        }
    }

    /* encoders: compile bodies */
    if (!(flags & DSYM_LOAD_NO_COMPILE)) {
        for (size_t i = 0; i < s->encoder_count; i++) {
            DsymEncoder *e = &s->encoders[i];
            RPATH(r, "/encoders/%zu/body", i);
            e->program = dsym_program_compile(s->arena, e->body, s->source_path, 0,
                                              r->path, reloc_lookup_cb, ix, diags);
            if (!e->program) continue;
            if (flags & (DSYM_LOAD_LINT | DSYM_LOAD_STRICT)) {
                for (size_t j = 0; j < e->program->stmt_count; j++) {
                    const DsymStmt *st = &e->program->stmts[j];
                    if (!biased_opt_out(st->text) &&
                        (biased_modulo(e->program, st->expr[0]) ||
                         biased_modulo(e->program, st->expr[1])))
                        rdiag(r, (flags & DSYM_LOAD_STRICT) ? DSYM_ERROR : DSYM_WARNING,
                              "DSYM-W001: encoder '%s', statement %u: suspicious biased modulo: %s",
                              e->name, st->line, st->text);
                }
            }
            if (flags & DSYM_LOAD_LINT) {
                size_t field_size = 0;
                for (size_t j = 0; j < e->program->stmt_count; j++) {
                    const DsymStmt *st = &e->program->stmts[j];
                    if (st->kind == DSYM_ST_EMIT || st->kind == DSYM_ST_EMIT_IMM ||
                        st->kind == DSYM_ST_EMIT_MODRM) field_size = st->size;
                    if (st->kind == DSYM_ST_RELOC && st->reloc_index >= 0 &&
                        (size_t)st->reloc_index < s->reloc_count) {
                        const DsymReloc *rel = &s->relocs[st->reloc_index];
                        if (!reloc_fits_field(rel, field_size))
                            rdiag(r, DSYM_WARNING,
                                  "DSYM-W004: encoder '%s', statement %u: relocation '%s' width %"
                                  PRId64 " at bit offset %" PRId64 " does not fit %zu-byte field",
                                  e->name, st->line, rel->name, rel->width,
                                  rel->has_offset ? rel->offset : 0, field_size);
                    }
                }
            }
            RPATH(r, "/encoders/%zu", i);
            if (e->has_max_bytes && e->program->static_size > (uint64_t)e->max_bytes)
                rdiag(r, (flags & DSYM_LOAD_LINT) ? DSYM_WARNING : DSYM_NOTE,
                      "encoder '%s' always emits %u byte(s) but max_bytes is %" PRId64,
                      e->name, e->program->static_size, e->max_bytes);
            if (e->program->reloc_count > (e->has_max_relocs ? (uint64_t)e->max_relocs : 0) &&
                (e->has_max_relocs || (flags & DSYM_LOAD_LINT)))
                rdiag(r, (flags & DSYM_LOAD_LINT) ? DSYM_WARNING : DSYM_NOTE,
                      "encoder '%s' emits %u relocation(s) but max_relocs is %" PRId64,
                      e->name, e->program->reloc_count,
                      e->has_max_relocs ? e->max_relocs : 0);
            if (e->program->static_size > DSYM_ENCODE_MAX_BYTES ||
                e->program->reloc_count > DSYM_ENCODE_MAX_RELOCS)
                rdiag(r, DSYM_ERROR, "encoder '%s' exceeds the encoder limits (%d bytes, %d relocs)",
                      e->name, DSYM_ENCODE_MAX_BYTES, DSYM_ENCODE_MAX_RELOCS);
        }
    }

    /* forms */
    {
        int32_t *cnt = dsym_arena_calloc(s->arena, s->mnemonic_count + 1, sizeof *cnt);
        if (!cnt) goto oom;
        for (size_t i = 0; i < s->form_count; i++) {
            DsymForm *f = &s->forms[i];
            f->mnemonic_index = strmap_get(&ix->mnemonics, f->mnemonic);
            f->encoder_index = strmap_get(&ix->encoders, f->encoder);
            if (f->mnemonic_index < 0) {
                RPATH(r, "/forms/%zu/mnemonic", i);
                rdiag(r, DSYM_ERROR, "unknown mnemonic '%s'", f->mnemonic);
            } else {
                cnt[f->mnemonic_index]++;
            }
            if (f->encoder_index < 0) {
                RPATH(r, "/forms/%zu/encoder", i);
                rdiag(r, DSYM_ERROR, "unknown encoder '%s'", f->encoder);
            }
            if ((flags & DSYM_LOAD_LINT) && f->encoder_index >= 0) {
                const DsymProgram *p = s->encoders[f->encoder_index].program;
                size_t visible = visible_operand_count(f);
                if (p && visible != p->param_count) {
                    RPATH(r, "/forms/%zu/operands", i);
                    rdiag(r, DSYM_WARNING,
                          "DSYM-W002: form %zu (%s): %zu visible operand(s), encoder '%s' has %zu parameter(s)",
                          i, f->mnemonic, visible, f->encoder, p->param_count);
                }
                if (p && visible == p->param_count) {
                    size_t slot = 0;
                    for (size_t k = 0; k < f->operand_count; k++) {
                        const DsymOperand *o = &f->operands[k];
                        if (!operand_visible(o)) continue;
                        bool symbolic = o->kind == DSYM_OK_SYM || o->kind == DSYM_OK_REL;
                        if (symbolic != param_is_relocated(p, slot)) {
                            RPATH(r, "/forms/%zu/operands/%zu", i, k);
                            rdiag(r, DSYM_WARNING,
                                  "DSYM-W003: form %zu operand %zu (%s), parameter '%s': %s",
                                  i, k + 1, dsym_operand_kind_name(o->kind), p->params[slot],
                                  symbolic ? "no EMIT_RELOCATION for symbolic operand" :
                                             "EMIT_RELOCATION requires a sym or rel operand");
                        }
                        slot++;
                    }
                }
            }
            const char *pf = "/forms/%zu/%s/%zu";
            if (!(f->required_index = resolve_list(r, &f->required, &ix->features, "feature", pf, i, "required")) ||
                !(f->forbidden_index = resolve_list(r, &f->forbidden, &ix->features, "feature", pf, i, "forbidden")) ||
                !(f->syntax_index = resolve_list(r, &f->syntaxes, &ix->syntaxes, "syntax", pf, i, "syntaxes")))
                goto oom;
            for (size_t a = 0; a < f->required.count; a++)
                for (size_t b = 0; b < f->forbidden.count; b++)
                    if (strcmp(f->required.items[a], f->forbidden.items[b]) == 0) {
                        RPATH(r, "/forms/%zu/forbidden/%zu", i, b);
                        rdiag(r, DSYM_ERROR, "feature '%s' is both required and forbidden",
                              f->forbidden.items[b]);
                    }
            if (f->has_ops && f->has_operands && (size_t)f->ops != f->operand_count &&
                (flags & DSYM_LOAD_LINT)) {
                RPATH(r, "/forms/%zu/ops", i);
                rdiag(r, DSYM_WARNING, "ops is %" PRId64 " but %zu operand(s) are listed",
                      f->ops, f->operand_count);
            }
            for (size_t k = 0; k < f->operand_count; k++) {
                const DsymOperand *o = &f->operands[k];
                if (o->has_imm_min && o->has_imm_max && o->imm_min > o->imm_max) {
                    RPATH(r, "/forms/%zu/operands/%zu", i, k);
                    rdiag(r, DSYM_ERROR, "imm_min %" PRId64 " exceeds imm_max %" PRId64,
                          o->imm_min, o->imm_max);
                }
            }
        }
        /* CSR index mnemonic -> forms */
        ix->form_start = dsym_arena_calloc(s->arena, s->mnemonic_count + 1, sizeof(int32_t));
        ix->form_list = dsym_arena_calloc(s->arena, s->form_count + 1, sizeof(int32_t));
        if (!ix->form_start || !ix->form_list) goto oom;
        int32_t acc = 0;
        for (size_t m = 0; m < s->mnemonic_count; m++) {
            ix->form_start[m] = acc;
            acc += cnt[m];
            cnt[m] = ix->form_start[m];
        }
        ix->form_start[s->mnemonic_count] = acc;
        for (size_t i = 0; i < s->form_count; i++)
            if (s->forms[i].mnemonic_index >= 0)
                ix->form_list[cnt[s->forms[i].mnemonic_index]++] = (int32_t)i;
    }

    /* sections */
    for (size_t i = 0; i < s->section_count; i++)
        if (s->sections[i].has_align && !is_pow2(s->sections[i].align)) {
            RPATH(r, "/sections/%zu/align", i);
            rdiag(r, DSYM_WARNING, "align %" PRId64 " is not a power of two",
                  s->sections[i].align);
        }

    /* debug references (lint only: free-form) */
    if (flags & DSYM_LOAD_LINT) {
        for (size_t i = 0; i < s->debug_count; i++) {
            const DsymDebug *d = &s->debug[i];
            if (!d->ref || !*d->ref) continue;
            int32_t hit = 0;
            switch (d->kind) {
            case DSYM_DBG_REGISTER: hit = strmap_get(&ix->registers, d->ref); break;
            case DSYM_DBG_FEATURE: hit = strmap_get(&ix->features, d->ref); break;
            case DSYM_DBG_MNEMONIC:
            case DSYM_DBG_FORM: hit = dsym_spec_find_mnemonic_or_alias(s, d->ref); break;
            case DSYM_DBG_ENCODER: hit = strmap_get(&ix->encoders, d->ref); break;
            case DSYM_DBG_SYNTAX: hit = strmap_get(&ix->syntaxes, d->ref); break;
            case DSYM_DBG_SECTION: hit = strmap_get(&ix->sections, d->ref); break;
            case DSYM_DBG_LINKAGE: hit = strmap_get(&ix->linkages, d->ref); break;
            case DSYM_DBG_RELOC: hit = strmap_get(&ix->relocs, d->ref); break;
            default: break;
            }
            if (hit < 0) {
                RPATH(r, "/debug/%zu/ref", i);
                rdiag(r, DSYM_WARNING, "debug ref '%s' does not name a %s", d->ref,
                      DEBUG_KIND_N[d->kind]);
            }
        }
    }

    /* asmtk */
    if (s->asmtk) {
        DsymAsmtk *t = s->asmtk;
        for (size_t i = 0; i < t->encoding_count; i++) {
            DsymAsmEncoding *e = &t->encodings[i];
            e->encoder_index = -1;
            e->implicit_register_index = -1;
            RPATH(r, "/asmtk/encodings/%zu", i);
            if ((e->opcode.present ? 1 : 0) + (e->encoder ? 1 : 0) != 1)
                rdiag(r, DSYM_ERROR, "exactly one of 'opcode' or 'encoder' is required");
            if (e->encoder && (e->encoder_index = strmap_get(&ix->encoders, e->encoder)) < 0)
                rdiag(r, DSYM_ERROR, "unknown encoder '%s'", e->encoder);
            if (e->implicit_register &&
                (e->implicit_register_index = strmap_get(&ix->registers, e->implicit_register)) < 0)
                rdiag(r, DSYM_ERROR, "unknown implicit register '%s'", e->implicit_register);
            if (strmap_get(&ix->mnemonics, e->mnemonic) < 0 && (flags & DSYM_LOAD_LINT))
                rdiag(r, DSYM_NOTE, "asmtk mnemonic '%s' is not in mnemonics", e->mnemonic);
        }
        uint32_t seen = 0;
        for (size_t i = 0; i < t->condition_count; i++) {
            int64_t c = t->conditions[i].code;
            if (seen & (1u << c)) {
                RPATH(r, "/asmtk/conditions/%zu/code", i);
                rdiag(r, DSYM_ERROR, "duplicate condition code %" PRId64, c);
            }
            seen |= 1u << c;
        }
    }

    /* metachain: registers and mnemonics must exist */
    if (s->metachain) {
        DsymMetachain *m = s->metachain;
        const char *lr[] = {m->lowering->stack_pointer, m->lowering->accumulator,
                            m->lowering->scratch};
        const char *lk[] = {"stack_pointer", "accumulator", "scratch"};
        for (size_t k = 0; k < 3; k++)
            if (strmap_get(&ix->registers, lr[k]) < 0) {
                RPATH(r, "/metachain/lowering/%s", lk[k]);
                rdiag(r, DSYM_ERROR, "unknown register '%s'", lr[k]);
            }
        const char *lm[] = {m->lowering->copy, m->lowering->ret};
        const char *lmk[] = {"copy", "return"};
        for (size_t k = 0; k < 2; k++)
            if (dsym_spec_find_mnemonic_or_alias(s, lm[k]) < 0) {
                RPATH(r, "/metachain/lowering/%s", lmk[k]);
                rdiag(r, DSYM_WARNING, "unknown mnemonic '%s'", lm[k]);
            }
        const DsymStrList *al[] = {&m->allocation->reserved, &m->allocation->callee_saved,
                                   &m->allocation->arguments, &m->allocation->results};
        const char *ak[] = {"reserved", "callee_saved", "arguments", "results"};
        for (size_t k = 0; k < 4; k++)
            for (size_t j = 0; j < al[k]->count; j++)
                if (strmap_get(&ix->registers, al[k]->items[j]) < 0) {
                    RPATH(r, "/metachain/allocation/%s/%zu", ak[k], j);
                    rdiag(r, DSYM_ERROR, "unknown register '%s'", al[k]->items[j]);
                }
        if (!is_pow2(m->allocation->stack_alignment)) {
            RPATH(r, "/metachain/allocation/stack_alignment");
            rdiag(r, DSYM_WARNING, "stack_alignment is not a power of two");
        }
        for (size_t i = 0; i < m->value_count; i++)
            if (dsym_spec_find_mnemonic_or_alias(s, m->values[i].mnemonic) < 0) {
                RPATH(r, "/metachain/values/%zu/mnemonic", i);
                rdiag(r, DSYM_WARNING, "unknown mnemonic '%s'", m->values[i].mnemonic);
            }
    }

    {
        bool ok = diags->errors == R.errors_before;
        if (own) dsym_diags_free(&local);
        return ok;
    }
oom:
    dsym_diag_add(diags, DSYM_ERROR, s->source_path, 0, 0, NULL, "out of memory");
    if (own) dsym_diags_free(&local);
    return false;
}

/* ======================================================================== */
/* Encoder compiler                                                         */
/* ======================================================================== */

typedef enum TokKind {
    TK_END = 0,
    TK_NUM,
    TK_IDENT,
    TK_OP, /* operator text in tok.op */
    TK_LPAREN,
    TK_RPAREN,
    TK_COMMA,
    TK_BAD
} TokKind;

typedef struct Tok {
    TokKind kind;
    const char *start;
    size_t len;
    uint64_t num;
    char op[3];
} Tok;

typedef struct Lexer {
    const char *p, *end, *line_start;
    Tok tok;
} Lexer;

static void lex_next(Lexer *L) {
    while (L->p < L->end && (*L->p == ' ' || *L->p == '\t' || *L->p == '\r')) L->p++;
    Tok *t = &L->tok;
    memset(t, 0, sizeof *t);
    t->start = L->p;
    if (L->p >= L->end || *L->p == '#') {
        t->kind = TK_END;
        return;
    }
    char c = *L->p;
    if (isdigit((unsigned char)c)) {
        int base = 10;
        const char *q = L->p;
        if (c == '0' && q + 1 < L->end) {
            char x = (char)tolower((unsigned char)q[1]);
            if (x == 'x') base = 16, q += 2;
            else if (x == 'b') base = 2, q += 2;
            else if (x == 'o') base = 8, q += 2;
        }
        uint64_t v = 0;
        bool any = false, ovf = false;
        while (q < L->end) {
            int d;
            char ch = *q;
            if (ch == '_') { q++; continue; }
            if (isdigit((unsigned char)ch)) d = ch - '0';
            else if (isalpha((unsigned char)ch)) d = tolower((unsigned char)ch) - 'a' + 10;
            else break;
            if (d >= base) { t->kind = TK_BAD; L->p = q; t->len = (size_t)(q - t->start); return; }
            if (v > (UINT64_MAX - (uint64_t)d) / (uint64_t)base) ovf = true;
            v = v * (uint64_t)base + (uint64_t)d;
            any = true;
            q++;
        }
        t->kind = (any && !ovf) ? TK_NUM : TK_BAD;
        t->num = v;
        L->p = q;
        t->len = (size_t)(q - t->start);
        return;
    }
    if (isalpha((unsigned char)c) || c == '_') {
        const char *q = L->p;
        while (q < L->end && (isalnum((unsigned char)*q) || *q == '_')) q++;
        t->kind = TK_IDENT;
        t->len = (size_t)(q - L->p);
        L->p = q;
        return;
    }
    L->p++;
    t->len = 1;
    switch (c) {
    case '(': t->kind = TK_LPAREN; return;
    case ')': t->kind = TK_RPAREN; return;
    case ',': t->kind = TK_COMMA; return;
    default: break;
    }
    static const char *two[] = {"<<", ">>", "<=", ">=", "==", "!=", "&&", "||"};
    if (L->p < L->end)
        for (size_t i = 0; i < ARRLEN(two); i++)
            if (two[i][0] == c && two[i][1] == *L->p) {
                t->kind = TK_OP;
                t->op[0] = c;
                t->op[1] = *L->p++;
                t->len = 2;
                return;
            }
    if (strchr("+-*/%<>&|^~!", c)) {
        t->kind = TK_OP;
        t->op[0] = c;
        return;
    }
    t->kind = TK_BAD;
}

typedef struct Compiler {
    DsymArena *arena;
    DsymDiags *diags;
    const char *file;
    const char *path;
    uint32_t line;
    Lexer L;
    DsymInsn *code;
    size_t ncode, capcode;
    const char **params;
    size_t nparams, capparams;
    int depth, max_depth;
    bool err;
} Compiler;

#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
static void cerr(Compiler *c, const char *fmt, ...) {
    if (c->err) return; /* one error per statement */
    char msg[384];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    uint32_t col = (uint32_t)(c->L.tok.start - c->L.line_start) + 1;
    dsym_diag_add(c->diags, DSYM_ERROR, c->file, 0, 0, c->path,
                  "body line %u, column %u: %s", c->line, col, msg);
    c->err = true;
}

static bool emit_insn(Compiler *c, DsymOp op, int64_t imm) {
    DsymInsn *p = grow(c->arena, c->code, c->ncode, &c->capcode, sizeof *p);
    if (!p) return false;
    c->code = p;
    p[c->ncode].op = op;
    p[c->ncode].imm = imm;
    c->ncode++;
    return true;
}

/* Pure evaluation of one operator (shared by folding and the VM). */
static bool apply_op(DsymOp op, int64_t a, int64_t b, int64_t *out,
                     const char **err) {
    uint64_t ua = (uint64_t)a, ub = (uint64_t)b;
    switch (op) {
    case DSYM_OP_NEG: *out = (int64_t)(0 - ua); return true;
    case DSYM_OP_BNOT: *out = (int64_t)~ua; return true;
    case DSYM_OP_LNOT: *out = !a; return true;
    case DSYM_OP_MUL: *out = (int64_t)(ua * ub); return true;
    case DSYM_OP_DIV:
    case DSYM_OP_MOD:
        if (b == 0) {
            *err = "division by zero";
            return false;
        }
        if (a == INT64_MIN && b == -1) {
            *out = op == DSYM_OP_DIV ? INT64_MIN : 0;
            return true;
        }
        if (op == DSYM_OP_DIV) {
            *out = a / b;
        } else {
            int64_t m = a % b;
            if (m < 0) m += b < 0 ? -b : b; /* Euclidean */
            *out = m;
        }
        return true;
    case DSYM_OP_ADD: *out = (int64_t)(ua + ub); return true;
    case DSYM_OP_SUB: *out = (int64_t)(ua - ub); return true;
    case DSYM_OP_SHL: *out = (b < 0 || b > 63) ? 0 : (int64_t)(ua << b); return true;
    case DSYM_OP_SHR:
        *out = (b < 0 || b > 63) ? (a < 0 ? -1 : 0) : (a >> b);
        return true;
    case DSYM_OP_LT: *out = a < b; return true;
    case DSYM_OP_LE: *out = a <= b; return true;
    case DSYM_OP_GT: *out = a > b; return true;
    case DSYM_OP_GE: *out = a >= b; return true;
    case DSYM_OP_EQ: *out = a == b; return true;
    case DSYM_OP_NE: *out = a != b; return true;
    case DSYM_OP_BAND: *out = (int64_t)(ua & ub); return true;
    case DSYM_OP_BXOR: *out = (int64_t)(ua ^ ub); return true;
    case DSYM_OP_BOR: *out = (int64_t)(ua | ub); return true;
    case DSYM_OP_LAND: *out = a && b; return true;
    case DSYM_OP_LOR: *out = a || b; return true;
    default: *err = "bad opcode"; return false;
    }
}

static void emit_unary(Compiler *c, DsymOp op, size_t expr_start) {
    if (c->ncode == expr_start + 1 && c->code[c->ncode - 1].op == DSYM_OP_CONST) {
        int64_t r;
        const char *e;
        if (apply_op(op, c->code[c->ncode - 1].imm, 0, &r, &e)) {
            c->code[c->ncode - 1].imm = r;
            return;
        }
    }
    emit_insn(c, op, 0);
}

/* The right operand ends at ncode-1; if it and the left operand are both a
   single CONST they are exactly the last two instructions (a compound
   subexpression always ends with an operator). */
static void emit_binary(Compiler *c, DsymOp op, size_t lhs_start) {
    c->depth--;
    if (c->ncode >= lhs_start + 2 && c->ncode - lhs_start == 2 &&
        c->code[c->ncode - 1].op == DSYM_OP_CONST &&
        c->code[c->ncode - 2].op == DSYM_OP_CONST) {
        int64_t r;
        const char *e;
        if (apply_op(op, c->code[c->ncode - 2].imm, c->code[c->ncode - 1].imm, &r, &e)) {
            c->ncode--;
            c->code[c->ncode - 1].imm = r;
            return;
        }
    }
    emit_insn(c, op, 0);
}

static int param_slot(Compiler *c, const char *s, size_t len) {
    for (size_t i = 0; i < c->nparams; i++)
        if (strlen(c->params[i]) == len && memcmp(c->params[i], s, len) == 0)
            return (int)i;
    const char **p = grow(c->arena, (void *)c->params, c->nparams, &c->capparams, sizeof *p);
    if (!p) return -1;
    c->params = p;
    p[c->nparams] = dsym_arena_strndup(c->arena, s, len);
    return (int)c->nparams++;
}

typedef struct BinOp {
    const char *text;
    int prec;
    DsymOp op;
} BinOp;

static const BinOp BINOPS[] = {
    {"||", 1, DSYM_OP_LOR}, {"&&", 2, DSYM_OP_LAND}, {"|", 3, DSYM_OP_BOR},
    {"^", 4, DSYM_OP_BXOR}, {"&", 5, DSYM_OP_BAND},  {"==", 6, DSYM_OP_EQ},
    {"!=", 6, DSYM_OP_NE},  {"<", 7, DSYM_OP_LT},    {"<=", 7, DSYM_OP_LE},
    {">", 7, DSYM_OP_GT},   {">=", 7, DSYM_OP_GE},   {"<<", 8, DSYM_OP_SHL},
    {">>", 8, DSYM_OP_SHR}, {"+", 9, DSYM_OP_ADD},   {"-", 9, DSYM_OP_SUB},
    {"*", 10, DSYM_OP_MUL}, {"/", 10, DSYM_OP_DIV},  {"%", 10, DSYM_OP_MOD},
};

static const BinOp *peek_binop(Compiler *c) {
    if (c->L.tok.kind != TK_OP) return NULL;
    for (size_t i = 0; i < ARRLEN(BINOPS); i++)
        if (strcmp(BINOPS[i].text, c->L.tok.op) == 0) return &BINOPS[i];
    return NULL;
}

static void parse_expr(Compiler *c, int min_prec);

static void parse_primary(Compiler *c) {
    size_t start = c->ncode;
    Tok t = c->L.tok;
    switch (t.kind) {
    case TK_NUM:
        emit_insn(c, DSYM_OP_CONST, (int64_t)t.num);
        if (++c->depth > c->max_depth) c->max_depth = c->depth;
        lex_next(&c->L);
        return;
    case TK_IDENT: {
        int slot = param_slot(c, t.start, t.len);
        if (slot < 0) {
            cerr(c, "out of memory");
            return;
        }
        emit_insn(c, DSYM_OP_PARAM, slot);
        if (++c->depth > c->max_depth) c->max_depth = c->depth;
        lex_next(&c->L);
        return;
    }
    case TK_LPAREN:
        lex_next(&c->L);
        parse_expr(c, 1);
        if (c->L.tok.kind != TK_RPAREN) {
            cerr(c, "expected ')'");
            return;
        }
        lex_next(&c->L);
        return;
    case TK_OP:
        if (t.op[1] == 0 && (t.op[0] == '-' || t.op[0] == '~' || t.op[0] == '!' ||
                             t.op[0] == '+')) {
            lex_next(&c->L);
            parse_primary(c);
            if (c->err) return;
            if (t.op[0] == '-') emit_unary(c, DSYM_OP_NEG, start);
            else if (t.op[0] == '~') emit_unary(c, DSYM_OP_BNOT, start);
            else if (t.op[0] == '!') emit_unary(c, DSYM_OP_LNOT, start);
            return;
        }
        cerr(c, "unexpected operator '%s'", t.op);
        return;
    case TK_END: cerr(c, "expected an expression"); return;
    case TK_BAD: cerr(c, "invalid token '%.*s'", (int)(t.len ? t.len : 1), t.start); return;
    default: cerr(c, "unexpected '%.*s'", (int)t.len, t.start); return;
    }
}

static void parse_expr(Compiler *c, int min_prec) {
    size_t lhs = c->ncode;
    parse_primary(c);
    for (;;) {
        if (c->err) return;
        const BinOp *b = peek_binop(c);
        if (!b || b->prec < min_prec) return;
        lex_next(&c->L);
        parse_expr(c, b->prec + 1); /* all binary operators are left-assoc */
        if (c->err) return;
        emit_binary(c, b->op, lhs);
    }
}

static DsymExprRef compile_expr(Compiler *c) {
    DsymExprRef r = {(uint32_t)c->ncode, 0};
    c->depth = 0;
    parse_expr(c, 1);
    r.len = (uint32_t)(c->ncode - r.start);
    return r;
}

static bool expect_comma(Compiler *c) {
    if (c->L.tok.kind != TK_COMMA) {
        cerr(c, "expected ','");
        return false;
    }
    lex_next(&c->L);
    return true;
}

typedef struct StmtSpec {
    const char *name;
    DsymStmtKind kind;
    uint8_t size;
    DsymEndian endian;
} StmtSpec;

static const StmtSpec STMTS[] = {
    {"EMIT8", DSYM_ST_EMIT, 1, DSYM_ENDIAN_LITTLE},
    {"EMIT16LE", DSYM_ST_EMIT, 2, DSYM_ENDIAN_LITTLE},
    {"EMIT16BE", DSYM_ST_EMIT, 2, DSYM_ENDIAN_BIG},
    {"EMIT32LE", DSYM_ST_EMIT, 4, DSYM_ENDIAN_LITTLE},
    {"EMIT32BE", DSYM_ST_EMIT, 4, DSYM_ENDIAN_BIG},
    {"EMIT64LE", DSYM_ST_EMIT, 8, DSYM_ENDIAN_LITTLE},
    {"EMIT64BE", DSYM_ST_EMIT, 8, DSYM_ENDIAN_BIG},
    {"EMIT_IMM", DSYM_ST_EMIT_IMM, 0, DSYM_ENDIAN_TARGET},
    {"EMIT_MODRM", DSYM_ST_EMIT_MODRM, 1, DSYM_ENDIAN_LITTLE},
    {"EMIT_RELOCATION", DSYM_ST_RELOC, 0, DSYM_ENDIAN_LITTLE},
    {"REQUIRE", DSYM_ST_REQUIRE, 0, DSYM_ENDIAN_LITTLE},
};

/* Evaluate a constant expression at compile time (EMIT_IMM width). */
static bool const_value(Compiler *c, DsymExprRef e, int64_t *v) {
    if (e.len == 1 && c->code[e.start].op == DSYM_OP_CONST) {
        *v = c->code[e.start].imm;
        return true;
    }
    return false;
}

const DsymProgram *dsym_program_compile(DsymArena *a, const char *body,
                                        const char *file, uint32_t base_line,
                                        const char *path, DsymRelocLookup lookup,
                                        void *ctx, DsymDiags *diags) {
    Compiler C = {0};
    Compiler *c = &C;
    c->arena = a;
    c->diags = diags;
    c->file = file;
    c->path = path;
    DsymStmt *stmts = NULL;
    size_t nst = 0, capst = 0;
    bool any_err = false, have_field = false;
    uint32_t static_size = 0, nrel = 0;
    int max_stack = 0;
    (void)base_line;

    const char *p = body ? body : "";
    uint32_t line = 0;
    while (*p) {
        const char *eol = strchr(p, '\n');
        if (!eol) eol = p + strlen(p);
        line++;
        c->line = line;
        c->err = false;
        c->L.p = p;
        c->L.end = eol;
        c->L.line_start = p;
        lex_next(&c->L);
        if (c->L.tok.kind == TK_END) { /* blank or comment line */
            p = *eol ? eol + 1 : eol;
            continue;
        }
        if (c->L.tok.kind != TK_IDENT) {
            cerr(c, "expected a statement keyword");
            any_err = true;
            p = *eol ? eol + 1 : eol;
            continue;
        }
        const StmtSpec *sp = NULL;
        for (size_t i = 0; i < ARRLEN(STMTS); i++)
            if (strlen(STMTS[i].name) == c->L.tok.len &&
                memcmp(STMTS[i].name, c->L.tok.start, c->L.tok.len) == 0)
                sp = &STMTS[i];
        if (!sp) {
            cerr(c, "unknown statement '%.*s'", (int)c->L.tok.len, c->L.tok.start);
            any_err = true;
            p = *eol ? eol + 1 : eol;
            continue;
        }
        lex_next(&c->L);
        DsymStmt st;
        memset(&st, 0, sizeof st);
        st.kind = sp->kind;
        st.size = sp->size;
        st.endian = sp->endian;
        st.line = line;
        st.reloc_index = -1;
        {
            const char *ts = p, *te = eol;
            while (ts < te && (*ts == ' ' || *ts == '\t')) ts++;
            while (te > ts && (te[-1] == ' ' || te[-1] == '\t' || te[-1] == '\r')) te--;
            st.text = dsym_arena_strndup(a, ts, (size_t)(te - ts));
        }
        switch (sp->kind) {
        case DSYM_ST_EMIT:
        case DSYM_ST_REQUIRE:
            st.expr[0] = compile_expr(c);
            break;
        case DSYM_ST_EMIT_IMM: {
            DsymExprRef w = compile_expr(c);
            int64_t wv = 0;
            if (!c->err && !const_value(c, w, &wv))
                cerr(c, "EMIT_IMM width must be a constant");
            else if (!c->err && wv != 1 && wv != 2 && wv != 4 && wv != 8)
                cerr(c, "EMIT_IMM width must be 1, 2, 4 or 8 (got %" PRId64 ")", wv);
            c->ncode = w.start; /* drop the width expression */
            st.size = (uint8_t)wv;
            if (!c->err && expect_comma(c)) st.expr[0] = compile_expr(c);
            break;
        }
        case DSYM_ST_EMIT_MODRM:
            st.expr[0] = compile_expr(c);
            if (!c->err && expect_comma(c)) st.expr[1] = compile_expr(c);
            break;
        case DSYM_ST_RELOC: {
            if (c->L.tok.kind != TK_IDENT) {
                cerr(c, "EMIT_RELOCATION expects an operand name");
                break;
            }
            int slot = param_slot(c, c->L.tok.start, c->L.tok.len);
            st.sym_param = slot < 0 ? 0 : (uint32_t)slot;
            lex_next(&c->L);
            if (!expect_comma(c)) break;
            if (c->L.tok.kind != TK_IDENT) {
                cerr(c, "EMIT_RELOCATION expects a relocation name");
                break;
            }
            st.reloc_name = dsym_arena_strndup(a, c->L.tok.start, c->L.tok.len);
            if (lookup) {
                st.reloc_index = lookup(ctx, st.reloc_name);
                if (st.reloc_index < 0)
                    cerr(c, "unknown relocation '%s'", st.reloc_name);
            }
            lex_next(&c->L);
            if (!have_field && !c->err)
                cerr(c, "EMIT_RELOCATION must follow an emitted field");
            nrel++;
            break;
        }
        }
        if (!c->err && c->L.tok.kind != TK_END)
            cerr(c, "unexpected '%.*s' after statement", (int)(c->L.tok.len ? c->L.tok.len : 1),
                 c->L.tok.start);
        if (c->err) {
            any_err = true;
        } else {
            if (c->max_depth > max_stack) max_stack = c->max_depth;
            if (st.kind == DSYM_ST_EMIT || st.kind == DSYM_ST_EMIT_IMM ||
                st.kind == DSYM_ST_EMIT_MODRM) {
                static_size += st.size;
                have_field = true;
            }
            DsymStmt *ns = grow(a, stmts, nst, &capst, sizeof *ns);
            if (!ns) return NULL;
            stmts = ns;
            stmts[nst++] = st;
        }
        p = *eol ? eol + 1 : eol;
    }
    if (any_err) return NULL;
    if (nst == 0) {
        dsym_diag_add(diags, DSYM_ERROR, file, 0, 0, path, "encoder body has no statements");
        return NULL;
    }
    DsymProgram *prog = dsym_arena_alloc(a, sizeof *prog);
    if (!prog) return NULL;
    prog->stmts = stmts;
    prog->stmt_count = nst;
    prog->code = c->code;
    prog->code_count = c->ncode;
    prog->params = c->params;
    prog->param_count = c->nparams;
    prog->static_size = static_size;
    prog->reloc_count = nrel;
    prog->max_stack = (uint32_t)(max_stack < 1 ? 1 : max_stack);
    return prog;
}

int dsym_program_param_index(const DsymProgram *p, const char *name) {
    if (!p || !name) return -1;
    for (size_t i = 0; i < p->param_count; i++)
        if (strcmp(p->params[i], name) == 0) return (int)i;
    return -1;
}

#define VM_STACK 64

bool dsym_expr_eval(const DsymProgram *p, DsymExprRef e, const int64_t *args,
                    int64_t *result, const char **error) {
    int64_t st[VM_STACK];
    int sp = 0;
    const char *err = NULL;
    if (!error) error = &err;
    for (uint32_t i = 0; i < e.len; i++) {
        const DsymInsn *in = &p->code[e.start + i];
        switch (in->op) {
        case DSYM_OP_CONST:
        case DSYM_OP_PARAM:
            if (sp >= VM_STACK) {
                *error = "expression too deep";
                return false;
            }
            if (in->op == DSYM_OP_PARAM && (in->imm < 0 || (size_t)in->imm >= p->param_count)) {
                *error = "bad parameter slot";
                return false;
            }
            st[sp++] = in->op == DSYM_OP_CONST ? in->imm : args[in->imm];
            break;
        case DSYM_OP_NEG:
        case DSYM_OP_BNOT:
        case DSYM_OP_LNOT:
            if (sp < 1) {
                *error = "stack underflow";
                return false;
            }
            if (!apply_op(in->op, st[sp - 1], 0, &st[sp - 1], error)) return false;
            break;
        default:
            if (sp < 2) {
                *error = "stack underflow";
                return false;
            }
            if (!apply_op(in->op, st[sp - 2], st[sp - 1], &st[sp - 2], error)) return false;
            sp--;
            break;
        }
    }
    if (sp != 1) {
        *error = "malformed expression";
        return false;
    }
    *result = st[0];
    return true;
}

const char *dsym_op_name(DsymOp op) {
    static const char *n[] = {"const", "param", "neg", "bnot", "lnot", "mul",
                              "div",   "mod",   "add", "sub",  "shl",  "shr",
                              "lt",    "le",    "gt",  "ge",   "eq",   "ne",
                              "band",  "bxor",  "bor", "land", "lor"};
    return (unsigned)op < ARRLEN(n) ? n[op] : "?";
}

static void dump_expr(const DsymProgram *p, DsymExprRef e, DsymBuf *out) {
    dsym_buf_putc(out, '[');
    for (uint32_t i = 0; i < e.len; i++) {
        const DsymInsn *in = &p->code[e.start + i];
        if (i) dsym_buf_putc(out, ' ');
        if (in->op == DSYM_OP_CONST)
            dsym_buf_printf(out, "%" PRId64, in->imm);
        else if (in->op == DSYM_OP_PARAM)
            dsym_buf_printf(out, "$%s", p->params[in->imm]);
        else
            dsym_buf_puts(out, dsym_op_name(in->op));
    }
    dsym_buf_putc(out, ']');
}

void dsym_program_dump(const DsymProgram *p, DsymBuf *out) {
    if (!p) return;
    dsym_buf_printf(out, "; %zu stmt(s), %u byte(s), %u reloc(s), stack %u, params:",
                    p->stmt_count, p->static_size, p->reloc_count, p->max_stack);
    for (size_t i = 0; i < p->param_count; i++) dsym_buf_printf(out, " %s", p->params[i]);
    dsym_buf_putc(out, '\n');
    static const char *endn[] = {"le", "be", "target"};
    for (size_t i = 0; i < p->stmt_count; i++) {
        const DsymStmt *s = &p->stmts[i];
        dsym_buf_printf(out, "%4u  ", s->line);
        switch (s->kind) {
        case DSYM_ST_EMIT:
            dsym_buf_printf(out, "emit.%u.%s ", s->size, endn[s->endian]);
            dump_expr(p, s->expr[0], out);
            break;
        case DSYM_ST_EMIT_IMM:
            dsym_buf_printf(out, "emit.%u.%s ", s->size, endn[s->endian]);
            dump_expr(p, s->expr[0], out);
            break;
        case DSYM_ST_EMIT_MODRM:
            dsym_buf_puts(out, "modrm reg=");
            dump_expr(p, s->expr[0], out);
            dsym_buf_puts(out, " rm=");
            dump_expr(p, s->expr[1], out);
            break;
        case DSYM_ST_RELOC:
            dsym_buf_printf(out, "reloc %s sym=$%s", s->reloc_name, p->params[s->sym_param]);
            break;
        case DSYM_ST_REQUIRE:
            dsym_buf_puts(out, "require ");
            dump_expr(p, s->expr[0], out);
            break;
        }
        dsym_buf_printf(out, "    ; %s\n", s->text ? s->text : "");
    }
}

/* ======================================================================== */
/* Encoding                                                                 */
/* ======================================================================== */

#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
static bool enc_fail(DsymEncoded *out, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out->error, sizeof out->error, fmt, ap);
    va_end(ap);
    return false;
}

static bool fits(int64_t v, unsigned bytes) {
    if (bytes >= 8) return true;
    int64_t lo = -((int64_t)1 << (bytes * 8 - 1));
    int64_t hi = ((int64_t)1 << (bytes * 8)) - 1;
    return v >= lo && v <= hi;
}

bool dsym_encode(const DsymSpec *s, int32_t ei, size_t ti, const int64_t *args,
                  size_t argc, DsymEncoded *out) {
    if (!out) return false;
    memset(out, 0, sizeof *out);
    if (argc && !args) return enc_fail(out, "operand array is NULL");
    if (!s || ei < 0 || (size_t)ei >= s->encoder_count)
        return enc_fail(out, "no such encoder");
    const DsymEncoder *e = &s->encoders[ei];
    const DsymProgram *p = e->program;
    if (!p) return enc_fail(out, "encoder '%s' is not compiled", e->name);
    if (argc < p->param_count)
        return enc_fail(out, "encoder '%s' needs %zu operand(s), got %zu", e->name,
                        p->param_count, argc);
    bool target_be = ti < s->target_count && dsym_target_is_big_endian(&s->targets[ti]);
    size_t last_off = 0, last_size = 0;
    DsymEndian last_endian = DSYM_ENDIAN_LITTLE;
    for (size_t i = 0; i < p->stmt_count; i++) {
        const DsymStmt *st = &p->stmts[i];
        int64_t v = 0, v2 = 0;
        const char *err = NULL;
        switch (st->kind) {
        case DSYM_ST_REQUIRE:
            if (!dsym_expr_eval(p, st->expr[0], args, &v, &err))
                return enc_fail(out, "%s: line %u: %s", e->name, st->line, err);
            if (!v) return enc_fail(out, "%s: constraint failed: %s", e->name, st->text);
            break;
        case DSYM_ST_EMIT:
        case DSYM_ST_EMIT_IMM: {
            if (!dsym_expr_eval(p, st->expr[0], args, &v, &err))
                return enc_fail(out, "%s: line %u: %s", e->name, st->line, err);
            if (!fits(v, st->size))
                return enc_fail(out, "%s: value %" PRId64 " does not fit in %u byte(s) (%s)",
                                e->name, v, st->size, st->text);
            if (out->size + st->size > DSYM_ENCODE_MAX_BYTES)
                return enc_fail(out, "%s: output too long", e->name);
            bool be = st->endian == DSYM_ENDIAN_BIG ||
                      (st->endian == DSYM_ENDIAN_TARGET && target_be);
            uint64_t u = (uint64_t)v;
            for (unsigned k = 0; k < st->size; k++) {
                unsigned shift = be ? (st->size - 1 - k) * 8 : k * 8;
                out->bytes[out->size + k] = (uint8_t)(u >> shift);
            }
            last_off = out->size;
            last_size = st->size;
            last_endian = be ? DSYM_ENDIAN_BIG : DSYM_ENDIAN_LITTLE;
            out->size += st->size;
            break;
        }
        case DSYM_ST_EMIT_MODRM:
            if (!dsym_expr_eval(p, st->expr[0], args, &v, &err) ||
                !dsym_expr_eval(p, st->expr[1], args, &v2, &err))
                return enc_fail(out, "%s: line %u: %s", e->name, st->line, err);
            if (v < 0 || v2 < 0)
                return enc_fail(out, "%s: negative ModR/M field", e->name);
            if (out->size + 1 > DSYM_ENCODE_MAX_BYTES)
                return enc_fail(out, "%s: output too long", e->name);
            last_off = out->size;
            last_size = 1;
            last_endian = DSYM_ENDIAN_LITTLE;
            out->bytes[out->size++] = (uint8_t)(0xC0 | ((v & 7) << 3) | (v2 & 7));
            break;
        case DSYM_ST_RELOC: {
            if (out->reloc_count >= DSYM_ENCODE_MAX_RELOCS)
                return enc_fail(out, "%s: too many relocations", e->name);
            if (st->reloc_index < 0 || (size_t)st->reloc_index >= s->reloc_count)
                return enc_fail(out, "%s: unresolved relocation '%s'", e->name, st->reloc_name);
            const DsymReloc *rel = &s->relocs[st->reloc_index];
            if (!reloc_fits_field(rel, last_size))
                return enc_fail(out, "%s: relocation '%s' does not fit %zu-byte field",
                                e->name, rel->name, last_size);
            DsymEncodedReloc *r = &out->relocs[out->reloc_count++];
            r->offset = (uint32_t)last_off;
            r->size = (uint32_t)last_size;
            r->reloc_index = st->reloc_index;
            r->reloc_name = st->reloc_name;
            r->sym_param = st->sym_param;
            r->sym_value = args[st->sym_param];
            r->bit_offset = (uint32_t)(rel->has_offset ? rel->offset : 0);
            r->bit_width = (uint32_t)rel->width;
            r->endian = last_endian;
            break;
        }
        }
    }
    return true;
}

static bool operand_value_valid(const DsymSpec *s, const DsymOperand *o,
                                size_t position, int64_t value, DsymEncoded *out) {
    const char *kind = dsym_operand_kind_name(o->kind);
    if (o->has_imm_min && value < o->imm_min)
        return enc_fail(out, "operand %zu (%s): value %" PRId64 " below minimum %" PRId64,
                        position, kind, value, o->imm_min);
    if (o->has_imm_max && value > o->imm_max)
        return enc_fail(out, "operand %zu (%s): value %" PRId64 " above maximum %" PRId64,
                        position, kind, value, o->imm_max);
    if (o->kind == DSYM_OK_REG) {
        for (size_t i = 0; i < s->register_count; i++) {
            const DsymRegister *r = &s->registers[i];
            if (r->enc == value && (!o->has_reg_class || r->reg_class == o->reg_class) &&
                (!o->has_width || r->width == o->width)) return true;
        }
        return enc_fail(out, "operand %zu (reg): encoding %" PRId64
                        " has no register matching class %s and width %" PRId64,
                        position, value,
                        o->has_reg_class ? dsym_reg_class_name(o->reg_class) : "any",
                        o->has_width ? o->width : 0);
    }
    /* Widths >= 64 never require a shift by 64 or a narrowing conversion.
       The int64_t API cannot represent the unsigned upper half of uint64_t. */
    bool is_signed = o->has_signed && o->is_signed;
    bool fits_width = true;
    if (o->has_width) {
        if (o->width < 1) fits_width = false;
        else if (is_signed && o->width < 64) {
            int64_t bound = INT64_C(1) << (o->width - 1);
            fits_width = value >= -bound && value < bound;
        } else if (!is_signed) {
            fits_width = value >= 0 && (o->width >= 63 ||
                         (uint64_t)value < (UINT64_C(1) << o->width));
        }
    } else if (o->has_signed && !o->is_signed) {
        fits_width = value >= 0;
    }
    if (!fits_width)
        return enc_fail(out, "operand %zu (%s): value %" PRId64 " outside %s width %" PRId64,
                        position, kind, value, is_signed ? "signed" : "unsigned", o->width);
    return true;
}

bool dsym_encode_form(const DsymSpec *s, size_t fi, size_t ti, const int64_t *args,
                      size_t n, DsymEncoded *out) {
    if (!out) return false;
    memset(out, 0, sizeof *out);
    if (!s || fi >= s->form_count) return enc_fail(out, "no such form");
    if (ti >= s->target_count) return enc_fail(out, "no such target");
    const DsymForm *f = &s->forms[fi];
    if (f->encoder_index < 0 || (size_t)f->encoder_index >= s->encoder_count)
        return enc_fail(out, "form %zu: no resolved encoder", fi);
    const DsymProgram *p = s->encoders[f->encoder_index].program;
    if (!p) return enc_fail(out, "form %zu: encoder is not compiled", fi);
    size_t visible = visible_operand_count(f);
    if (visible != p->param_count)
        return enc_fail(out, "form %zu: %zu visible operands cannot map to %zu encoder parameters",
                        fi, visible, p->param_count);
    if (n > visible)
        return enc_fail(out, "form %zu: expected %zu visible operand(s), got %zu", fi, visible, n);
    if (n && !args) return enc_fail(out, "operand array is NULL");
    size_t slot = 0;
    for (size_t i = 0; i < f->operand_count; i++) {
        const DsymOperand *o = &f->operands[i];
        if (!operand_visible(o)) continue;
        if (slot >= n)
            return enc_fail(out, "operand %zu (%s): missing argument", i + 1,
                            dsym_operand_kind_name(o->kind));
        bool symbolic = o->kind == DSYM_OK_SYM || o->kind == DSYM_OK_REL;
        if (symbolic != param_is_relocated(p, slot))
            return enc_fail(out, "operand %zu (%s): relocation parameter mismatch for '%s'",
                            i + 1, dsym_operand_kind_name(o->kind), p->params[slot]);
        if (!operand_value_valid(s, o, i + 1, args[slot], out)) return false;
        slot++;
    }
    return dsym_encode(s, f->encoder_index, ti, args, n, out);
}

bool dsym_encode_named(const DsymSpec *s, const char *encoder, size_t ti,
                       const char *const *names, const int64_t *values, size_t n,
                        bool allow_unbound, DsymEncoded *out) {
    if (!out) return false;
    if (n && (!names || !values)) {
        memset(out, 0, sizeof *out);
        return enc_fail(out, "operand names or values array is NULL");
    }
    int32_t ei = dsym_spec_find_encoder(s, encoder);
    if (ei < 0) {
        memset(out, 0, sizeof *out);
        return enc_fail(out, "unknown encoder '%s'", encoder ? encoder : "");
    }
    const DsymProgram *p = s->encoders[ei].program;
    if (!p) {
        memset(out, 0, sizeof *out);
        return enc_fail(out, "encoder '%s' is not compiled", encoder);
    }
    int64_t args[64] = {0};
    bool bound[64] = {0};
    if (p->param_count > 64) {
        memset(out, 0, sizeof *out);
        return enc_fail(out, "too many operands");
    }
    for (size_t i = 0; i < n; i++) {
        int k = dsym_program_param_index(p, names[i]);
        if (k < 0) {
            memset(out, 0, sizeof *out);
            return enc_fail(out, "encoder '%s' has no operand '%s'", encoder, names[i]);
        }
        args[k] = values[i];
        bound[k] = true;
    }
    if (!allow_unbound)
        for (size_t k = 0; k < p->param_count; k++)
            if (!bound[k]) {
                memset(out, 0, sizeof *out);
                return enc_fail(out, "operand '%s' of encoder '%s' is not bound",
                                p->params[k], encoder);
            }
    return dsym_encode(s, ei, ti, args, p->param_count, out);
}

/* ======================================================================== */
/* Load / free / write                                                      */
/* ======================================================================== */

DsymSpec *dsym_spec_new(void) {
    DsymArena *a = dsym_arena_new();
    if (!a) return NULL;
    DsymSpec *s = dsym_arena_alloc(a, sizeof *s);
    if (!s) {
        dsym_arena_free(a);
        return NULL;
    }
    s->arena = a;
    s->schema = DSYM_SCHEMA_VERSION;
    s->base_dir = ".";
    return s;
}

void dsym_spec_free(DsymSpec *spec) {
    if (spec) dsym_arena_free(spec->arena);
}

static void set_paths(DsymSpec *s, const char *vpath) {
    if (!vpath) return;
    s->source_path = dsym_arena_strdup(s->arena, vpath);
    const char *slash = strrchr(vpath, '/');
    s->base_dir = slash ? dsym_arena_strndup(s->arena, vpath, (size_t)(slash - vpath) + (slash == vpath))
                        : ".";
}

DsymSpec *dsym_spec_from_tree(const DsymNode *root, const char *vpath,
                              uint32_t flags, DsymDiags *diags) {
    DsymSpec *s = dsym_spec_new();
    if (!s) return NULL;
    set_paths(s, vpath);
    Binder b = {s->arena, diags, s->source_path, "", 0, false};
    size_t errs = diags ? diags->errors : 0;
    if (!bind_record(&b, &SPEC_R, root, s) || b.failed) {
        dsym_spec_free(s);
        return NULL;
    }
    if (!dsym_spec_resolve(s, flags, diags) || (diags && diags->errors > errs)) {
        dsym_spec_free(s);
        return NULL;
    }
    if ((flags & DSYM_LOAD_BACKEND) && s->backend && s->backend->file &&
        !s->backend->document && !dsym_spec_load_backend(s, diags)) {
        dsym_spec_free(s);
        return NULL;
    }
    return s;
}

DsymSpec *dsym_spec_load_buffer(const char *data, size_t len, DsymFormat fmt,
                                const char *vpath, uint32_t flags,
                                DsymDiags *diags) {
    DsymArena *tmp = dsym_arena_new();
    if (!tmp) return NULL;
    DsymNode *root = dsym_tree_read(fmt, tmp, data, len, vpath, diags);
    DsymSpec *s = root ? dsym_spec_from_tree(root, vpath, flags, diags) : NULL;
    dsym_arena_free(tmp);
    return s;
}

DsymSpec *dsym_spec_load_file(const char *path, DsymFormat fmt, uint32_t flags,
                              DsymDiags *diags) {
    DsymBuf buf;
    dsym_buf_init(&buf);
    if (!dsym_buf_read_file(&buf, path)) {
        dsym_diag_add(diags, DSYM_ERROR, path, 0, 0, NULL, "cannot read file: %s",
                      strerror(errno));
        dsym_buf_free(&buf);
        return NULL;
    }
    DsymSpec *s = dsym_spec_load_buffer(buf.data, buf.len, fmt, path, flags, diags);
    dsym_buf_free(&buf);
    return s;
}

bool dsym_spec_load_backend(DsymSpec *s, DsymDiags *diags) {
    if (!s || !s->backend) return true;
    if (s->backend->document) return true;
    if (!s->backend->file) return true;
    char *path = dsym_arena_printf(s->arena, "%s/%s", s->base_dir, s->backend->file);
    DsymBuf buf;
    dsym_buf_init(&buf);
    if (!path || !dsym_buf_read_file(&buf, path)) {
        dsym_diag_add(diags, DSYM_ERROR, s->source_path, 0, 0, "/backend/file",
                      "cannot read backend '%s': %s", path ? path : "?", strerror(errno));
        dsym_buf_free(&buf);
        return false;
    }
    DsymNode *doc = dsym_json_read(s->arena, buf.data, buf.len, path, diags);
    dsym_buf_free(&buf);
    if (!doc) return false;
    if (doc->kind != DSYM_NODE_MAP) {
        dsym_diag_add(diags, DSYM_ERROR, path, 0, 0, NULL,
                      "backend document must be a JSON object");
        return false;
    }
    s->backend->document = doc;
    return true;
}

bool dsym_spec_write(const DsymSpec *spec, DsymFormat fmt, uint32_t flags,
                     DsymBuf *out, DsymDiags *diags) {
    if (fmt == DSYM_FORMAT_JSON && !(flags & DSYM_WRITE_REF_BACKEND))
        flags |= DSYM_WRITE_EMBED_BACKEND;
    DsymArena *a = dsym_arena_new();
    if (!a) return false;
    DsymNode *root = dsym_spec_to_tree(spec, a, flags);
    bool ok = root != NULL;
    if (!ok) dsym_diag_add(diags, DSYM_ERROR, NULL, 0, 0, NULL, "out of memory");
    if (ok && !(flags & DSYM_WRITE_NO_HEADER)) {
        const char *src = spec->source_path ? spec->source_path : "(memory)";
        const char *slash = strrchr(src, '/');
        if (slash) src = slash + 1;
        if (fmt == DSYM_FORMAT_YAML)
            ok = dsym_buf_printf(out, "# Generated by MetaArch from %s (schema %d).\n", src,
                                 DSYM_SCHEMA_VERSION);
        else if (fmt == DSYM_FORMAT_SEXPR)
            ok = dsym_buf_printf(out, ";; Generated by MetaArch from %s (schema %d).\n", src,
                                 DSYM_SCHEMA_VERSION);
    }
    if (ok) ok = dsym_tree_write(fmt, root, out, flags, diags);
    dsym_arena_free(a);
    return ok && !out->oom;
}

bool dsym_spec_write_file(const DsymSpec *spec, const char *path, DsymFormat fmt,
                          uint32_t flags, DsymDiags *diags) {
    if (fmt == DSYM_FORMAT_AUTO) fmt = dsym_format_from_path(path);
    if (fmt == DSYM_FORMAT_AUTO) fmt = DSYM_FORMAT_YAML;
    DsymBuf buf;
    dsym_buf_init(&buf);
    bool ok = dsym_spec_write(spec, fmt, flags, &buf, diags);
    if (ok && !dsym_buf_write_file(&buf, path)) {
        dsym_diag_add(diags, DSYM_ERROR, path, 0, 0, NULL, "cannot write file: %s",
                      strerror(errno));
        ok = false;
    }
    dsym_buf_free(&buf);
    return ok;
}
