/*
 * ArchBFD.c — catalogue of BFD object-format descriptions (see ArchBFD.h).
 *
 *   - binder: document tree -> typed model, with schema validation
 *   - writer: typed model -> document tree (any supported format)
 *   - catalogue: directory scan or catalogue file, lazily loaded documents
 *   - lookup: relocation, record and constant tables
 */
#include "ArchBFD.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ARRLEN(a) (sizeof(a) / sizeof((a)[0]))

/* ======================================================================== */
/* Small helpers                                                            */
/* ======================================================================== */

static const char *const FORMAT_NAMES[] = {"elf", "coff", "pe", "macho", "ecoff", "wasm",
                                            "other"};
static const char *const BYTE_ORDER_NAMES[] = {"little", "big"};
static const char *const OVERFLOW_NAMES[] = {"dont",    "bitfield", "signed",
                                             "unsigned", "truncate", "error"};

const char *archbfd_format_name(ArchBFDFormat f) {
    if ((size_t)f >= ARRLEN(FORMAT_NAMES)) return "other";
    return FORMAT_NAMES[f];
}

const char *archbfd_byte_order_name(ArchBFDByteOrder v) {
    return (size_t)v < ARRLEN(BYTE_ORDER_NAMES) ? BYTE_ORDER_NAMES[v] : "little";
}

const char *archbfd_overflow_name(ArchBFDOverflow v) {
    if ((size_t)v >= ARRLEN(OVERFLOW_NAMES)) return "dont";
    return OVERFLOW_NAMES[v];
}

static const char *dirname_of(DsymArena *a, const char *path) {
    const char *s = strrchr(path, '/');
    if (!s) return ".";
    if (s == path) return "/";
    return dsym_arena_strndup(a, path, (size_t)(s - path));
}

static const char *join_path(DsymArena *a, const char *dir, const char *rel) {
    if (!dir || !*dir || !strcmp(dir, ".") || rel[0] == '/') return dsym_arena_strdup(a, rel);
    size_t L = strlen(dir);
    return dsym_arena_printf(a, "%s%s%s", dir, dir[L - 1] == '/' ? "" : "/", rel);
}

/* ======================================================================== */
/* Binder: tree -> model                                                    */
/* ======================================================================== */

typedef struct Binder {
    DsymArena *arena; /* destination arena (the spec's) */
    DsymDiags *diags;
    const char *file;
    char path[512];
    size_t plen;
    bool failed;
    bool strict;
    bool lint;
} Binder;

static size_t path_push(Binder *b, const char *seg) {
    size_t old = b->plen;
    int n = snprintf(b->path + b->plen, sizeof b->path - b->plen, "/%s", seg);
    if (n > 0)
        b->plen = b->plen + (size_t)n < sizeof b->path ? b->plen + (size_t)n
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
    dsym_diag_add(b->diags, DSYM_ERROR, b->file, n ? n->line : 0, n ? n->col : 0, b->path,
                  "%s", msg);
    b->failed = true;
}

#if defined(__GNUC__)
__attribute__((format(printf, 3, 4)))
#endif
static void bwarn(Binder *b, const DsymNode *n, const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    dsym_diag_add(b->diags, DSYM_WARNING, b->file, n ? n->line : 0, n ? n->col : 0, b->path,
                  "%s", msg);
    if (b->strict) b->failed = true;
}

/* Text of a scalar node (strings, or the original spelling of numbers). */
static bool scalar_text(const DsymNode *n, const char **s, size_t *len) {
    if (!n) return false;
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

static bool node_int(const DsymNode *n, int64_t *out) {
    if (!n) return false;
    if (n->kind == DSYM_NODE_INT) {
        *out = n->v.i;
        return true;
    }
    if (n->kind == DSYM_NODE_UINT) {
        if (n->v.u > (uint64_t)INT64_MAX) return false;
        *out = (int64_t)n->v.u;
        return true;
    }
    return false;
}

static bool node_uint(const DsymNode *n, uint64_t *out) {
    if (!n) return false;
    if (n->kind == DSYM_NODE_INT) {
        if (n->v.i < 0) return false;
        *out = (uint64_t)n->v.i;
        return true;
    }
    if (n->kind == DSYM_NODE_UINT) {
        *out = n->v.u;
        return true;
    }
    return false;
}

static bool node_bool(const DsymNode *n, bool *out) {
    if (!n || n->kind != DSYM_NODE_BOOL) return false;
    *out = n->v.b;
    return true;
}

/* Names are printable ASCII without spaces, like the architecture specs. */
static bool check_name(Binder *b, const DsymNode *n, const char *s, size_t len) {
    if (len == 0 || len > ARCHBFD_NAME_MAX) {
        berr(b, n, "name must be 1..%d characters (got %zu)", ARCHBFD_NAME_MAX, len);
        return false;
    }
    for (size_t i = 0; i < len; i++)
        if ((unsigned char)s[i] < '!' || (unsigned char)s[i] > '~') {
            berr(b, n, "name '%.*s' must be printable ASCII without spaces", (int)len, s);
            return false;
        }
    return true;
}

static bool bind_name(Binder *b, const DsymNode *n, const char *key, const char **out) {
    const char *s;
    size_t len;
    if (!n) return false;
    if (!scalar_text(n, &s, &len)) {
        berr(b, n, "'%s' must be a string", key);
        return false;
    }
    if (!check_name(b, n, s, len)) return false;
    *out = dsym_arena_strndup(b->arena, s, len);
    return true;
}

/* Magic bytes are hex digits, optionally separated by spaces. */
static bool bind_magic(Binder *b, const DsymNode *n, const char **out, size_t *out_len) {
    const char *s;
    size_t len, count = 0;
    if (!scalar_text(n, &s, &len)) {
        berr(b, n, "'magic' must be a string");
        return false;
    }
    char hex[64];
    size_t hn = 0;
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == ' ' || c == ':' || c == '-') continue;
        int v;
        if (c >= '0' && c <= '9')
            v = c - '0';
        else if (c >= 'a' && c <= 'f')
            v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            v = c - 'A' + 10;
        else {
            berr(b, n, "'magic' must be hexadecimal digits");
            return false;
        }
        if (hn + 1 >= sizeof hex) {
            berr(b, n, "'magic' is too long");
            return false;
        }
        hex[hn++] = digits[v];
        count++;
    }
    if (count == 0) {
        berr(b, n, "'magic' must not be empty");
        return false;
    }
    if (hn % 2) {
        berr(b, n, "'magic' must have an even number of hexadecimal digits");
        return false;
    }
    *out = dsym_arena_strndup(b->arena, hex, hn);
    *out_len = (hn + 1) / 2;
    return true;
}

/* ---- records ------------------------------------------------------------ */

static bool bind_fields(Binder *b, const DsymNode *rec, ArchBFDRecord *out) {
    const DsymNode *fields = dsym_map_get(rec, "fields");
    if (!fields) return true; /* a record without fields is allowed */
    if (fields->kind != DSYM_NODE_SEQ) {
        berr(b, fields, "'fields' must be a list");
        return false;
    }
    out->fields = dsym_arena_calloc(b->arena, fields->v.seq.count ? fields->v.seq.count : 1,
                                    sizeof *out->fields);
    if (!out->fields) return false;
    int64_t expected = 0;
    for (size_t i = 0; i < fields->v.seq.count; i++) {
        const DsymNode *f = fields->v.seq.items[i];
        size_t old = path_push_idx(b, i);
        path_push(b, "fields");
        path_push_idx(b, i);
        if (f->kind != DSYM_NODE_MAP) {
            berr(b, f, "field must be a mapping");
            path_pop(b, old);
            continue;
        }
        ArchBFDField *out_f = &out->fields[out->field_count++];
        const char *name = NULL;
        if (!bind_name(b, dsym_map_get(f, "name"), "name", &name))
            berr(b, f, "field needs a 'name'");
        out_f->name = name;
        int64_t off = 0, size = 0;
        if (!node_int(dsym_map_get(f, "offset"), &off) || off < 0)
            berr(b, f, "field needs a non-negative 'offset'");
        if (!node_int(dsym_map_get(f, "size"), &size) || size < 0)
            berr(b, f, "field needs a non-negative 'size'");
        out_f->offset = off;
        out_f->size = size;
        const DsymNode *desc = dsym_map_get(f, "desc");
        if (desc) {
            const char *s;
            size_t len;
            if (scalar_text(desc, &s, &len))
                out_f->desc = dsym_arena_strndup(b->arena, s, len);
            else
                berr(b, desc, "'desc' must be a string");
        }
        if (off < expected)
            bwarn(b, f, "field '%s' overlaps the previous field", name ? name : "?");
        if (size > 0) expected = off + size;
        path_pop(b, old);
    }
    return true;
}

static bool bind_encodings(Binder *b, const DsymNode *rec, ArchBFDRecord *out) {
    const DsymNode *encs = dsym_map_get(rec, "encodings");
    if (!encs) return true;
    if (encs->kind != DSYM_NODE_SEQ) {
        berr(b, encs, "'encodings' must be a list");
        return false;
    }
    out->encodings =
        dsym_arena_calloc(b->arena, encs->v.seq.count ? encs->v.seq.count : 1,
                          sizeof *out->encodings);
    if (!out->encodings) return false;
    for (size_t i = 0; i < encs->v.seq.count; i++) {
        const DsymNode *e = encs->v.seq.items[i];
        size_t old = path_push_idx(b, i);
        path_push(b, "encodings");
        path_push_idx(b, i);
        if (e->kind != DSYM_NODE_MAP) {
            berr(b, e, "encoding must be a mapping");
            path_pop(b, old);
            continue;
        }
        ArchBFDEncoding *out_e = &out->encodings[out->encoding_count++];
        const char *field = NULL, *target = NULL;
        if (!bind_name(b, dsym_map_get(e, "field"), "field", &field))
            berr(b, e, "encoding needs a 'field'");
        if (!bind_name(b, dsym_map_get(e, "target"), "target", &target))
            berr(b, e, "encoding needs a 'target'");
        out_e->field = field;
        out_e->target = target;
        uint64_t mask = 0;
        if (!node_uint(dsym_map_get(e, "mask"), &mask))
            berr(b, e, "encoding needs an unsigned 'mask'");
        out_e->mask = mask;
        int64_t shift = 0;
        if (!node_int(dsym_map_get(e, "shift"), &shift)) shift = 0;
        out_e->shift = shift;
        path_pop(b, old);
    }
    return true;
}

static bool bind_record(Binder *b, const DsymNode *n, ArchBFDRecord *out) {
    const char *name = NULL;
    if (!bind_name(b, dsym_map_get(n, "name"), "name", &name)) return false;
    out->name = name;
    const DsymNode *desc = dsym_map_get(n, "desc");
    if (desc) {
        const char *s;
        size_t len;
        if (scalar_text(desc, &s, &len))
            out->desc = dsym_arena_strndup(b->arena, s, len);
        else
            berr(b, desc, "'desc' must be a string");
    }
    int64_t size = 0;
    bool has_size = node_int(dsym_map_get(n, "size"), &size);
    if (!has_size) size = 0;
    if (!bind_fields(b, n, out)) return false;
    if (!bind_encodings(b, n, out)) return false;
    if (!has_size) {
        int64_t total = 0;
        for (size_t i = 0; i < out->field_count; i++) {
            int64_t end = out->fields[i].offset + out->fields[i].size;
            if (end > total) total = end;
        }
        size = total;
    }
    out->size = size;
    /* Cross checks: fields must fit, encodings must name a real field. */
    for (size_t i = 0; i < out->field_count; i++) {
        const ArchBFDField *f = &out->fields[i];
        if (f->size == 0) continue;
        if (f->offset + f->size > out->size)
            berr(b, n, "field '%s' ends at %" PRId64 " but the record is %" PRId64
                            " bytes",
                 f->name ? f->name : "?", f->offset + f->size, out->size);
    }
    for (size_t i = 0; i < out->encoding_count; i++) {
        const ArchBFDEncoding *e = &out->encodings[i];
        bool found = false;
        for (size_t k = 0; k < out->field_count; k++)
            if (out->fields[k].name && e->field && !strcmp(out->fields[k].name, e->field))
                found = true;
        if (!found)
            berr(b, n, "encoding refers to unknown field '%s'", e->field ? e->field : "?");
    }
    return true;
}

