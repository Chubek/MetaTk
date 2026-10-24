/*
 * ArchDB.c — architecture catalogue (see ArchDB.h).
 */
#include "ArchDB.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct ArchDB {
    DsymArena *arena;
    const char *root;     /* directory holding the spec files */
    const char *root_rel; /* root as written in the catalogue */
    ArchDBEntry *entries;
    size_t count, cap;
    DsymSpec **cache;
};

static ArchDB *db_new(void) {
    DsymArena *a = dsym_arena_new();
    if (!a) return NULL;
    ArchDB *db = dsym_arena_alloc(a, sizeof *db);
    if (!db) {
        dsym_arena_free(a);
        return NULL;
    }
    db->arena = a;
    return db;
}

void archdb_close(ArchDB *db) {
    if (!db) return;
    if (db->cache)
        for (size_t i = 0; i < db->count; i++) dsym_spec_free(db->cache[i]);
    free(db->cache);
    free(db->entries);
    dsym_arena_free(db->arena);
}

static ArchDBEntry *db_add(ArchDB *db) {
    if (db->count == db->cap) {
        size_t nc = db->cap ? db->cap * 2 : 32;
        ArchDBEntry *p = realloc(db->entries, nc * sizeof *p);
        if (!p) return NULL;
        db->entries = p;
        db->cap = nc;
    }
    ArchDBEntry *e = &db->entries[db->count++];
    memset(e, 0, sizeof *e);
    return e;
}

static bool db_finish(ArchDB *db) {
    db->cache = calloc(db->count ? db->count : 1, sizeof *db->cache);
    return db->cache != NULL;
}

static const char *join(DsymArena *a, const char *dir, const char *rel) {
    if (!dir || !*dir || !strcmp(dir, ".") || rel[0] == '/') return dsym_arena_strdup(a, rel);
    size_t L = strlen(dir);
    return dsym_arena_printf(a, "%s%s%s", dir, dir[L - 1] == '/' ? "" : "/", rel);
}

static const char *dirname_of(DsymArena *a, const char *path) {
    const char *s = strrchr(path, '/');
    if (!s) return ".";
    if (s == path) return "/";
    return dsym_arena_strndup(a, path, (size_t)(s - path));
}

static int cmp_str(const void *x, const void *y) {
    return strcmp(*(const char *const *)x, *(const char *const *)y);
}

static void fill_summary(ArchDB *db, ArchDBEntry *e, const DsymSpec *s) {
    if (!s || !s->target_count) return;
    const DsymTarget *t = &s->targets[0];
    e->arch = dsym_arena_strdup(db->arena, t->arch);
    e->subarch = t->subarch ? dsym_arena_strdup(db->arena, t->subarch) : NULL;
    e->addr_size = t->addr_size;
    e->byte_order = dsym_byte_order_name(t->byte_order);
    e->obj_format = t->obj_format ? dsym_arena_strdup(db->arena, t->obj_format) : NULL;
    if (s->backend && s->backend->file)
        e->backend = dsym_arena_strdup(db->arena, s->backend->file);
}

