/*
 * ToJSON.c — JSON codec for the MetaArch document tree, built on yyjson
 * (https://github.com/ibireme/yyjson): RFC 8259 compliant, validates UTF-8,
 * full 64-bit integer range, and the fastest C JSON reader/writer available.
 *
 * The writer builds a yyjson mutable document that *references* the tree's
 * strings (no copies) and serialises it in one pass.
 */
#include "DSymspec.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

/* ---- reading ----------------------------------------------------------- */

static DsymNode *from_yy(DsymArena *a, yyjson_val *v, int depth, bool *oom) {
    if (depth > 512) { /* yyjson itself has no depth limit; keep ours sane */
        *oom = true;
        return NULL;
    }
    DsymNode *n = NULL;
    switch (yyjson_get_type(v)) {
    case YYJSON_TYPE_NULL: n = dsym_node_null(a); break;
    case YYJSON_TYPE_BOOL: n = dsym_node_bool(a, yyjson_get_bool(v)); break;
    case YYJSON_TYPE_NUM:
        if (yyjson_is_uint(v)) n = dsym_node_uint(a, yyjson_get_uint(v));
        else if (yyjson_is_sint(v)) n = dsym_node_int(a, yyjson_get_sint(v));
        else n = dsym_node_real(a, yyjson_get_real(v));
        break;
    case YYJSON_TYPE_STR:
        n = dsym_node_strn(a, yyjson_get_str(v), yyjson_get_len(v));
        if (n) n->hints |= DSYM_HINT_QUOTED;
        break;
    case YYJSON_TYPE_ARR: {
        n = dsym_node_seq(a);
        if (!n) break;
        size_t cnt = yyjson_arr_size(v);
        n->v.seq.items = dsym_arena_calloc(a, cnt ? cnt : 1, sizeof(DsymNode *));
        n->v.seq.cap = cnt ? cnt : 1;
        if (!n->v.seq.items) return *oom = true, NULL;
        yyjson_val *it;
        yyjson_arr_iter iter = yyjson_arr_iter_with(v);
        while ((it = yyjson_arr_iter_next(&iter))) {
            DsymNode *c = from_yy(a, it, depth + 1, oom);
            if (!c) return NULL;
            n->v.seq.items[n->v.seq.count++] = c;
        }
        break;
    }
    case YYJSON_TYPE_OBJ: {
        n = dsym_node_map(a);
        if (!n) break;
        size_t cnt = yyjson_obj_size(v);
        n->v.map.items = dsym_arena_calloc(a, cnt ? cnt : 1, sizeof(DsymPair));
        n->v.map.cap = cnt ? cnt : 1;
        if (!n->v.map.items) return *oom = true, NULL;
        yyjson_val *k;
        yyjson_obj_iter iter = yyjson_obj_iter_with(v);
        while ((k = yyjson_obj_iter_next(&iter))) {
            DsymPair *p = &n->v.map.items[n->v.map.count];
            p->key = dsym_arena_strndup(a, yyjson_get_str(k), yyjson_get_len(k));
            p->key_len = yyjson_get_len(k);
            p->value = from_yy(a, yyjson_obj_iter_get_val(k), depth + 1, oom);
            if (!p->key || !p->value) return *oom = true, NULL;
            n->v.map.count++;
        }
        break;
    }
    default: break;
    }
    if (!n) *oom = true;
    return n;
}