/* ---- constants ---------------------------------------------------------- */

static bool bind_constant_group(Binder *b, const DsymNode *n, ArchBFDConstantGroup *out) {
    const char *name = NULL;
    if (!bind_name(b, dsym_map_get(n, "name"), "name", &name)) return false;
    out->name = name;
    const DsymNode *desc = dsym_map_get(n, "desc");
    if (desc) {
        const char *s;
        size_t len;
        if (scalar_text(desc, &s, &len))
            out->desc = dsym_arena_strndup(b->arena, s, len);
        else
            berr(b, desc, "'desc' must be a string");
    }
    const DsymNode *items = dsym_map_get(n, "items");
    if (!items) return true;
    if (items->kind != DSYM_NODE_SEQ) {
        berr(b, items, "'items' must be a list");
        return false;
    }
    out->items = dsym_arena_calloc(b->arena, items->v.seq.count ? items->v.seq.count : 1,
                                   sizeof *out->items);
    if (!out->items) return false;
    for (size_t i = 0; i < items->v.seq.count; i++) {
        const DsymNode *it = items->v.seq.items[i];
        size_t old = path_push_idx(b, i);
        path_push(b, "items");
        path_push_idx(b, i);
        if (it->kind != DSYM_NODE_MAP) {
            berr(b, it, "constant must be a mapping");
            path_pop(b, old);
            continue;
        }
        ArchBFDConstant *c = &out->items[out->count++];
        const char *cname = NULL;
        if (!bind_name(b, dsym_map_get(it, "name"), "name", &cname))
            berr(b, it, "constant needs a 'name'");
        c->name = cname;
        int64_t value = 0;
        if (!node_int(dsym_map_get(it, "value"), &value))
            berr(b, it, "constant needs an integer 'value'");
        c->value = value;
        const DsymNode *cdesc = dsym_map_get(it, "desc");
        if (cdesc) {
            const char *s;
            size_t len;
            if (scalar_text(cdesc, &s, &len))
                c->desc = dsym_arena_strndup(b->arena, s, len);
            else
                berr(b, cdesc, "'desc' must be a string");
        }
        for (size_t k = 0; k + 1 < out->count; k++)
            if (out->items[k].name && c->name && !strcmp(out->items[k].name, c->name))
                berr(b, it, "duplicate constant '%s'", c->name);
        path_pop(b, old);
    }
    return true;
}

/* ---- relocations -------------------------------------------------------- */

static bool bind_reloc(Binder *b, const DsymNode *n, ArchBFDReloc *out) {
    const char *name = NULL;
    if (!bind_name(b, dsym_map_get(n, "name"), "name", &name)) return false;
    out->name = name;
    int64_t number = -1;
    if (!node_int(dsym_map_get(n, "number"), &number))
        berr(b, n, "relocation needs an integer 'number'");
    out->number = number;
    int64_t size = 0, bitsize = 0;
    if (node_int(dsym_map_get(n, "size"), &size)) out->size = size;
    if (node_int(dsym_map_get(n, "bitsize"), &bitsize)) out->bitsize = bitsize;
    bool flag = false;
    if (node_bool(dsym_map_get(n, "pcrel"), &flag)) out->pcrel = flag;
    if (node_bool(dsym_map_get(n, "pcrel_offset"), &flag)) out->pcrel_offset = flag;
    if (node_bool(dsym_map_get(n, "partial"), &flag)) out->partial = flag;
    const DsymNode *ovf = dsym_map_get(n, "overflow");
    if (ovf) {
        const char *s;
        size_t len;
        if (!scalar_text(ovf, &s, &len)) {
            berr(b, ovf, "'overflow' must be a string");
        } else {
            bool found = false;
            for (size_t i = 0; i < ARRLEN(OVERFLOW_NAMES); i++)
                if (len == strlen(OVERFLOW_NAMES[i]) && !memcmp(s, OVERFLOW_NAMES[i], len)) {
                    out->overflow = (ArchBFDOverflow)i;
                    found = true;
                }
            if (!found) berr(b, ovf, "unknown overflow policy '%.*s'", (int)len, s);
        }
    }
    int64_t v = 0;
    if (node_int(dsym_map_get(n, "rightshift"), &v)) out->rightshift = v;
    if (node_int(dsym_map_get(n, "bitpos"), &v)) out->bitpos = v;
    uint64_t mask = 0;
    if (node_uint(dsym_map_get(n, "src_mask"), &mask)) out->src_mask = mask;
    if (node_uint(dsym_map_get(n, "dst_mask"), &mask)) out->dst_mask = mask;
    const DsymNode *desc = dsym_map_get(n, "desc");
    if (desc) {
        const char *s;
        size_t len;
        if (scalar_text(desc, &s, &len))
            out->desc = dsym_arena_strndup(b->arena, s, len);
        else
            berr(b, desc, "'desc' must be a string");
    }
    /* A relocation that occupies more bits than it has bytes is a data error. */
    if (out->size > 0 && out->bitsize > out->size * 8)
        berr(b, n, "relocation '%s' claims %" PRId64 " bits but only %" PRId64
                    " bytes",
             name ? name : "?", out->bitsize, out->size);
    if (out->size == 0 && out->bitsize != 0)
        bwarn(b, n, "relocation '%s' has no size but %" PRId64 " bits",
              name ? name : "?", out->bitsize);
    return true;
}

/* ---- document ----------------------------------------------------------- */

static bool bind_source(Binder *b, const DsymNode *n, ArchBFDSource *out) {
    const char *file = NULL;
    if (!bind_name(b, dsym_map_get(n, "file"), "file", &file)) return false;
    out->file = file;
    const DsymNode *note = dsym_map_get(n, "note");
    if (note) {
        const char *s;
        size_t len;
        if (scalar_text(note, &s, &len))
            out->note = dsym_arena_strndup(b->arena, s, len);
        else
            berr(b, note, "'note' must be a string");
    }
    return true;
}

static bool bind_spec(Binder *b, const DsymNode *root, ArchBFDSpec *spec) {
    if (root->kind != DSYM_NODE_MAP) {
        berr(b, root, "document must be a mapping");
        return false;
    }
    /* schema */
    const DsymNode *sch = dsym_map_get(root, "schema");
    int64_t schema = 0;
    if (!sch) {
        berr(b, root, "missing required key 'schema'");
        return false;
    }
    if (!node_int(sch, &schema) || schema != ARCHBFD_SCHEMA_VERSION) {
        berr(b, sch, "schema must be %d", ARCHBFD_SCHEMA_VERSION);
        return false;
    }
    spec->schema = schema;

    /* name */
    const DsymNode *name = dsym_map_get(root, "name");
    if (!name) {
        berr(b, root, "missing required key 'name'");
        return false;
    }
    if (!bind_name(b, name, "name", &spec->name)) return false;

    /* format */
    const DsymNode *fmt = dsym_map_get(root, "format");
    if (fmt) {
        const char *s;
        size_t len;
        if (!scalar_text(fmt, &s, &len)) {
            berr(b, fmt, "'format' must be a string");
        } else {
            bool found = false;
            for (size_t i = 0; i < ARRLEN(FORMAT_NAMES); i++)
                if (len == strlen(FORMAT_NAMES[i]) && !memcmp(s, FORMAT_NAMES[i], len)) {
                    spec->format = (ArchBFDFormat)i;
                    spec->has_format = true;
                    found = true;
                }
            if (!found) berr(b, fmt, "unknown object format '%.*s'", (int)len, s);
        }
    }

    /* description */
    const DsymNode *desc = dsym_map_get(root, "description");
    if (desc) {
        const char *s;
        size_t len;
        if (scalar_text(desc, &s, &len))
            spec->description = dsym_arena_strndup(b->arena, s, len);
        else
            berr(b, desc, "'description' must be a string");
    }

    /* byte_order */
    const DsymNode *bo = dsym_map_get(root, "byte_order");
    if (bo) {
        const char *s;
        size_t len;
        if (!scalar_text(bo, &s, &len)) {
            berr(b, bo, "'byte_order' must be a string");
        } else if (len == 6 && !memcmp(s, "little", 6)) {
            spec->byte_order = ARCHBFD_BO_LITTLE;
            spec->has_byte_order = true;
        } else if (len == 3 && !memcmp(s, "big", 3)) {
            spec->byte_order = ARCHBFD_BO_BIG;
            spec->has_byte_order = true;
        } else {
            berr(b, bo, "unknown byte order '%.*s'", (int)len, s);
        }
    }

    /* target_vector, arch, container */
    const DsymNode *tv = dsym_map_get(root, "target_vector");
    if (tv) {
        const char *s;
        size_t len;
        if (scalar_text(tv, &s, &len))
            spec->target_vector = dsym_arena_strndup(b->arena, s, len);
        else
            berr(b, tv, "'target_vector' must be a string");
    }
    const DsymNode *arch = dsym_map_get(root, "arch");
    if (arch) {
        const char *s;
        size_t len;
        if (scalar_text(arch, &s, &len))
            spec->arch = dsym_arena_strndup(b->arena, s, len);
        else
            berr(b, arch, "'arch' must be a string");
    }
    const DsymNode *cont = dsym_map_get(root, "container");
    if (cont) {
        const char *s;
        size_t len;
        if (scalar_text(cont, &s, &len))
            spec->container = dsym_arena_strndup(b->arena, s, len);
        else
            berr(b, cont, "'container' must be a string");
    }

    /* magic */
    const DsymNode *magic = dsym_map_get(root, "magic");
    if (magic) bind_magic(b, magic, &spec->magic, &spec->magic_len);

    /* flags */
    const DsymNode *flags = dsym_map_get(root, "flags");
    if (flags) {
        if (flags->kind != DSYM_NODE_SEQ) {
            berr(b, flags, "'flags' must be a list");
        } else {
            spec->flags = dsym_arena_calloc(b->arena, flags->v.seq.count ? flags->v.seq.count : 1,
                                            sizeof *spec->flags);
            if (spec->flags)
                for (size_t i = 0; i < flags->v.seq.count; i++) {
                    const DsymNode *f = flags->v.seq.items[i];
                    const char *s;
                    size_t len;
                    if (scalar_text(f, &s, &len))
                        spec->flags[spec->flag_count++] =
                            dsym_arena_strndup(b->arena, s, len);
                    else
                        berr(b, f, "flag must be a string");
                }
        }
    }

    /* source */
    const DsymNode *src = dsym_map_get(root, "source");
    if (src) {
        if (src->kind != DSYM_NODE_SEQ) {
            berr(b, src, "'source' must be a list");
        } else {
            spec->sources = dsym_arena_calloc(b->arena, src->v.seq.count ? src->v.seq.count : 1,
                                              sizeof *spec->sources);
            if (spec->sources)
                for (size_t i = 0; i < src->v.seq.count; i++) {
                    const DsymNode *it = src->v.seq.items[i];
                    size_t old = path_push_idx(b, i);
                    path_push(b, "source");
                    path_push_idx(b, i);
                    if (it->kind != DSYM_NODE_MAP) {
                        berr(b, it, "source entry must be a mapping");
                    } else {
                        bind_source(b, it, &spec->sources[spec->source_count++]);
                    }
                    path_pop(b, old);
                }
        }
    }

    /* records */
    const DsymNode *records = dsym_map_get(root, "records");
    if (records) {
        if (records->kind != DSYM_NODE_SEQ) {
            berr(b, records, "'records' must be a list");
        } else {
            spec->records = dsym_arena_calloc(b->arena, records->v.seq.count ? records->v.seq.count : 1,
                                              sizeof *spec->records);
            if (spec->records)
                for (size_t i = 0; i < records->v.seq.count; i++) {
                    const DsymNode *it = records->v.seq.items[i];
                    size_t old = path_push_idx(b, i);
                    path_push(b, "records");
                    path_push_idx(b, i);
                    if (it->kind != DSYM_NODE_MAP) {
                        berr(b, it, "record must be a mapping");
                    } else {
                        bind_record(b, it, &spec->records[spec->record_count++]);
                    }
                    path_pop(b, old);
                }
        }
    }

    /* constants */
    const DsymNode *constants = dsym_map_get(root, "constants");
    if (constants) {
        if (constants->kind != DSYM_NODE_SEQ) {
            berr(b, constants, "'constants' must be a list");
        } else {
            spec->constants =
                dsym_arena_calloc(b->arena, constants->v.seq.count ? constants->v.seq.count : 1,
                                  sizeof *spec->constants);
            if (spec->constants)
                for (size_t i = 0; i < constants->v.seq.count; i++) {
                    const DsymNode *it = constants->v.seq.items[i];
                    size_t old = path_push_idx(b, i);
                    path_push(b, "constants");
                    path_push_idx(b, i);
                    if (it->kind != DSYM_NODE_MAP) {
                        berr(b, it, "constant group must be a mapping");
                    } else {
                        bind_constant_group(b, it, &spec->constants[spec->constant_group_count++]);
                    }
                    path_pop(b, old);
                }
        }
    }

    /* relocations */
    const DsymNode *relocs = dsym_map_get(root, "relocations");
    if (relocs) {
        if (relocs->kind != DSYM_NODE_SEQ) {
            berr(b, relocs, "'relocations' must be a list");
        } else {
            spec->relocations =
                dsym_arena_calloc(b->arena, relocs->v.seq.count ? relocs->v.seq.count : 1,
                                  sizeof *spec->relocations);
            if (spec->relocations)
                for (size_t i = 0; i < relocs->v.seq.count; i++) {
                    const DsymNode *it = relocs->v.seq.items[i];
                    size_t old = path_push_idx(b, i);
                    path_push(b, "relocations");
                    path_push_idx(b, i);
                    if (it->kind != DSYM_NODE_MAP) {
                        berr(b, it, "relocation must be a mapping");
                    } else {
                        bind_reloc(b, it, &spec->relocations[spec->reloc_count++]);
                    }
                    path_pop(b, old);
                }
        }
    }

    /* sections */
    const DsymNode *sections = dsym_map_get(root, "sections");
    if (sections) {
        if (sections->kind != DSYM_NODE_SEQ) {
            berr(b, sections, "'sections' must be a list");
        } else {
            spec->sections = dsym_arena_calloc(b->arena, sections->v.seq.count ? sections->v.seq.count : 1,
                                               sizeof *spec->sections);
            if (spec->sections)
                for (size_t i = 0; i < sections->v.seq.count; i++) {
                    const DsymNode *it = sections->v.seq.items[i];
                    size_t old = path_push_idx(b, i);
                    path_push(b, "sections");
                    path_push_idx(b, i);
                    if (it->kind != DSYM_NODE_MAP) {
                        berr(b, it, "section must be a mapping");
                    } else {
                        const char *sname = NULL;
                        if (!bind_name(b, dsym_map_get(it, "name"), "name", &sname))
                            berr(b, it, "section needs a 'name'");
                        ArchBFDSection *sec = &spec->sections[spec->section_count++];
                        sec->name = sname;
                        const DsymNode *sdesc = dsym_map_get(it, "desc");
                        if (sdesc) {
                            const char *s;
                            size_t len;
                            if (scalar_text(sdesc, &s, &len))
                                sec->desc = dsym_arena_strndup(b->arena, s, len);
                            else
                                berr(b, sdesc, "'desc' must be a string");
                        }
                    }
                    path_pop(b, old);
                }
        }
    }

    /* Unknown keys are a warning: they usually mean a typo. */
    for (size_t i = 0; i < root->v.map.count; i++) {
        const char *k = root->v.map.items[i].key;
        static const char *const KNOWN[] = {
            "schema",     "name",       "format",     "description", "byte_order",
            "target_vector", "arch",    "container",  "magic",       "flags",
            "source",     "records",    "constants",  "relocations", "sections"};
        bool known = false;
        for (size_t j = 0; j < ARRLEN(KNOWN); j++)
            if (!strcmp(k, KNOWN[j])) known = true;
        if (!known)
            bwarn(b, root->v.map.items[i].value, "unknown key '%s'", k);
    }

    /* Duplicate relocation numbers are ambiguous. */
    for (size_t i = 0; i < spec->reloc_count; i++)
        for (size_t k = i + 1; k < spec->reloc_count; k++)
            if (spec->relocations[i].number >= 0 &&
                spec->relocations[i].number == spec->relocations[k].number)
                berr(b, root, "relocations '%s' and '%s' share the number %" PRId64,
                     spec->relocations[i].name ? spec->relocations[i].name : "?",
                     spec->relocations[k].name ? spec->relocations[k].name : "?",
                     spec->relocations[i].number);

    /* Duplicate record names are ambiguous. */
    for (size_t i = 0; i < spec->record_count; i++)
        for (size_t k = i + 1; k < spec->record_count; k++)
            if (spec->records[i].name && spec->records[k].name &&
                !strcmp(spec->records[i].name, spec->records[k].name))
                berr(b, root, "duplicate record '%s'", spec->records[i].name);

    return !b->failed;
}