ArchDB *archdb_scan(const char *dir, DsymDiags *diags) {
    DIR *d = opendir(dir);
    if (!d) {
        dsym_diag_add(diags, DSYM_ERROR, dir, 0, 0, NULL, "cannot open directory: %s",
                      strerror(errno));
        return NULL;
    }
    ArchDB *db = db_new();
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
        const char *m = strstr(de->d_name, ".dsymspec.");
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
        size_t stem = (size_t)(strstr(fn, ".dsymspec.") - fn);
        bool dup = false;
        for (size_t k = 0; k < db->count; k++)
            if (strlen(db->entries[k].name) == stem && !strncmp(db->entries[k].name, fn, stem))
                dup = true; /* same arch in several formats: first (sorted) wins */
        if (dup) continue;
        ArchDBEntry *e = db_add(db);
        if (!e) {
            ok = false;
            break;
        }
        e->name = dsym_arena_strndup(db->arena, fn, stem);
        e->file = fn;
        e->path = join(db->arena, dir, fn);
        DsymSpec *s = dsym_spec_load_file(e->path, DSYM_FORMAT_AUTO, DSYM_LOAD_NO_COMPILE, diags);
        if (!s) ok = false;
        fill_summary(db, e, s);
        dsym_spec_free(s);
    }
    free(names);
    if (!ok || !db_finish(db)) {
        archdb_close(db);
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

ArchDB *archdb_open(const char *path, DsymDiags *diags) {
    struct stat st;
    if (stat(path, &st) != 0) {
        dsym_diag_add(diags, DSYM_ERROR, path, 0, 0, NULL, "%s", strerror(errno));
        return NULL;
    }
    if (S_ISDIR(st.st_mode)) return archdb_scan(path, diags);

    DsymBuf buf;
    dsym_buf_init(&buf);
    if (!dsym_buf_read_file(&buf, path)) {
        dsym_diag_add(diags, DSYM_ERROR, path, 0, 0, NULL, "cannot read: %s", strerror(errno));
        return NULL;
    }
    ArchDB *db = db_new();
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
        if (!sch || sch->kind != DSYM_NODE_INT || sch->v.i != ARCHDB_CATALOGUE_SCHEMA) {
            dsym_diag_add(diags, DSYM_ERROR, path, sch ? sch->line : 0, 0, "/schema",
                          "catalogue schema must be %d", ARCHDB_CATALOGUE_SCHEMA);
            ok = false;
        }
        for (size_t i = 0; i < root->v.map.count; i++) {
            const char *k = root->v.map.items[i].key;
            if (strcmp(k, "schema") && strcmp(k, "root") && strcmp(k, "architectures"))
                dsym_diag_add(diags, DSYM_WARNING, path, root->v.map.items[i].value->line, 0,
                              NULL, "unknown catalogue key '%s'", k);
        }
    }
    const char *catdir = dirname_of(db->arena, path);
    if (ok) {
        const char *rel = str_or_null(db->arena, dsym_map_get(root, "root"));
        db->root_rel = rel ? rel : ".";
        db->root = join(db->arena, catdir, db->root_rel);
        const DsymNode *archs = dsym_map_get(root, "architectures");
        if (!archs || archs->kind != DSYM_NODE_SEQ) {
            dsym_diag_add(diags, DSYM_ERROR, path, 0, 0, "/architectures",
                          "'architectures' must be a list");
            ok = false;
        }
        for (size_t i = 0; ok && i < archs->v.seq.count; i++) {
            const DsymNode *it = archs->v.seq.items[i];
            char where[48];
            snprintf(where, sizeof where, "/architectures/%zu", i);
            ArchDBEntry *e = it->kind == DSYM_NODE_MAP ? db_add(db) : NULL;
            if (!e) {
                dsym_diag_add(diags, DSYM_ERROR, path, it->line, it->col, where,
                              "entry must be a mapping");
                ok = false;
                break;
            }
            e->name = str_or_null(db->arena, dsym_map_get(it, "name"));
            e->file = str_or_null(db->arena, dsym_map_get(it, "file"));
            if (!e->name || !e->file) {
                dsym_diag_add(diags, DSYM_ERROR, path, it->line, it->col, where,
                              "entry needs 'name' and 'file'");
                ok = false;
                break;
            }
            for (size_t k = 0; k + 1 < db->count; k++)
                if (!strcmp(db->entries[k].name, e->name)) {
                    dsym_diag_add(diags, DSYM_ERROR, path, it->line, it->col, where,
                                  "duplicate architecture '%s'", e->name);
                    ok = false;
                }
            e->path = join(db->arena, db->root, e->file);
            e->arch = str_or_null(db->arena, dsym_map_get(it, "arch"));
            e->subarch = str_or_null(db->arena, dsym_map_get(it, "subarch"));
            const DsymNode *as = dsym_map_get(it, "addr_size");
            e->addr_size = as && as->kind == DSYM_NODE_INT ? as->v.i : 0;
            e->byte_order = str_or_null(db->arena, dsym_map_get(it, "byte_order"));
            e->obj_format = str_or_null(db->arena, dsym_map_get(it, "obj_format"));
            e->backend = str_or_null(db->arena, dsym_map_get(it, "backend"));
        }
    }
    if (!ok || !db_finish(db)) {
        archdb_close(db);
        return NULL;
    }
    return db;
}