DsymNode *dsym_json_read(DsymArena *a, const char *data, size_t len,
                         const char *file, DsymDiags *diags) {
    /* Skip a UTF-8 BOM, which RFC 8259 permits parsers to ignore. */
    if (len >= 3 && (unsigned char)data[0] == 0xEF &&
        (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF) {
        data += 3;
        len -= 3;
    }
    yyjson_read_err err;
    yyjson_doc *doc =
        yyjson_read_opts((char *)data, len, YYJSON_READ_NOFLAG, NULL, &err);
    if (!doc) {
        size_t line = 0, col = 0, chr = 0;
        yyjson_locate_pos(data, len, err.pos, &line, &col, &chr);
        dsym_diag_add(diags, DSYM_ERROR, file, (uint32_t)line, (uint32_t)col,
                      NULL, "JSON: %s", err.msg);
        return NULL;
    }
    bool oom = false;
    DsymNode *root = from_yy(a, yyjson_doc_get_root(doc), 0, &oom);
    yyjson_doc_free(doc);
    if (!root)
        dsym_diag_add(diags, DSYM_ERROR, file, 0, 0, NULL,
                      oom ? "JSON: out of memory or nesting too deep"
                          : "JSON: empty document");
    return root;
}

/* ---- writing ----------------------------------------------------------- */

static yyjson_mut_val *to_yy(yyjson_mut_doc *d, const DsymNode *n,
                             const char **err) {
    switch (n->kind) {
    case DSYM_NODE_NULL: return yyjson_mut_null(d);
    case DSYM_NODE_BOOL: return yyjson_mut_bool(d, n->v.b);
    case DSYM_NODE_INT: return yyjson_mut_sint(d, n->v.i);
    case DSYM_NODE_UINT: return yyjson_mut_uint(d, n->v.u);
    case DSYM_NODE_REAL:
        if (!isfinite(n->v.r)) {
            *err = "JSON cannot represent infinity or NaN";
            return NULL;
        }
        return yyjson_mut_real(d, n->v.r);
    case DSYM_NODE_STR: return yyjson_mut_strn(d, n->v.str.ptr, n->v.str.len);
    case DSYM_NODE_SEQ: {
        yyjson_mut_val *arr = yyjson_mut_arr(d);
        for (size_t i = 0; arr && i < n->v.seq.count; i++) {
            yyjson_mut_val *c = to_yy(d, n->v.seq.items[i], err);
            if (!c || !yyjson_mut_arr_append(arr, c)) return NULL;
        }
        return arr;
    }
    case DSYM_NODE_MAP: {
        yyjson_mut_val *obj = yyjson_mut_obj(d);
        for (size_t i = 0; obj && i < n->v.map.count; i++) {
            const DsymPair *p = &n->v.map.items[i];
            yyjson_mut_val *k = yyjson_mut_strn(d, p->key, p->key_len);
            yyjson_mut_val *c = to_yy(d, p->value, err);
            if (!k || !c || !yyjson_mut_obj_add(obj, k, c)) return NULL;
        }
        return obj;
    }
    }
    return NULL;
}

bool dsym_json_write(const DsymNode *root, DsymBuf *out, uint32_t flags,
                     DsymDiags *diags) {
    yyjson_mut_doc *d = yyjson_mut_doc_new(NULL);
    if (!d) return false;
    const char *err = NULL;
    yyjson_mut_val *r = to_yy(d, root, &err);
    if (!r) {
        dsym_diag_add(diags, DSYM_ERROR, NULL, 0, 0, NULL, "JSON: %s",
                      err ? err : "out of memory");
        yyjson_mut_doc_free(d);
        return false;
    }
    yyjson_mut_doc_set_root(d, r);
    yyjson_write_flag wf = YYJSON_WRITE_NEWLINE_AT_END;
    if (!(flags & DSYM_WRITE_COMPACT))
        wf |= YYJSON_WRITE_PRETTY_TWO_SPACES;
    size_t len = 0;
    yyjson_write_err werr;
    char *s = yyjson_mut_write_opts(d, wf, NULL, &len, &werr);
    bool ok = s != NULL;
    if (ok)
        ok = dsym_buf_append(out, s, len);
    else
        dsym_diag_add(diags, DSYM_ERROR, NULL, 0, 0, NULL, "JSON: %s", werr.msg);
    free(s);
    yyjson_mut_doc_free(d);
    return ok;
}