/* ======================================================================== */
/* Lookup tables                                                            */
/* ======================================================================== */

typedef struct StrEntry {
    const char *key;
    int32_t index;
} StrEntry;

/* Open-addressing tables: one per kind, sized to a power of two. */
typedef struct ArchBFDIndex {
    StrEntry *relocs;
    size_t reloc_cap;
    StrEntry *records;
    size_t record_cap;
    StrEntry *groups; /* constant groups */
    size_t group_cap;
} ArchBFDIndex;

static uint64_t hash_str(const char *s) {
    uint64_t h = 1469598103934665603ULL;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 1099511628211ULL;
    }
    return h;
}

static StrEntry *index_find(StrEntry *tab, size_t cap, const char *key) {
    if (!tab || !cap || !key) return NULL;
    size_t i = (size_t)(hash_str(key) % cap);
    for (size_t probes = 0; probes <= cap; probes++) {
        if (!tab[i].key) return NULL;
        if (!strcmp(tab[i].key, key)) return &tab[i];
        i = (i + 1) % cap;
    }
    return NULL;
}

/* Fill the open-addressing tables of the index. */
static bool build_tables(ArchBFDIndex *t, const ArchBFDSpec *s) {
    t->reloc_cap = s->reloc_count * 2 + 16;
    t->relocs = calloc(t->reloc_cap, sizeof *t->relocs);
    t->record_cap = s->record_count * 2 + 16;
    t->records = calloc(t->record_cap, sizeof *t->records);
    t->group_cap = s->constant_group_count * 2 + 16;
    t->groups = calloc(t->group_cap, sizeof *t->groups);
    if (!t->relocs || !t->records || !t->groups) return false;
    for (size_t i = 0; i < s->reloc_count; i++) {
        if (!s->relocations[i].name) continue;
        size_t slot = (size_t)(hash_str(s->relocations[i].name) % t->reloc_cap);
        while (t->relocs[slot].key) {
            if (!strcmp(t->relocs[slot].key, s->relocations[i].name)) break;
            slot = (slot + 1) % t->reloc_cap;
        }
        if (!t->relocs[slot].key) {
            t->relocs[slot].key = s->relocations[i].name;
            t->relocs[slot].index = (int32_t)i;
        }
    }
    for (size_t i = 0; i < s->record_count; i++) {
        if (!s->records[i].name) continue;
        size_t slot = (size_t)(hash_str(s->records[i].name) % t->record_cap);
        while (t->records[slot].key) {
            if (!strcmp(t->records[slot].key, s->records[i].name)) break;
            slot = (slot + 1) % t->record_cap;
        }
        if (!t->records[slot].key) {
            t->records[slot].key = s->records[i].name;
            t->records[slot].index = (int32_t)i;
        }
    }
    for (size_t i = 0; i < s->constant_group_count; i++) {
        if (!s->constants[i].name) continue;
        size_t slot = (size_t)(hash_str(s->constants[i].name) % t->group_cap);
        while (t->groups[slot].key) {
            if (!strcmp(t->groups[slot].key, s->constants[i].name)) break;
            slot = (slot + 1) % t->group_cap;
        }
        if (!t->groups[slot].key) {
            t->groups[slot].key = s->constants[i].name;
            t->groups[slot].index = (int32_t)i;
        }
    }
    return true;
}

int32_t archbfd_find_reloc(const ArchBFDSpec *s, const char *name) {
    if (!s || !s->index || !name) return -1;
    StrEntry *e = index_find(s->index->relocs, s->index->reloc_cap, name);
    return e ? e->index : -1;
}

int32_t archbfd_find_reloc_number(const ArchBFDSpec *s, int64_t number) {
    if (!s) return -1;
    for (size_t i = 0; i < s->reloc_count; i++)
        if (s->relocations[i].number == number) return (int32_t)i;
    return -1;
}

const ArchBFDRecord *archbfd_find_record(const ArchBFDSpec *s, const char *name) {
    if (!s || !s->index || !name) return NULL;
    StrEntry *e = index_find(s->index->records, s->index->record_cap, name);
    return e ? &s->records[e->index] : NULL;
}

const ArchBFDRecord *archbfd_resolve_record(const ArchBFDSpec *s, const char *name) {
    const ArchBFDRecord *r = archbfd_find_record(s, name);
    if (r) return r;
    if (s && s->container_spec) return archbfd_resolve_record(s->container_spec, name);
    return NULL;
}

bool archbfd_constant(const ArchBFDSpec *s, const char *group, const char *name,
                      int64_t *value) {
    if (!s || !s->index || !group || !name) return false;
    StrEntry *g = index_find(s->index->groups, s->index->group_cap, group);
    if (!g) return false;
    const ArchBFDConstantGroup *grp = &s->constants[g->index];
    for (size_t i = 0; i < grp->count; i++)
        if (grp->items[i].name && !strcmp(grp->items[i].name, name)) {
            if (value) *value = grp->items[i].value;
            return true;
        }
    return false;
}

bool archbfd_resolve_constant(const ArchBFDSpec *s, const char *group, const char *name,
                               int64_t *value) {
    if (archbfd_constant(s, group, name, value)) return true;
    if (s && s->container_spec)
        return archbfd_resolve_constant(s->container_spec, group, name, value);
    return false;
}

/* ======================================================================== */
/* Loading                                                                  */
/* ======================================================================== */

static void spec_free_internal(ArchBFDSpec *spec) {
    if (!spec) return;
    if (spec->owned_container) archbfd_spec_free(spec->owned_container);
    if (spec->index) {
        free(spec->index->relocs);
        free(spec->index->records);
        free(spec->index->groups);
        free(spec->index);
    }
    dsym_arena_free(spec->arena);
    free(spec);
}

void archbfd_spec_free(ArchBFDSpec *spec) { spec_free_internal(spec); }

ArchBFDSpec *archbfd_spec_load_buffer(const char *data, size_t len, DsymFormat fmt,
                                       const char *virtual_path, uint32_t flags,
                                       DsymDiags *diags) {
    if (!data && len) return NULL;
    DsymArena *a = dsym_arena_new();
    if (!a) return NULL;
    DsymNode *root = dsym_tree_read(fmt, a, data, len, virtual_path, diags);
    if (!root) {
        dsym_arena_free(a);
        return NULL;
    }
    ArchBFDSpec *spec = calloc(1, sizeof *spec);
    if (!spec) {
        dsym_arena_free(a);
        return NULL;
    }
    spec->arena = a;
    spec->container_index = -1;
    spec->source_path = virtual_path ? dsym_arena_strdup(a, virtual_path) : NULL;
    spec->base_dir = virtual_path ? dirname_of(a, virtual_path) : dsym_arena_strdup(a, ".");

    Binder b;
    memset(&b, 0, sizeof b);
    b.arena = a;
    b.diags = diags;
    b.file = virtual_path;
    b.strict = (flags & ARCHBFD_LOAD_STRICT) != 0;
    b.lint = (flags & ARCHBFD_LOAD_LINT) != 0;
    if (!bind_spec(&b, root, spec)) {
        spec_free_internal(spec);
        return NULL;
    }
    /* Build the lookup tables. */
    spec->index = calloc(1, sizeof *spec->index);
    if (!spec->index) {
        spec_free_internal(spec);
        return NULL;
    }
    if (!build_tables(spec->index, spec)) {
        spec_free_internal(spec);
        return NULL;
    }
    return spec;
}