size_t archdb_count(const ArchDB *db) { return db ? db->count : 0; }

const ArchDBEntry *archdb_entry(const ArchDB *db, size_t i) {
    return db && i < db->count ? &db->entries[i] : NULL;
}

const ArchDBEntry *archdb_find(const ArchDB *db, const char *name) {
    if (!db || !name) return NULL;
    for (size_t i = 0; i < db->count; i++)
        if (!strcmp(db->entries[i].name, name)) return &db->entries[i];
    for (size_t i = 0; i < db->count; i++)
        if (db->entries[i].arch && !strcmp(db->entries[i].arch, name)) return &db->entries[i];
    for (size_t i = 0; i < db->count; i++)
        if (db->entries[i].subarch && !strcmp(db->entries[i].subarch, name))
            return &db->entries[i];
    return NULL;
}

const DsymSpec *archdb_load(ArchDB *db, const ArchDBEntry *e, uint32_t flags,
                            DsymDiags *diags) {
    if (!db || !e || e < db->entries || e >= db->entries + db->count) return NULL;
    size_t i = (size_t)(e - db->entries);
    if (!db->cache[i])
        db->cache[i] = dsym_spec_load_file(e->path, DSYM_FORMAT_AUTO, flags, diags);
    else if ((flags & DSYM_LOAD_BACKEND))
        dsym_spec_load_backend(db->cache[i], diags);
    return db->cache[i];
}

const DsymSpec *archdb_load_name(ArchDB *db, const char *name, uint32_t flags,
                                 DsymDiags *diags) {
    const ArchDBEntry *e = archdb_find(db, name);
    if (!e) {
        dsym_diag_add(diags, DSYM_ERROR, NULL, 0, 0, NULL, "unknown architecture '%s'",
                      name ? name : "");
        return NULL;
    }
    return archdb_load(db, e, flags, diags);
}

bool archdb_write_catalogue(const ArchDB *db, DsymFormat fmt, const char *root,
                            DsymBuf *out, DsymDiags *diags) {
    DsymArena *a = dsym_arena_new();
    if (!a) return false;
    DsymNode *m = dsym_node_map(a);
    DsymNode *list = dsym_node_seq(a);
    bool ok = m && list;
    ok = ok && dsym_map_put_ref(a, m, "schema", dsym_node_int(a, ARCHDB_CATALOGUE_SCHEMA));
    ok = ok && dsym_map_put_ref(a, m, "root", dsym_node_str_ref(a, root ? root : db->root_rel));
    ok = ok && dsym_map_put_ref(a, m, "architectures", list);
    for (size_t i = 0; ok && i < db->count; i++) {
        const ArchDBEntry *e = &db->entries[i];
        DsymNode *it = dsym_node_map(a);
        ok = it && dsym_seq_push(a, list, it);
#define PUT_S(k, v)                                                            \
    if (ok && (v)) ok = dsym_map_put_ref(a, it, k, dsym_node_str_ref(a, v));
        PUT_S("name", e->name)
        PUT_S("file", e->file)
        PUT_S("arch", e->arch)
        PUT_S("subarch", e->subarch)
        if (ok && e->addr_size) ok = dsym_map_put_ref(a, it, "addr_size", dsym_node_int(a, e->addr_size));
        PUT_S("byte_order", e->byte_order)
        PUT_S("obj_format", e->obj_format)
        PUT_S("backend", e->backend)
#undef PUT_S
    }
    if (ok && fmt == DSYM_FORMAT_YAML)
        ok = dsym_buf_puts(out, "# ArchDBCatalogue.yaml — index of architecture specifications.\n"
                                "# Regenerate with: archdb catalogue ArchDB -o ArchDBCatalogue.yaml\n");
    if (ok) ok = dsym_tree_write(fmt, m, out, DSYM_WRITE_NO_HEADER, diags);
    dsym_arena_free(a);
    return ok;
}