ArchBFDSpec *archbfd_spec_load_file(const char *path, DsymFormat fmt, uint32_t flags,
                                     DsymDiags *diags) {
    if (!path) return NULL;
    DsymBuf buf;
    dsym_buf_init(&buf);
    if (!dsym_buf_read_file(&buf, path)) {
        dsym_diag_add(diags, DSYM_ERROR, path, 0, 0, NULL, "cannot read: %s", strerror(errno));
        dsym_buf_free(&buf);
        return NULL;
    }
    ArchBFDSpec *spec =
        archbfd_spec_load_buffer(buf.data, buf.len, fmt, path, flags, diags);
    dsym_buf_free(&buf);
    if (!spec) return NULL;
    /* A document that inherits a container looks for it next to itself, so
       that a document can be read without the catalogue. */
    if (spec->container && !spec->container_spec) {
        DsymArena *a = spec->arena;
        const char *dir = dirname_of(a, path);
        static const char *const EXT[] = {".bfd.yaml", ".bfd.yml", ".bfd.json", ".bfd.xml",
                                          ".bfd.sexp"};
        for (size_t i = 0; i < sizeof EXT / sizeof *EXT; i++) {
            char *candidate = dsym_arena_printf(a, "%s/%s%s", dir, spec->container, EXT[i]);
            if (!candidate) break;
            FILE *fh = fopen(candidate, "rb");
            if (!fh) continue;
            fclose(fh);
            ArchBFDSpec *cont = archbfd_spec_load_file(candidate, DSYM_FORMAT_AUTO, flags, diags);
            if (cont) {
                spec->owned_container = cont;
                spec->container_spec = cont;
                break;
            }
        }
    }
    return spec;
}

/* ======================================================================== */
/* Writer: model -> tree                                                    */
/* ======================================================================== */

static DsymNode *record_to_tree(DsymArena *a, const ArchBFDRecord *r) {
    DsymNode *m = dsym_node_map(a);
    if (!m) return NULL;
    dsym_map_put_ref(a, m, "name", dsym_node_str_ref(a, r->name));
    if (r->desc) dsym_map_put_ref(a, m, "desc", dsym_node_str_ref(a, r->desc));
    dsym_map_put_ref(a, m, "size", dsym_node_int(a, r->size));
    if (r->field_count) {
        DsymNode *list = dsym_node_seq(a);
        if (!list) return NULL;
        dsym_map_put_ref(a, m, "fields", list);
        for (size_t i = 0; i < r->field_count; i++) {
            const ArchBFDField *f = &r->fields[i];
            DsymNode *fm = dsym_node_map(a);
            if (!fm) return NULL;
            dsym_map_put_ref(a, fm, "name", dsym_node_str_ref(a, f->name));
            dsym_map_put_ref(a, fm, "offset", dsym_node_int(a, f->offset));
            dsym_map_put_ref(a, fm, "size", dsym_node_int(a, f->size));
            if (f->desc) dsym_map_put_ref(a, fm, "desc", dsym_node_str_ref(a, f->desc));
            dsym_seq_push(a, list, fm);
        }
    }
    if (r->encoding_count) {
        DsymNode *list = dsym_node_seq(a);
        if (!list) return NULL;
        dsym_map_put_ref(a, m, "encodings", list);
        for (size_t i = 0; i < r->encoding_count; i++) {
            const ArchBFDEncoding *e = &r->encodings[i];
            DsymNode *em = dsym_node_map(a);
            if (!em) return NULL;
            dsym_map_put_ref(a, em, "field", dsym_node_str_ref(a, e->field));
            dsym_map_put_ref(a, em, "target", dsym_node_str_ref(a, e->target));
            dsym_map_put_ref(a, em, "mask", dsym_node_uint(a, e->mask));
            dsym_map_put_ref(a, em, "shift", dsym_node_int(a, e->shift));
            dsym_seq_push(a, list, em);
        }
    }
    return m;
}

static DsymNode *reloc_to_tree(DsymArena *a, const ArchBFDReloc *r) {
    DsymNode *m = dsym_node_map(a);
    if (!m) return NULL;
    dsym_map_put_ref(a, m, "name", dsym_node_str_ref(a, r->name));
    dsym_map_put_ref(a, m, "number", dsym_node_int(a, r->number));
    dsym_map_put_ref(a, m, "size", dsym_node_int(a, r->size));
    dsym_map_put_ref(a, m, "bitsize", dsym_node_int(a, r->bitsize));
    if (r->pcrel) dsym_map_put_ref(a, m, "pcrel", dsym_node_bool(a, true));
    if (r->pcrel_offset) dsym_map_put_ref(a, m, "pcrel_offset", dsym_node_bool(a, true));
    if (r->partial) dsym_map_put_ref(a, m, "partial", dsym_node_bool(a, true));
    dsym_map_put_ref(a, m, "overflow", dsym_node_str_ref(a, archbfd_overflow_name(r->overflow)));
    if (r->rightshift) dsym_map_put_ref(a, m, "rightshift", dsym_node_int(a, r->rightshift));
    if (r->bitpos) dsym_map_put_ref(a, m, "bitpos", dsym_node_int(a, r->bitpos));
    if (r->src_mask) dsym_map_put_ref(a, m, "src_mask", dsym_node_uint(a, r->src_mask));
    if (r->dst_mask) dsym_map_put_ref(a, m, "dst_mask", dsym_node_uint(a, r->dst_mask));
    if (r->desc) dsym_map_put_ref(a, m, "desc", dsym_node_str_ref(a, r->desc));
    return m;
}

static DsymNode *spec_to_tree(DsymArena *a, const ArchBFDSpec *s) {
    DsymNode *m = dsym_node_map(a);
    if (!m) return NULL;
    dsym_map_put_ref(a, m, "schema", dsym_node_int(a, s->schema));
    dsym_map_put_ref(a, m, "name", dsym_node_str_ref(a, s->name));
    if (s->has_format)
        dsym_map_put_ref(a, m, "format", dsym_node_str_ref(a, archbfd_format_name(s->format)));
    if (s->description) dsym_map_put_ref(a, m, "description", dsym_node_str_ref(a, s->description));
    if (s->has_byte_order)
        dsym_map_put_ref(a, m, "byte_order",
                         dsym_node_str_ref(a, archbfd_byte_order_name(s->byte_order)));
    if (s->target_vector)
        dsym_map_put_ref(a, m, "target_vector", dsym_node_str_ref(a, s->target_vector));
    if (s->arch) dsym_map_put_ref(a, m, "arch", dsym_node_str_ref(a, s->arch));
    if (s->container) dsym_map_put_ref(a, m, "container", dsym_node_str_ref(a, s->container));
    if (s->magic) dsym_map_put_ref(a, m, "magic", dsym_node_str_ref(a, s->magic));
    if (s->flag_count) {
        DsymNode *list = dsym_node_seq(a);
        if (!list) return NULL;
        dsym_map_put_ref(a, m, "flags", list);
        for (size_t i = 0; i < s->flag_count; i++)
            dsym_seq_push(a, list, dsym_node_str_ref(a, s->flags[i]));
    }
    if (s->source_count) {
        DsymNode *list = dsym_node_seq(a);
        if (!list) return NULL;
        dsym_map_put_ref(a, m, "source", list);
        for (size_t i = 0; i < s->source_count; i++) {
            DsymNode *sm = dsym_node_map(a);
            if (!sm) return NULL;
            dsym_map_put_ref(a, sm, "file", dsym_node_str_ref(a, s->sources[i].file));
            if (s->sources[i].note)
                dsym_map_put_ref(a, sm, "note", dsym_node_str_ref(a, s->sources[i].note));
            dsym_seq_push(a, list, sm);
        }
    }
    if (s->record_count) {
        DsymNode *list = dsym_node_seq(a);
        if (!list) return NULL;
        dsym_map_put_ref(a, m, "records", list);
        for (size_t i = 0; i < s->record_count; i++)
            dsym_seq_push(a, list, record_to_tree(a, &s->records[i]));
    }
    if (s->constant_group_count) {
        DsymNode *list = dsym_node_seq(a);
        if (!list) return NULL;
        dsym_map_put_ref(a, m, "constants", list);
        for (size_t i = 0; i < s->constant_group_count; i++) {
            const ArchBFDConstantGroup *g = &s->constants[i];
            DsymNode *gm = dsym_node_map(a);
            if (!gm) return NULL;
            dsym_map_put_ref(a, gm, "name", dsym_node_str_ref(a, g->name));
            if (g->desc) dsym_map_put_ref(a, gm, "desc", dsym_node_str_ref(a, g->desc));
            DsymNode *items = dsym_node_seq(a);
            if (!items) return NULL;
            dsym_map_put_ref(a, gm, "items", items);
            for (size_t k = 0; k < g->count; k++) {
                DsymNode *im = dsym_node_map(a);
                if (!im) return NULL;
                dsym_map_put_ref(a, im, "name", dsym_node_str_ref(a, g->items[k].name));
                dsym_map_put_ref(a, im, "value", dsym_node_int(a, g->items[k].value));
                if (g->items[k].desc)
                    dsym_map_put_ref(a, im, "desc", dsym_node_str_ref(a, g->items[k].desc));
                dsym_seq_push(a, items, im);
            }
            dsym_seq_push(a, list, gm);
        }
    }
    if (s->reloc_count) {
        DsymNode *list = dsym_node_seq(a);
        if (!list) return NULL;
        dsym_map_put_ref(a, m, "relocations", list);
        for (size_t i = 0; i < s->reloc_count; i++)
            dsym_seq_push(a, list, reloc_to_tree(a, &s->relocations[i]));
    }
    if (s->section_count) {
        DsymNode *list = dsym_node_seq(a);
        if (!list) return NULL;
        dsym_map_put_ref(a, m, "sections", list);
        for (size_t i = 0; i < s->section_count; i++) {
            DsymNode *sm = dsym_node_map(a);
            if (!sm) return NULL;
            dsym_map_put_ref(a, sm, "name", dsym_node_str_ref(a, s->sections[i].name));
            if (s->sections[i].desc)
                dsym_map_put_ref(a, sm, "desc", dsym_node_str_ref(a, s->sections[i].desc));
            dsym_seq_push(a, list, sm);
        }
    }
    return m;
}

bool archbfd_spec_write(const ArchBFDSpec *spec, DsymFormat fmt, uint32_t flags,
                        DsymBuf *out, DsymDiags *diags) {
    if (!spec || !out) return false;
    DsymArena *a = dsym_arena_new();
    if (!a) return false;
    DsymNode *tree = spec_to_tree(a, spec);
    bool ok = tree != NULL;
    if (ok && fmt == DSYM_FORMAT_YAML)
        ok = dsym_buf_puts(out, "# ArchBFD object-format description.\n");
    if (ok) ok = dsym_tree_write(fmt, tree, out, flags | DSYM_WRITE_NO_HEADER, diags);
    dsym_arena_free(a);
    return ok;
}

/* ======================================================================== */
/* Catalogue                                                                */
/* ======================================================================== */

struct ArchBFD {
    DsymArena *arena;
    const char *root;     /* directory holding the documents */
    const char *root_rel; /* root as written in the catalogue */
    ArchBFDEntry *entries;
    size_t count, cap;
    ArchBFDSpec **cache;
};

static ArchBFD *db_new(void) {
    DsymArena *a = dsym_arena_new();
    if (!a) return NULL;
    ArchBFD *db = dsym_arena_alloc(a, sizeof *db);
    if (!db) {
        dsym_arena_free(a);
        return NULL;
    }
    db->arena = a;
    return db;
}

void archbfd_close(ArchBFD *db) {
    if (!db) return;
    if (db->cache)
        for (size_t i = 0; i < db->count; i++) archbfd_spec_free(db->cache[i]);
    free(db->cache);
    free(db->entries);
    dsym_arena_free(db->arena);
}

static ArchBFDEntry *db_add(ArchBFD *db) {
    if (db->count == db->cap) {
        size_t nc = db->cap ? db->cap * 2 : 16;
        ArchBFDEntry *p = realloc(db->entries, nc * sizeof *p);
        if (!p) return NULL;
        db->entries = p;
        db->cap = nc;
    }
    ArchBFDEntry *e = &db->entries[db->count++];
    memset(e, 0, sizeof *e);
    return e;
}

static bool db_finish(ArchBFD *db) {
    db->cache = calloc(db->count ? db->count : 1, sizeof *db->cache);
    return db->cache != NULL;
}

static int cmp_str(const void *x, const void *y) {
    return strcmp(*(const char *const *)x, *(const char *const *)y);
}

/* Fill the catalogue summary from a loaded document. */
static void fill_summary(ArchBFD *db, ArchBFDEntry *e, const ArchBFDSpec *s) {
    if (!s) return;
    if (s->has_format) e->format = dsym_arena_strdup(db->arena, archbfd_format_name(s->format));
    if (s->has_byte_order)
        e->byte_order = dsym_arena_strdup(db->arena, archbfd_byte_order_name(s->byte_order));
    if (s->arch) e->arch = dsym_arena_strdup(db->arena, s->arch);
    if (s->target_vector)
        e->target_vector = dsym_arena_strdup(db->arena, s->target_vector);
}

ArchBFD *archbfd_scan(const char *dir, DsymDiags *diags) {
    if (!dir) return NULL;
    DIR *d = opendir(dir);
    if (!d) {
        dsym_diag_add(diags, DSYM_ERROR, dir, 0, 0, NULL, "cannot open directory: %s",
                      strerror(errno));
        return NULL;
    }
    ArchBFD *db = db_new();
    if (!db) {
        closedir(d);
        return NULL;
    }
    db->root = dsym_arena_strdup(db->arena, dir);
    db->root_rel = db->root;
    const char **names = NULL;
    size_t n = 0, cap = 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        const char *m = strstr(de->d_name, ".bfd.");
        if (!m || de->d_name[0] == '.') continue;
        if (dsym_format_from_path(de->d_name) == DSYM_FORMAT_AUTO) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 32;
            const char **p = realloc(names, cap * sizeof *p);
            if (!p) break;
            names = p;
        }
        names[n++] = dsym_arena_strdup(db->arena, de->d_name);
    }
    closedir(d);
    qsort(names, n, sizeof *names, cmp_str);
    bool ok = true;
    for (size_t i = 0; i < n; i++) {
        const char *fn = names[i];
        size_t stem = (size_t)(strstr(fn, ".bfd.") - fn);
        bool dup = false;
        for (size_t k = 0; k < db->count; k++)
            if (strlen(db->entries[k].name) == stem && !strncmp(db->entries[k].name, fn, stem))
                dup = true; /* the same document in several formats: first wins */
        if (dup) continue;
        ArchBFDEntry *e = db_add(db);
        if (!e) {
            ok = false;
            break;
        }
        e->name = dsym_arena_strndup(db->arena, fn, stem);
        e->file = fn;
        e->path = join_path(db->arena, dir, fn);
        ArchBFDSpec *s = archbfd_spec_load_file(e->path, DSYM_FORMAT_AUTO, 0, diags);
        if (!s) {
            ok = false;
            continue;
        }
        fill_summary(db, e, s);
        archbfd_spec_free(s);
    }
    free(names);
    if (!ok || !db_finish(db)) {
        archbfd_close(db);
        return NULL;
    }
    return db;
}

static const char *str_or_null(DsymArena *a, const DsymNode *n) {
    if (!n) return NULL;
    if (n->kind == DSYM_NODE_STR) return dsym_arena_strndup(a, n->v.str.ptr, n->v.str.len);
    if (n->text) return dsym_arena_strdup(a, n->text);
    return NULL;
}

ArchBFD *archbfd_open(const char *path, DsymDiags *diags) {
    if (!path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0) {
        dsym_diag_add(diags, DSYM_ERROR, path, 0, 0, NULL, "%s", strerror(errno));
        return NULL;
    }
    if (S_ISDIR(st.st_mode)) return archbfd_scan(path, diags);

    DsymBuf buf;
    dsym_buf_init(&buf);
    if (!dsym_buf_read_file(&buf, path)) {
        dsym_diag_add(diags, DSYM_ERROR, path, 0, 0, NULL, "cannot read: %s", strerror(errno));
        dsym_buf_free(&buf);
        return NULL;
    }
    ArchBFD *db = db_new();
    if (!db) {
        dsym_buf_free(&buf);
        return NULL;
    }
    DsymNode *root = dsym_tree_read(DSYM_FORMAT_AUTO, db->arena, buf.data, buf.len, path, diags);
    dsym_buf_free(&buf);
    bool ok = root && root->kind == DSYM_NODE_MAP;
    if (root && !ok)
        dsym_diag_add(diags, DSYM_ERROR, path, root->line, root->col, NULL,
                      "catalogue must be a mapping");
    if (ok) {
        const DsymNode *sch = dsym_map_get(root, "schema");
        if (!sch || sch->kind != DSYM_NODE_INT || sch->v.i != ARCHBFD_SCHEMA_VERSION) {
            dsym_diag_add(diags, DSYM_ERROR, path, sch ? sch->line : 0, 0, "/schema",
                          "catalogue schema must be %d", ARCHBFD_SCHEMA_VERSION);
            ok = false;
        }
        for (size_t i = 0; i < root->v.map.count && ok; i++) {
            const char *k = root->v.map.items[i].key;
            if (strcmp(k, "schema") && strcmp(k, "root") && strcmp(k, "formats"))
                dsym_diag_add(diags, DSYM_WARNING, path, root->v.map.items[i].value->line, 0,
                              NULL, "unknown catalogue key '%s'", k);
        }
    }
    const char *catdir = dirname_of(db->arena, path);
    if (ok) {
        const char *rel = str_or_null(db->arena, dsym_map_get(root, "root"));
        db->root_rel = rel ? rel : ".";
        db->root = join_path(db->arena, catdir, db->root_rel);
        const DsymNode *formats = dsym_map_get(root, "formats");
        if (!formats || formats->kind != DSYM_NODE_SEQ) {
            dsym_diag_add(diags, DSYM_ERROR, path, 0, 0, "/formats",
                          "'formats' must be a list");
            ok = false;
        }
        for (size_t i = 0; ok && i < formats->v.seq.count; i++) {
            const DsymNode *it = formats->v.seq.items[i];
            char where[48];
            snprintf(where, sizeof where, "/formats/%zu", i);
            if (it->kind != DSYM_NODE_MAP) {
                dsym_diag_add(diags, DSYM_ERROR, path, it->line, it->col, where,
                              "entry must be a mapping");
                ok = false;
                break;
            }
            const char *name = str_or_null(db->arena, dsym_map_get(it, "name"));
            const char *file = str_or_null(db->arena, dsym_map_get(it, "file"));
            if (!name || !file) {
                dsym_diag_add(diags, DSYM_ERROR, path, it->line, it->col, where,
                              "entry needs 'name' and 'file'");
                ok = false;
                break;
            }
            for (size_t k = 0; k < db->count; k++)
                if (!strcmp(db->entries[k].name, name)) {
                    dsym_diag_add(diags, DSYM_ERROR, path, it->line, it->col, where,
                                  "duplicate format '%s'", name);
                    ok = false;
                }
            if (!ok) break;
            ArchBFDEntry *e = db_add(db);
            if (!e) {
                ok = false;
                break;
            }
            e->name = name;
            e->file = file;
            e->path = join_path(db->arena, db->root, file);
            e->format = str_or_null(db->arena, dsym_map_get(it, "format"));
            e->byte_order = str_or_null(db->arena, dsym_map_get(it, "byte_order"));
            e->arch = str_or_null(db->arena, dsym_map_get(it, "arch"));
            e->target_vector = str_or_null(db->arena, dsym_map_get(it, "target_vector"));
        }
    }
    if (!ok || !db_finish(db)) {
        archbfd_close(db);
        return NULL;
    }
    return db;
}

size_t archbfd_count(const ArchBFD *db) { return db ? db->count : 0; }

const ArchBFDEntry *archbfd_entry(const ArchBFD *db, size_t i) {
    return db && i < db->count ? &db->entries[i] : NULL;
}

const ArchBFDEntry *archbfd_find(const ArchBFD *db, const char *name) {
    if (!db || !name) return NULL;
    for (size_t i = 0; i < db->count; i++)
        if (!strcmp(db->entries[i].name, name)) return &db->entries[i];
    for (size_t i = 0; i < db->count; i++)
        if (db->entries[i].target_vector && !strcmp(db->entries[i].target_vector, name))
            return &db->entries[i];
    for (size_t i = 0; i < db->count; i++)
        if (db->entries[i].arch && !strcmp(db->entries[i].arch, name)) return &db->entries[i];
    return NULL;
}

const ArchBFDSpec *archbfd_load(ArchBFD *db, const ArchBFDEntry *e, uint32_t flags,
                                 DsymDiags *diags) {
    if (!db || !e || e < db->entries || e >= db->entries + db->count) return NULL;
    size_t i = (size_t)(e - db->entries);
    if (!db->cache[i]) {
        db->cache[i] = archbfd_spec_load_file(e->path, DSYM_FORMAT_AUTO, flags, diags);
        /* Resolve the container reference inside this catalogue. */
        ArchBFDSpec *spec = db->cache[i];
        if (spec && spec->container) {
            for (size_t k = 0; k < db->count; k++) {
                if (k == i || strcmp(db->entries[k].name, spec->container)) continue;
                const ArchBFDSpec *cont = archbfd_load(db, &db->entries[k], flags, diags);
                if (cont) {
                    spec->container_spec = cont;
                    spec->container_index = (int32_t)k;
                    break;
                }
            }
            if (!spec->container_spec)
                dsym_diag_add(diags, DSYM_WARNING, spec->source_path, 0, 0, NULL,
                              "unknown container '%s': the inherited records are missing",
                              spec->container);
        }
    }
    return db->cache[i];
}

const ArchBFDSpec *archbfd_load_name(ArchBFD *db, const char *name, uint32_t flags,
                                     DsymDiags *diags) {
    const ArchBFDEntry *e = archbfd_find(db, name);
    if (!e) {
        dsym_diag_add(diags, DSYM_ERROR, NULL, 0, 0, NULL, "unknown object format '%s'",
                      name ? name : "");
        return NULL;
    }
    return archbfd_load(db, e, flags, diags);
}

bool archbfd_write_catalogue(const ArchBFD *db, DsymFormat fmt, const char *root,
                              DsymBuf *out, DsymDiags *diags) {
    if (!db || !out) return false;
    DsymArena *a = dsym_arena_new();
    if (!a) return false;
    DsymNode *m = dsym_node_map(a);
    DsymNode *list = dsym_node_seq(a);
    bool ok = m && list;
    ok = ok && dsym_map_put_ref(a, m, "schema", dsym_node_int(a, ARCHBFD_SCHEMA_VERSION));
    ok = ok && dsym_map_put_ref(a, m, "root", dsym_node_str_ref(a, root ? root : db->root_rel));
    ok = ok && dsym_map_put_ref(a, m, "formats", list);
    for (size_t i = 0; ok && i < db->count; i++) {
        const ArchBFDEntry *e = &db->entries[i];
        DsymNode *it = dsym_node_map(a);
        ok = it && dsym_seq_push(a, list, it);
#define PUT_S(k, v)                                                            \
    if (ok && (v)) ok = dsym_map_put_ref(a, it, k, dsym_node_str_ref(a, v));
        PUT_S("name", e->name)
        PUT_S("file", e->file)
        PUT_S("format", e->format)
        PUT_S("byte_order", e->byte_order)
        PUT_S("arch", e->arch)
        PUT_S("target_vector", e->target_vector)
#undef PUT_S
    }
    if (ok && fmt == DSYM_FORMAT_YAML)
        ok = dsym_buf_puts(out, "# ArchBFDCatalogue.yaml — index of object-format descriptions.\n"
                                "# Regenerate with: archbfd catalogue ArchBFD -o ArchBFDCatalogue.yaml\n");
    if (ok) ok = dsym_tree_write(fmt, m, out, DSYM_WRITE_NO_HEADER, diags);
    dsym_arena_free(a);
    return ok;
}

/* ======================================================================== */
/* Object files                                                              */
/* ======================================================================== */
/*
 * The reader is driven by the description: the byte layout of every
 * structure comes from the record fields, the section and symbol kinds come
 * from the constant groups and the relocation names come from the
 * relocation table.  Only the *structure* of the formats (where the symbol
 * table lives, how a relocation entry is split) is in the code.
 */

struct ArchBFDObject {
    DsymArena *arena;
    const ArchBFDSpec *spec; /* borrowed */
    uint8_t *data;
    size_t len;
    bool owns_data;
    char kind[16];
    int64_t machine;
    int64_t type;
    ArchBFDObjectSection *sections;
    size_t section_count;
    ArchBFDObjectSymbol *symbols;
    size_t symbol_count;
    ArchBFDObjectReloc *relocs;
    size_t reloc_count;
};

static bool little_endian(const ArchBFDSpec *spec) {
    return !spec || !spec->has_byte_order || spec->byte_order == ARCHBFD_BO_LITTLE;
}

static const ArchBFDField *find_field(const ArchBFDRecord *rec, const char *name) {
    if (!rec || !name) return NULL;
    for (size_t i = 0; i < rec->field_count; i++)
        if (rec->fields[i].name && !strcmp(rec->fields[i].name, name))
            return &rec->fields[i];
    return NULL;
}

/* Read the integer stored in a field of a record, at a base offset. */
static bool read_field_at(const uint8_t *data, size_t len, const ArchBFDRecord *rec,
                          const char *name, bool little, size_t base, uint64_t *out) {
    const ArchBFDField *f = find_field(rec, name);
    if (!f) return false;
    if (f->offset < 0 || f->size <= 0 || base > len ||
        (uint64_t)f->offset > len - base) return false;
    size_t off = base + (size_t)f->offset;
    if ((uint64_t)f->size > len - off) return false;
    uint64_t v = 0;
    const uint8_t *p = data + off;
    switch (f->size) {
    case 1: v = p[0]; break;
    case 2:
        if (little)
            v = (uint64_t)p[0] | ((uint64_t)p[1] << 8);
        else
            v = ((uint64_t)p[0] << 8) | (uint64_t)p[1];
        break;
    case 4:
        if (little) {
            v = (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
                ((uint64_t)p[3] << 24);
        } else {
            v = ((uint64_t)p[0] << 24) | ((uint64_t)p[1] << 16) | ((uint64_t)p[2] << 8) |
                (uint64_t)p[3];
        }
        break;
    case 8:
        if (little) {
            for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
        } else {
            for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
        }
        break;
    default: return false;
    }
    *out = v;
    return true;
}

/* Read the integer stored in a field of a record. */
static bool read_field(const uint8_t *data, size_t len, const ArchBFDRecord *rec,
                       const char *name, bool little, uint64_t *out) {
    return read_field_at(data, len, rec, name, little, 0, out);
}

/* Apply the encodings of a record to a raw value: the target names the
   part that is wanted ("type", "symbol", "bind", ...).  The value is shifted
   first and masked afterwards, so a mask always describes the width of the
   extracted part. */
static bool split_encoded(const ArchBFDRecord *rec, const char *target, uint64_t raw,
                          uint64_t *out) {
    for (size_t i = 0; i < rec->encoding_count; i++) {
        const ArchBFDEncoding *e = &rec->encodings[i];
        if (!e->target || !target || strcmp(e->target, target)) continue;
        if (e->shift < 0 || e->shift >= 64) return false;
        uint64_t v = raw >> (e->shift > 0 ? (uint64_t)e->shift : 0);
        *out = v & e->mask;
        return true;
    }
    return false;
}

static const char *constant_name(const ArchBFDSpec *spec, const char *group, int64_t value) {
    const ArchBFDConstantGroup *grp = NULL;
    if (spec && spec->index) {
        StrEntry *g = index_find(spec->index->groups, spec->index->group_cap, group);
        if (g) grp = &spec->constants[g->index];
    }
    if (!grp && spec && spec->container_spec) {
        const ArchBFDSpec *cs = spec->container_spec;
        if (cs->index) {
            StrEntry *g = index_find(cs->index->groups, cs->index->group_cap, group);
            if (g) grp = &cs->constants[g->index];
        }
    }
    if (!grp) return NULL;
    for (size_t i = 0; i < grp->count; i++)
        if (grp->items[i].value == value) return grp->items[i].name;
    return NULL;
}

static int64_t constant_value(const ArchBFDSpec *spec, const char *group, const char *name) {
    int64_t v = 0;
    if (archbfd_resolve_constant(spec, group, name, &v)) return v;
    return -1;
}

static char *dup_string(DsymArena *a, const char *s, size_t len) {
    if (!s) return dsym_arena_strdup(a, "");
    const char *nul = memchr(s, 0, len);
    size_t n = nul ? (size_t)(nul - s) : len;
    return dsym_arena_strndup(a, s, n);
}

static bool check_magic(const ArchBFDSpec *spec, const uint8_t *data, size_t len,
                         DsymDiags *diags) {
    if (!spec->magic || !spec->magic_len) return true;
    if (len < spec->magic_len) {
        dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                      "the file is too short to hold the %s magic", spec->name);
        return false;
    }
    for (size_t i = 0; i < spec->magic_len; i++) {
        char buf[3] = {spec->magic[i * 2], spec->magic[i * 2 + 1], 0};
        unsigned long byte = strtoul(buf, NULL, 16);
        if (data[i] != (uint8_t)byte) {
            dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                          "bad magic: expected %s, found %02x", spec->magic, data[i]);
            return false;
        }
    }
    return true;
}

size_t archbfd_magic_bytes(const ArchBFDSpec *s, uint8_t *out, size_t cap) {
    if (!s || !s->magic || !s->magic_len) return 0;
    if (cap < s->magic_len) return 0;
    for (size_t i = 0; i < s->magic_len; i++) {
        char buf[3] = {s->magic[i * 2], s->magic[i * 2 + 1], 0};
        out[i] = (uint8_t)strtoul(buf, NULL, 16);
    }
    return s->magic_len;
}

/* ---- ELF ---------------------------------------------------------------- */

static bool elf_error(const ArchBFDObject *obj, DsymDiags *diags,
                      size_t section, const char *message) {
    dsym_diag_add(diags, DSYM_ERROR, obj->spec->source_path, 0, 0, NULL,
                  "ELF section %zu: %s", section, message);
    return false;
}

static const char *elf_string(ArchBFDObject *obj, DsymDiags *diags,
                             size_t section, const char *strings,
                             size_t length, uint64_t offset) {
    if (offset >= length || !memchr(strings + (size_t)offset, 0,
                                   length - (size_t)offset)) {
        elf_error(obj, diags, section, "invalid or unterminated string table reference");
        return NULL;
    }
    const char *s = dsym_arena_strdup(obj->arena, strings + (size_t)offset);
    if (!s) elf_error(obj, diags, section, "out of memory reading string table");
    return s;
}

static bool read_elf(ArchBFDObject *obj, DsymDiags *diags) {
    const ArchBFDSpec *spec = obj->spec;
    const uint8_t *data = obj->data;
    size_t len = obj->len;
    bool little = little_endian(spec);
    const ArchBFDRecord *ehdr = archbfd_resolve_record(spec, "ehdr");
    const ArchBFDRecord *shdr = archbfd_resolve_record(spec, "shdr");
    const ArchBFDRecord *symrec = archbfd_resolve_record(spec, "sym");
    if (!ehdr || !shdr || !symrec || ehdr->size <= 0 ||
        shdr->size <= 0 || symrec->size <= 0) {
        dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                      "the ELF description has no ehdr/shdr/sym record");
        return false;
    }
    if (len < 16 || (uint64_t)ehdr->size > len)
        return elf_error(obj, diags, 0, "incomplete ELF header");
    if ((data[5] != 1 && data[5] != 2) || data[6] != 1)
        return elf_error(obj, diags, 0, "invalid ELF byte order or identification version");
    /* e_ident carries the class (byte 4) and the byte order (byte 5). */
    if (len >= 6) {
        int expected_class = ehdr->size == 64 ? 2 : 1; /* ELFCLASS64 / ELFCLASS32 */
        if (data[4] != (uint8_t)expected_class) {
            dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                          "the object is ELFCLASS%d but the description is for ELFCLASS%d",
                          data[4], expected_class);
            return false;
        }
        if (data[5] == 1 && !little) {
            dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                          "the object is little endian but the description is big endian");
            return false;
        }
        if (data[5] == 2 && little) {
            dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                          "the object is big endian but the description is little endian");
            return false;
        }
    }
    uint64_t shoff = 0, shnum = 0, shentsize = 0, shstrndx = 0, etype = 0, emachine = 0;
    if (!read_field(data, len, ehdr, "e_shoff", little, &shoff) ||
        !read_field(data, len, ehdr, "e_shnum", little, &shnum) ||
        !read_field(data, len, ehdr, "e_shentsize", little, &shentsize) ||
        !read_field(data, len, ehdr, "e_shstrndx", little, &shstrndx) ||
        !read_field(data, len, ehdr, "e_type", little, &etype) ||
        !read_field(data, len, ehdr, "e_machine", little, &emachine)) {
        dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                      "the ELF file header is incomplete");
        return false;
    }
    obj->machine = (int64_t)emachine;
    obj->type = (int64_t)etype;
    uint64_t version = 0, header_size = 0;
    if (!read_field(data, len, ehdr, "e_version", little, &version) || version != 1 ||
        !read_field(data, len, ehdr, "e_ehsize", little, &header_size) ||
        header_size != (uint64_t)ehdr->size)
        return elf_error(obj, diags, 0, "invalid ELF version or header size");
    if (shstrndx >= shnum && shstrndx != 0)
        return elf_error(obj, diags, 0, "invalid or unsupported extended section name index");
    if (shnum == 0 || shoff == 0) {
        dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                      "the object has no section header table");
        return false;
    }
    if (shentsize != (uint64_t)shdr->size) {
        dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                      "e_shentsize is %llu but the description says %" PRId64,
                      (unsigned long long)shentsize, shdr->size);
        return false;
    }
    /* ArchLD security hardening: never multiply attacker-controlled ELF counts
       before proving the complete span lies inside the input. */
    if (shoff > len || shentsize == 0 || shnum > (len - shoff) / shentsize) {
        dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                      "the section header table is outside the file");
        return false;
    }
    obj->sections = dsym_arena_calloc(obj->arena, (size_t)shnum, sizeof *obj->sections);
    if (!obj->sections) return false;
    obj->section_count = (size_t)shnum;
    for (size_t i = 0; i < obj->section_count; i++) {
        const uint8_t *p = data + shoff + i * shentsize;
        ArchBFDObjectSection *sec = &obj->sections[i];
        uint64_t v = 0;
        sec->index = i;
        sec->name = "";
        if (read_field(p, (size_t)shentsize, shdr, "sh_type", little, &v)) sec->type = (int64_t)v;
        if (read_field(p, (size_t)shentsize, shdr, "sh_flags", little, &v)) sec->flags = v;
        if (read_field(p, (size_t)shentsize, shdr, "sh_addr", little, &v)) sec->addr = (int64_t)v;
        if (read_field(p, (size_t)shentsize, shdr, "sh_offset", little, &v)) sec->offset = (int64_t)v;
        if (read_field(p, (size_t)shentsize, shdr, "sh_size", little, &v)) sec->size = (int64_t)v;
        if (read_field(p, (size_t)shentsize, shdr, "sh_link", little, &v)) sec->link = (int64_t)v;
        if (read_field(p, (size_t)shentsize, shdr, "sh_info", little, &v)) sec->info = (int64_t)v;
        if (read_field(p, (size_t)shentsize, shdr, "sh_addralign", little, &v)) sec->align = (int64_t)v;
        if (read_field(p, (size_t)shentsize, shdr, "sh_entsize", little, &v)) sec->entsize = (int64_t)v;
        if (sec->size < 0 || sec->offset < 0 || sec->align < 0 || sec->entsize < 0 ||
            (sec->align && ((uint64_t)sec->align & ((uint64_t)sec->align - 1))))
            return elf_error(obj, diags, i, "invalid section size, offset or alignment");
        /* SHT_NULL has no payload; SHT_NOBITS occupies memory only. */
        if (sec->type != 0 && sec->type != 8 &&
            ((uint64_t)sec->offset > len ||
             (uint64_t)sec->size > len - (uint64_t)sec->offset))
            return elf_error(obj, diags, i, "section payload outside file");
        if (sec->type == 3 && sec->size &&
            (data[sec->offset] || data[sec->offset + sec->size - 1]))
            return elf_error(obj, diags, i, "string table must begin and end with NUL");
    }
    if (obj->sections[0].type != 0)
        return elf_error(obj, diags, 0, "first section must be SHT_NULL");
    /* Section names come from the section header string table. */
    if (shstrndx != 0) {
        const ArchBFDObjectSection *strsec = &obj->sections[shstrndx];
        if (strsec->type != 3)
            return elf_error(obj, diags, shstrndx, "section names must link to SHT_STRTAB");
        if (strsec->offset >= 0 && strsec->size >= 0 &&
            (uint64_t)strsec->offset <= len &&
            (uint64_t)strsec->size <= len - (uint64_t)strsec->offset) {
            const char *names = (const char *)data + strsec->offset;
            size_t names_len = (size_t)strsec->size;
            for (size_t i = 0; i < obj->section_count; i++) {
                uint64_t name_off = 0;
                const uint8_t *p = data + shoff + i * shentsize;
                if (!read_field(p, (size_t)shentsize, shdr, "sh_name", little, &name_off)) continue;
                obj->sections[i].name = elf_string(obj, diags, i, names, names_len, name_off);
                if (!obj->sections[i].name) return false;
            }
        }
    } else {
        for (size_t i = 0; i < obj->section_count; i++) {
            uint64_t name = 0;
            if (!read_field(data + shoff + i * shentsize, (size_t)shentsize,
                            shdr, "sh_name", little, &name) || name)
                return elf_error(obj, diags, i, "section name without a section string table");
        }
    }

    /* The symbol table. */
    int64_t symtab_type = constant_value(spec, "section_types", "SHT_SYMTAB");
    int64_t symtab_index = -1;
    for (size_t i = 0; i < obj->section_count; i++)
        if (symtab_type >= 0 && obj->sections[i].type == symtab_type) {
            if (symtab_index >= 0)
                return elf_error(obj, diags, i, "multiple symbol tables are unsupported");
            symtab_index = (int64_t)i;
        }
    const char *strdata = NULL;
    size_t strdata_len = 0;
    for (size_t i = 0; i < obj->section_count; i++) {
        const ArchBFDObjectSection *sec = &obj->sections[i];
        if (sec->type == 17 && (symtab_index < 0 || sec->link != symtab_index))
            return elf_error(obj, diags, i, "invalid group symbol table link");
    }
    if (symtab_index >= 0) {
        const ArchBFDObjectSection *symsec = &obj->sections[symtab_index];
        if (symsec->link <= 0 || (uint64_t)symsec->link >= obj->section_count ||
            obj->sections[symsec->link].type != 3)
            return elf_error(obj, diags, (size_t)symtab_index, "symbol table must link to SHT_STRTAB");
        if (symsec->link >= 0 && (size_t)symsec->link < obj->section_count) {
            const ArchBFDObjectSection *strsec = &obj->sections[symsec->link];
            if (strsec->offset >= 0 && strsec->size >= 0 &&
                (uint64_t)strsec->offset <= len &&
                (uint64_t)strsec->size <= len - (uint64_t)strsec->offset) {
                strdata = (const char *)data + strsec->offset;
                strdata_len = (size_t)strsec->size;
            }
        }
        int64_t entsize = symsec->entsize;
        if (symsec->offset < 0 || symsec->size < 0 ||
            (uint64_t)symsec->offset > len ||
            (uint64_t)symsec->size > len - (uint64_t)symsec->offset ||
            entsize != symrec->size || entsize <= 0 || symsec->size % entsize) {
            dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                          "ELF symbol table has invalid extent or entry size");
            return false;
        }
        size_t count = (size_t)(symsec->size / entsize);
        obj->symbols = dsym_arena_calloc(obj->arena, count ? count : 1, sizeof *obj->symbols);
        if (!obj->symbols) return false;
        for (size_t i = 0; i < count; i++) {
            const uint8_t *p = data + symsec->offset + i * (size_t)entsize;
            if (symsec->offset + (int64_t)(i * (size_t)entsize) + entsize > (int64_t)len) break;
            ArchBFDObjectSymbol *sym = &obj->symbols[obj->symbol_count++];
            sym->index = obj->symbol_count - 1;
            uint64_t v = 0;
            if (!read_field(p, (size_t)entsize, symrec, "st_name", little, &v))
                return elf_error(obj, diags, (size_t)symtab_index, "missing symbol name field");
            sym->name = elf_string(obj, diags, (size_t)symtab_index, strdata, strdata_len, v);
            if (!sym->name) return false;
            if (read_field(p, (size_t)entsize, symrec, "st_value", little, &v)) sym->value = (int64_t)v;
            if (read_field(p, (size_t)entsize, symrec, "st_size", little, &v)) sym->size = (int64_t)v;
            if (read_field(p, (size_t)entsize, symrec, "st_shndx", little, &v)) sym->shndx = (int64_t)v;
            if (read_field(p, (size_t)entsize, symrec, "st_info", little, &v)) sym->info = (uint8_t)v;
            if (read_field(p, (size_t)entsize, symrec, "st_other", little, &v)) sym->other = (uint8_t)v;
            if ((sym->shndx < 0xff00 && (uint64_t)sym->shndx >= shnum) ||
                sym->shndx == 0xffff)
                return elf_error(obj, diags, (size_t)symtab_index, "invalid or unsupported extended symbol section index");
            uint64_t bind = 0, type = 0;
            if (split_encoded(symrec, "bind", sym->info, &bind)) {
                sym->class_value = (int64_t)bind;
                sym->bind = constant_name(spec, "symbol_bindings", (int64_t)bind);
            }
            if (split_encoded(symrec, "type", sym->info, &type)) {
                sym->type_value = (int64_t)type;
                sym->type = constant_name(spec, "symbol_types", (int64_t)type);
            }
        }
    }

    /* The relocations. */
    int64_t rela_type = constant_value(spec, "section_types", "SHT_RELA");
    int64_t rel_type = constant_value(spec, "section_types", "SHT_REL");
    const ArchBFDRecord *relarec = archbfd_resolve_record(spec, "rela");
    const ArchBFDRecord *relrec = archbfd_resolve_record(spec, "rel");
    size_t total = 0;
    for (size_t i = 0; i < obj->section_count; i++) {
        const ArchBFDObjectSection *sec = &obj->sections[i];
        bool is_rela = rela_type >= 0 && sec->type == rela_type;
        bool is_rel = rel_type >= 0 && sec->type == rel_type;
        if (!is_rela && !is_rel) continue;
        const ArchBFDRecord *rec = is_rela ? relarec : relrec;
        if (!rec || rec->size <= 0)
            return elf_error(obj, diags, i, "missing relocation record description");
        if (symtab_index < 0 || sec->link != symtab_index ||
            sec->info < 0 || (uint64_t)sec->info >= shnum || (etype == 1 && sec->info == 0))
            return elf_error(obj, diags, i, "invalid relocation symbol table or target section link");
        int64_t entsize = sec->entsize;
        if (entsize <= 0 || entsize != rec->size || sec->offset < 0 ||
            sec->size < 0 || (uint64_t)sec->offset > len ||
            (uint64_t)sec->size > len - (uint64_t)sec->offset ||
            sec->size % entsize) {
            dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                          "ELF relocation section has invalid extent or entry size");
            return false;
        }
        size_t nr = (size_t)(sec->size / entsize);
        if (nr > SIZE_MAX - total) return false;
        total += nr;
    }
    obj->relocs = dsym_arena_calloc(obj->arena, total ? total : 1, sizeof *obj->relocs);
    if (!obj->relocs) return false;
    for (size_t i = 0; i < obj->section_count; i++) {
        const ArchBFDObjectSection *sec = &obj->sections[i];
        bool is_rela = rela_type >= 0 && sec->type == rela_type;
        bool is_rel = rel_type >= 0 && sec->type == rel_type;
        if (!is_rela && !is_rel) continue;
        const ArchBFDRecord *rec = is_rela ? relarec : relrec;
        if (!rec) continue;
        int64_t entsize = sec->entsize ? sec->entsize : rec->size;
        if (entsize <= 0) continue;
        size_t count = (size_t)(sec->size / entsize);
        for (size_t k = 0; k < count; k++) {
            size_t off = (size_t)sec->offset + k * (size_t)entsize;
            if (off + (size_t)entsize > len) break;
            const uint8_t *p = data + off;
            ArchBFDObjectReloc *rel = &obj->relocs[obj->reloc_count++];
            rel->index = obj->reloc_count - 1;
            rel->source_section = i;
            rel->target_section = (size_t)sec->info;
            rel->has_addend = is_rela;
            rel->symbol = -1;
            uint64_t v = 0;
            if (read_field(p, (size_t)entsize, rec, "r_offset", little, &v))
                rel->offset = (int64_t)v;
            if (read_field(p, (size_t)entsize, rec, "r_addend", little, &v)) {
                /* GABI 6.1: Elf32_Rela uses Elf32_Sword, not Elf32_Word. */
                const ArchBFDField *addend = find_field(rec, "r_addend");
                rel->addend = addend->size == 4 && (v & UINT64_C(0x80000000))
                    ? (int64_t)v - INT64_C(0x100000000) : (int64_t)v;
            }
            uint64_t info = 0;
            if (read_field(p, (size_t)entsize, rec, "r_info", little, &info)) {
                uint64_t type = 0, sym = 0;
                if (split_encoded(rec, "type", info, &type)) rel->type = (int64_t)type;
                if (split_encoded(rec, "symbol", info, &sym)) rel->symbol = (int64_t)sym;
            }
            if (rel->symbol < 0 || (uint64_t)rel->symbol >= obj->symbol_count)
                return elf_error(obj, diags, i, "relocation symbol index outside symbol table");
            int32_t ri = archbfd_find_reloc_number(spec, rel->type);
            if (ri >= 0) rel->type_name = spec->relocations[ri].name;
            if (rel->symbol >= 0 && (size_t)rel->symbol < obj->symbol_count)
                rel->symbol_name = obj->symbols[rel->symbol].name;
        }
    }
    snprintf(obj->kind, sizeof obj->kind, "elf%lld",
             (unsigned long long)(len >= 5 && data[4] == 2 ? 64 : 32));
    return true;
}

/* ---- COFF and PE --------------------------------------------------------- */

static bool read_coff(ArchBFDObject *obj, DsymDiags *diags, bool pe) {
    const ArchBFDSpec *spec = obj->spec;
    const uint8_t *data = obj->data;
    size_t len = obj->len;
    bool little = little_endian(spec);
    const ArchBFDRecord *filehdr = archbfd_resolve_record(spec, "filehdr");
    const ArchBFDRecord *scnhdr = archbfd_resolve_record(spec, "scnhdr");
    const ArchBFDRecord *syment = archbfd_resolve_record(spec, "syment");
    const ArchBFDRecord *relrec = archbfd_resolve_record(spec, "reloc");
    if (!filehdr || !scnhdr || !syment) {
        dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                      "the COFF description has no filehdr/scnhdr/syment record");
        return false;
    }
    uint64_t nscns = 0, symptr = 0, nsyms = 0, opthdr = 0;
    /* A PE image keeps the COFF header behind the NT signature, so the
       fields are read relative to that header. */
    size_t base = 0;
    size_t table = 0;
    if (pe) {
        uint64_t lfanew = 0;
        if (!read_field(data, len, filehdr, "e_lfanew", little, &lfanew)) {
            dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                          "the PE file header has no e_lfanew");
            return false;
        }
        const ArchBFDField *fmagic = find_field(filehdr, "f_magic");
        if (!fmagic) {
            dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                          "the PE file header has no COFF header");
            return false;
        }
        base = (size_t)lfanew + 4 - (size_t)fmagic->offset;
        if (getenv("ARCHBFD_DEBUG"))
            fprintf(stderr, "DEBUG pe lfanew=%llu fmagic_off=%lld base=%zu len=%zu\n",
                    (unsigned long long)lfanew, (long long)fmagic->offset, base, len);
        if (base > len) {
            dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                          "the NT header is outside the file");
            return false;
        }
    }
    if (!read_field_at(data, len, filehdr, "f_nscns", little, base, &nscns) ||
        !read_field_at(data, len, filehdr, "f_symptr", little, base, &symptr) ||
        !read_field_at(data, len, filehdr, "f_nsyms", little, base, &nsyms) ||
        !read_field_at(data, len, filehdr, "f_opthdr", little, base, &opthdr)) {
        dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                      "the COFF file header is incomplete");
        return false;
    }
    if (pe)
        table = base + (size_t)find_field(filehdr, "f_magic")->offset + 20 + opthdr;
    else
        table = (uint64_t)filehdr->size + opthdr;
    if (table + nscns * (uint64_t)scnhdr->size > len) {
        dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                      "the COFF section table is outside the file");
        return false;
    }
    obj->sections = dsym_arena_calloc(obj->arena, (size_t)nscns ? (size_t)nscns : 1,
                                       sizeof *obj->sections);
    if (!obj->sections) return false;
    obj->section_count = (size_t)nscns;
    for (size_t i = 0; i < obj->section_count; i++) {
        const uint8_t *p = data + table + i * (size_t)scnhdr->size;
        ArchBFDObjectSection *sec = &obj->sections[i];
        uint64_t v = 0;
        sec->index = i;
        sec->name = "";
        const ArchBFDField *name_field = find_field(scnhdr, "s_name");
        if (name_field && name_field->size) {
            size_t n = (size_t)name_field->size;
            if (n > 8) n = 8;
            const char *nm = (const char *)p + name_field->offset;
            size_t len_nm = 0;
            while (len_nm < n && nm[len_nm]) len_nm++;
            sec->name = dup_string(obj->arena, nm, len_nm);
        }
        if (read_field(p, (size_t)scnhdr->size, scnhdr, "s_vaddr", little, &v))
            sec->addr = (int64_t)v;
        if (read_field(p, (size_t)scnhdr->size, scnhdr, "s_size", little, &v))
            sec->size = (int64_t)v;
        if (read_field(p, (size_t)scnhdr->size, scnhdr, "s_scnptr", little, &v))
            sec->offset = (int64_t)v;
        if (read_field(p, (size_t)scnhdr->size, scnhdr, "s_relptr", little, &v))
            sec->rel_offset = (int64_t)v;
        if (read_field(p, (size_t)scnhdr->size, scnhdr, "s_nreloc", little, &v))
            sec->rel_count = (int64_t)v;
        if (read_field(p, (size_t)scnhdr->size, scnhdr, "s_flags", little, &v))
            sec->flags = v;
    }
    /* The symbol table. */
    if (nsyms && symptr && symptr + nsyms * (uint64_t)syment->size <= len) {
        obj->symbols = dsym_arena_calloc(obj->arena, (size_t)nsyms, sizeof *obj->symbols);
        if (!obj->symbols) return false;
        const uint8_t *symtab = data + symptr;
        size_t strtab_off = (size_t)(symptr + nsyms * (uint64_t)syment->size);
        size_t strtab_len = len > strtab_off ? len - strtab_off : 0;
        for (size_t i = 0; i < (size_t)nsyms; i++) {
            const uint8_t *p = symtab + i * (size_t)syment->size;
            ArchBFDObjectSymbol *sym = &obj->symbols[obj->symbol_count++];
            sym->index = obj->symbol_count - 1;
            sym->name = "";
            uint64_t v = 0;
            const ArchBFDField *name_field = find_field(syment, "e_name");
            if (name_field && name_field->size >= 8) {
                const char *nm = (const char *)p + name_field->offset;
                if (nm[0] == 0 && nm[1] == 0 && nm[2] == 0 && nm[3] == 0) {
                    /* The name is in the string table that follows the
                       symbol table. */
                    uint64_t off = 0;
                    if (read_field(p, (size_t)syment->size, syment, "e_offset", little, &off) &&
                        off < strtab_len)
                        sym->name = dup_string(obj->arena, (const char *)data + strtab_off + off,
                                               strtab_len - (size_t)off);
                } else {
                    sym->name = dup_string(obj->arena, nm, 8);
                }
            }
            if (read_field(p, (size_t)syment->size, syment, "e_value", little, &v))
                sym->value = (int64_t)v;
            if (read_field(p, (size_t)syment->size, syment, "e_scnum", little, &v))
                sym->shndx = (int64_t)v;
            if (read_field(p, (size_t)syment->size, syment, "e_type", little, &v))
                sym->type_value = (int64_t)v;
            if (read_field(p, (size_t)syment->size, syment, "e_sclass", little, &v)) {
                sym->class_value = (int64_t)v;
                sym->other = (uint8_t)v;
            }
        }
    }
    /* The relocations live in the sections. */
    if (relrec) {
        size_t total = 0;
        for (size_t i = 0; i < obj->section_count; i++)
            if (obj->sections[i].rel_offset > 0) total += (size_t)obj->sections[i].rel_count;
        obj->relocs = dsym_arena_calloc(obj->arena, total ? total : 1, sizeof *obj->relocs);
        if (!obj->relocs) return false;
        for (size_t i = 0; i < obj->section_count; i++) {
            const ArchBFDObjectSection *sec = &obj->sections[i];
            if (sec->rel_offset <= 0 || sec->rel_count <= 0) continue;
            for (int64_t k = 0; k < sec->rel_count; k++) {
                size_t off = (size_t)sec->rel_offset + (size_t)k * (size_t)relrec->size;
                if (off + (size_t)relrec->size > len) break;
                const uint8_t *p = data + off;
                ArchBFDObjectReloc *rel = &obj->relocs[obj->reloc_count++];
                rel->index = obj->reloc_count - 1;
                rel->source_section = SIZE_MAX;
                rel->target_section = i;
                rel->symbol = -1;
                uint64_t v = 0;
                if (read_field(p, (size_t)relrec->size, relrec, "r_vaddr", little, &v))
                    rel->offset = (int64_t)v;
                if (read_field(p, (size_t)relrec->size, relrec, "r_symndx", little, &v))
                    rel->symbol = (int64_t)v;
                if (read_field(p, (size_t)relrec->size, relrec, "r_type", little, &v))
                    rel->type = (int64_t)v;
                int32_t ri = archbfd_find_reloc_number(spec, rel->type);
                if (ri >= 0) rel->type_name = spec->relocations[ri].name;
                if (rel->symbol >= 0 && (size_t)rel->symbol < obj->symbol_count)
                    rel->symbol_name = obj->symbols[rel->symbol].name;
            }
        }
    }
    snprintf(obj->kind, sizeof obj->kind, "%s", pe ? "pe" : "coff");
    return true;
}

ArchBFDObject *archbfd_object_open_memory(const void *data, size_t len,
                                           const ArchBFDSpec *spec, DsymDiags *diags) {
    if (!data || !spec) return NULL;
    ArchBFDObject *obj = calloc(1, sizeof *obj);
    if (!obj) return NULL;
    obj->arena = dsym_arena_new();
    obj->spec = spec;
    obj->len = len;
    obj->data = malloc(len ? len : 1);
    if (!obj->arena || !obj->data) {
        archbfd_object_close(obj);
        return NULL;
    }
    memcpy(obj->data, data, len);
    obj->owns_data = true;
    if (!check_magic(spec, obj->data, len, diags)) {
        archbfd_object_close(obj);
        return NULL;
    }
    bool ok = false;
    if (spec->has_format) {
        switch (spec->format) {
        case ARCHBFD_FMT_ELF: ok = read_elf(obj, diags); break;
        case ARCHBFD_FMT_COFF: ok = read_coff(obj, diags, false); break;
        case ARCHBFD_FMT_PE: ok = read_coff(obj, diags, true); break;
        default:
            dsym_diag_add(diags, DSYM_ERROR, spec->source_path, 0, 0, NULL,
                          "the %s object format is not supported by the reader",
                          archbfd_format_name(spec->format));
            break;
        }
    }
    if (!ok) {
        archbfd_object_close(obj);
        return NULL;
    }
    return obj;
}

ArchBFDObject *archbfd_object_open(const char *path, const ArchBFDSpec *spec,
                                    DsymDiags *diags) {
    if (!path || !spec) return NULL;
    DsymBuf buf;
    dsym_buf_init(&buf);
    if (!dsym_buf_read_file(&buf, path)) {
        dsym_diag_add(diags, DSYM_ERROR, path, 0, 0, NULL, "cannot read: %s", strerror(errno));
        dsym_buf_free(&buf);
        return NULL;
    }
    ArchBFDObject *obj = archbfd_object_open_memory(buf.data, buf.len, spec, diags);
    dsym_buf_free(&buf);
    return obj;
}

void archbfd_object_close(ArchBFDObject *obj) {
    if (!obj) return;
    if (obj->owns_data) free(obj->data);
    dsym_arena_free(obj->arena);
    free(obj);
}

size_t archbfd_object_section_count(const ArchBFDObject *obj) {
    return obj ? obj->section_count : 0;
}

const ArchBFDObjectSection *archbfd_object_section(const ArchBFDObject *obj, size_t i) {
    if (!obj || i >= obj->section_count) return NULL;
    return &obj->sections[i];
}

size_t archbfd_object_symbol_count(const ArchBFDObject *obj) {
    return obj ? obj->symbol_count : 0;
}

const ArchBFDObjectSymbol *archbfd_object_symbol(const ArchBFDObject *obj, size_t i) {
    if (!obj || i >= obj->symbol_count) return NULL;
    return &obj->symbols[i];
}

size_t archbfd_object_reloc_count(const ArchBFDObject *obj) {
    return obj ? obj->reloc_count : 0;
}

const ArchBFDObjectReloc *archbfd_object_reloc(const ArchBFDObject *obj, size_t i) {
    if (!obj || i >= obj->reloc_count) return NULL;
    return &obj->relocs[i];
}

const char *archbfd_object_kind(const ArchBFDObject *obj) {
    return obj ? obj->kind : "";
}

int64_t archbfd_object_machine(const ArchBFDObject *obj) {
    return obj ? obj->machine : 0;
}

int64_t archbfd_object_type(const ArchBFDObject *obj) {
    return obj ? obj->type : 0;
}
