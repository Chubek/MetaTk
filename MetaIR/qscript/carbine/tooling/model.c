/* Carbine tooling: the project model and the build plan.
 *
 * A model is the list of targets declared by Carbinefiles. cb_model_resolve
 * turns names into ids and expands source globs. cb_plan_target then lowers
 * one target into concrete commands. The native executor (exec.c) and all
 * generators (emit.c) read the same plan, so they cannot disagree. */
#define _GNU_SOURCE

#include "internal.h"

#include <fnmatch.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>

/* ---------- models ---------- */

void cb_model_init(cb_model *m, const char *root) {
    memset(m, 0, sizeof *m);
    m->root = cb_strdup(root);
    m->project = cb_strdup("carbine");
    m->spec = NULL;
}

static void target_free(cb_target *t) {
    free(t->name);
    free(t->declared_in);
    cb_list_free(&t->sources);
    cb_list_free(&t->deps);
    cb_list_free(&t->includes);
    cb_list_free(&t->defines);
    cb_list_free(&t->flags);
    cb_list_free(&t->files);
    free(t->dep_ids);
}

void cb_model_free(cb_model *m) {
    size_t i;
    for (i = 0; i < m->count; ++i) target_free(&m->targets[i]);
    free(m->targets);
    free(m->root);
    free(m->project);
    free(m->spec);
    memset(m, 0, sizeof *m);
}

long cb_model_find(const cb_model *m, const char *name) {
    size_t i;
    for (i = 0; i < m->count; ++i)
        if (strcmp(m->targets[i].name, name) == 0) return (long)i;
    return -1;
}

cb_status cb_model_add_target(cb_model *m, const char *name, cb_kind kind, cb_target **out) {
    cb_target *t;
    if (cb_model_find(m, name) >= 0) {
        cb_set_error("target declared twice: %s", name);
        return CB_INVALID_ARGUMENT;
    }
    if (m->count == m->cap) {
        size_t cap = m->cap ? m->cap * 2 : 8;
        cb_target *grown = realloc(m->targets, cap * sizeof *grown);
        if (!grown) return CB_ERROR;
        m->targets = grown;
        m->cap = cap;
    }
    t = &m->targets[m->count];
    memset(t, 0, sizeof *t);
    t->name = cb_strdup(name);
    if (!t->name) return CB_ERROR;
    t->kind = kind;
    ++m->count;
    m->resolved = 0;
    if (out) *out = t;
    return CB_OK;
}

const char *cb_kind_name(cb_kind kind) {
    switch (kind) {
    case CB_KIND_LIB: return "lib";
    case CB_KIND_EXE: return "exe";
    case CB_KIND_PROGRAM: return "program";
    }
    return "?";
}

/* ---------- resolution ---------- */

static int has_suffix(const char *s, const char *suffix) {
    size_t n = strlen(s), k = strlen(suffix);
    return n >= k && strcmp(s + n - k, suffix) == 0;
}

/* Adds rel to t->files unless already present. */
static cb_status add_unique(cb_list *files, const char *rel) {
    size_t i;
    for (i = 0; i < files->count; ++i)
        if (strcmp(files->items[i], rel) == 0) return CB_OK;
    return cb_list_push(files, rel);
}

static cb_status expand_sources(const cb_model *m, cb_target *t) {
    size_t i, j;
    size_t rootlen = strlen(m->root);

    cb_list_free(&t->files);
    cb_list_init(&t->files);
    for (i = 0; i < t->sources.count; ++i) {
        const char *pat = t->sources.items[i];
        if (pat[0] == '!') continue;
        {
            char *full;
            glob_t g;
            size_t n = rootlen + 1 + strlen(pat) + 1;
            int rc;

            full = malloc(n);
            if (!full) return CB_ERROR;
            snprintf(full, n, "%s/%s", m->root, pat);
            rc = glob(full, 0, NULL, &g);
            free(full);
            if (rc == GLOB_NOMATCH) {
                globfree(&g);
                cb_set_error("target %s: no files match '%s'", t->name, pat);
                return CB_NOT_FOUND;
            }
            if (rc != 0) {
                globfree(&g);
                cb_set_error("target %s: cannot expand '%s'", t->name, pat);
                return CB_ERROR;
            }
            for (j = 0; j < g.gl_pathc; ++j) {
                const char *path = g.gl_pathv[j];
                const char *rel = strncmp(path, m->root, rootlen) == 0 && path[rootlen] == '/'
                                      ? path + rootlen + 1 : path;
                if (add_unique(&t->files, rel) != CB_OK) { globfree(&g); return CB_ERROR; }
            }
            globfree(&g);
        }
    }
    /* Exclusions: "!pattern" removes matching relative paths. */
    for (i = 0; i < t->sources.count; ++i) {
        const char *pat = t->sources.items[i];
        size_t k;
        if (pat[0] != '!') continue;
        for (k = 0; k < t->files.count;) {
            if (fnmatch(pat + 1, t->files.items[k], 0) == 0) {
                free(t->files.items[k]);
                memmove(&t->files.items[k], &t->files.items[k + 1],
                        (t->files.count - k - 1) * sizeof *t->files.items);
                --t->files.count;
            } else {
                ++k;
            }
        }
    }
    if (t->files.count == 0) {
        cb_set_error("target %s has no source files", t->name);
        return CB_NOT_FOUND;
    }
    for (i = 0; i < t->files.count; ++i) {
        const char *f = t->files.items[i];
        int ok = t->kind == CB_KIND_PROGRAM ? has_suffix(f, ".c15") : has_suffix(f, ".c");
        if (!ok) {
            cb_set_error("target %s (%s): unexpected source %s",
                         t->name, cb_kind_name(t->kind), f);
            return CB_INVALID_ARGUMENT;
        }
    }
    return CB_OK;
}

cb_status cb_model_resolve(cb_model *m) {
    size_t i, k;
    cb_status st;

    m->resolved = 0;
    for (i = 0; i < m->count; ++i) {
        cb_target *t = &m->targets[i];
        free(t->dep_ids);
        t->dep_ids = NULL;
        t->dep_count = 0;
        if (t->sources.count == 0) {
            cb_set_error("target %s has no sources", t->name);
            return CB_INVALID_ARGUMENT;
        }
    }
    for (i = 0; i < m->count; ++i) {
        cb_target *t = &m->targets[i];
        st = expand_sources(m, t);
        if (st != CB_OK) return st;
        t->dep_ids = calloc(t->deps.count ? t->deps.count : 1, sizeof *t->dep_ids);
        if (!t->dep_ids) return CB_ERROR;
        for (k = 0; k < t->deps.count; ++k) {
            long id = cb_model_find(m, t->deps.items[k]);
            if (id < 0) {
                cb_set_error("target %s depends on unknown target %s", t->name, t->deps.items[k]);
                return CB_NOT_FOUND;
            }
            t->dep_ids[t->dep_count++] = (size_t)id;
        }
    }
    m->resolved = 1;
    return CB_OK;
}

/* ---------- plan ---------- */

void cb_plan_init(cb_plan *p) {
    p->steps = NULL;
    p->count = p->cap = 0;
}

void cb_plan_free(cb_plan *p) {
    size_t i;
    for (i = 0; i < p->count; ++i) {
        cb_list_free(&p->steps[i].inputs);
        cb_list_free(&p->steps[i].argv);
        free(p->steps[i].output);
    }
    free(p->steps);
    cb_plan_init(p);
}

static cb_status plan_push(cb_plan *p, cb_step **out) {
    if (p->count == p->cap) {
        size_t cap = p->cap ? p->cap * 2 : 8;
        cb_step *grown = realloc(p->steps, cap * sizeof *grown);
        if (!grown) return CB_ERROR;
        p->steps = grown;
        p->cap = cap;
    }
    *out = &p->steps[p->count];
    memset(*out, 0, sizeof **out);
    cb_list_init(&(*out)->inputs);
    cb_list_init(&(*out)->argv);
    ++p->count;
    return CB_OK;
}

/* Splits an environment variable on blanks into list. */
static cb_status env_words(const char *name, cb_list *list) {
    const char *value = getenv(name);
    char *copy, *save = NULL, *word;
    if (!value) return CB_OK;
    copy = cb_strdup(value);
    if (!copy) return CB_ERROR;
    for (word = strtok_r(copy, " \t", &save); word; word = strtok_r(NULL, " \t", &save))
        if (cb_list_push(list, word) != CB_OK) { free(copy); return CB_ERROR; }
    free(copy);
    return CB_OK;
}

static const char *tool(const char *env, const char *fallback) {
    const char *v = getenv(env);
    return v && *v ? v : fallback;
}

cb_status cb_target_output(const cb_model *m, size_t target, cb_buf *out) {
    const cb_target *t = &m->targets[target];
    cb_buf_init(out);
    if (t->kind == CB_KIND_LIB) return cb_buf_printf(out, "build/lib%s.a", t->name);
    return cb_buf_printf(out, "build/%s", t->name);
}

/* Object path for a source: build/obj/<target>/<source with / as _>.o */
static cb_status object_path(const cb_target *t, const char *src, cb_buf *out) {
    const char *p;
    cb_buf_init(out);
    cb_buf_printf(out, "build/obj/%s/", t->name);
    for (p = src; *p; ++p) {
        char c = *p == '/' ? '_' : *p;
        cb_buf_add(out, &c, 1);
    }
    return cb_buf_puts(out, ".o");
}

static int link_flag(const char *f) {
    return strncmp(f, "-l", 2) == 0 || strncmp(f, "-L", 2) == 0 || strncmp(f, "-Wl,", 4) == 0;
}

static cb_status plan_compile(const cb_model *m, size_t ti, cb_plan *p, cb_list *objects) {
    const cb_target *t = &m->targets[ti];
    size_t i, k;
    cb_list cflags = {0};
    cb_status st = CB_OK;

    cb_list_init(&cflags);
    st = env_words("CFLAGS", &cflags);
    for (i = 0; st == CB_OK && i < t->files.count; ++i) {
        cb_step *s;
        cb_buf obj;
        const char *src = t->files.items[i];
        st = object_path(t, src, &obj);
        if (st != CB_OK) break;
        st = plan_push(p, &s);
        if (st != CB_OK) { cb_buf_free(&obj); break; }
        s->kind = CB_STEP_COMPILE;
        s->target = ti;
        s->output = cb_strdup(obj.data);
        st = cb_list_push(&s->inputs, src);
        if (st == CB_OK) st = cb_list_push(&s->argv, tool("CC", "cc"));
        if (st == CB_OK) st = cb_list_push(&s->argv, "-c");
        if (st == CB_OK) st = cb_list_push(&s->argv, src);
        if (st == CB_OK) st = cb_list_push(&s->argv, "-o");
        if (st == CB_OK) st = cb_list_push(&s->argv, obj.data);
        for (k = 0; st == CB_OK && k < t->includes.count; ++k) {
            cb_buf b; cb_buf_init(&b);
            cb_buf_printf(&b, "-I%s", t->includes.items[k]);
            st = cb_list_push(&s->argv, b.data);
            cb_buf_free(&b);
        }
        for (k = 0; st == CB_OK && k < t->defines.count; ++k) {
            cb_buf b; cb_buf_init(&b);
            cb_buf_printf(&b, "-D%s", t->defines.items[k]);
            st = cb_list_push(&s->argv, b.data);
            cb_buf_free(&b);
        }
        if (st == CB_OK) st = cb_list_append(&s->argv, &t->flags);
        if (st == CB_OK) st = cb_list_append(&s->argv, &cflags);
        cb_buf_free(&obj);
        if (st == CB_OK) st = cb_list_push(objects, s->output);
    }
    cb_list_free(&cflags);
    return st;
}

cb_status cb_plan_target(const cb_model *m, size_t ti, cb_plan *out) {
    const cb_target *t = &m->targets[ti];
    cb_list objects, inputs_extra;
    cb_step *s;
    cb_buf output;
    size_t k;
    cb_status st;

    cb_plan_init(out);
    cb_list_init(&objects);
    cb_list_init(&inputs_extra);

    if (t->kind == CB_KIND_PROGRAM) {
        st = cb_target_output(m, ti, &output);
        if (st == CB_OK) st = plan_push(out, &s);
        if (st == CB_OK) {
            s->kind = CB_STEP_PROGRAM;
            s->target = ti;
            s->output = cb_strdup(output.data);
            st = cb_list_push(&s->argv, tool("QSCRIPT", "qscript"));
            if (st == CB_OK) st = cb_list_append(&s->argv, &t->flags);
            if (st == CB_OK) st = cb_list_push(&s->argv, "build");
            if (st == CB_OK) st = cb_list_append(&s->inputs, &t->files);
            if (st == CB_OK) st = cb_list_append(&s->argv, &t->files);
            if (st == CB_OK) st = cb_list_push(&s->argv, "-o");
            if (st == CB_OK) st = cb_list_push(&s->argv, output.data);
        }
        cb_buf_free(&output);
        cb_list_free(&objects);
        cb_list_free(&inputs_extra);
        return st;
    }

    st = plan_compile(m, ti, out, &objects);
    if (st == CB_OK) {
        if (t->kind == CB_KIND_LIB) {
            st = cb_target_output(m, ti, &output);
            if (st == CB_OK) st = plan_push(out, &s);
            if (st == CB_OK) {
                s->kind = CB_STEP_ARCHIVE;
                s->target = ti;
                s->output = cb_strdup(output.data);
                st = cb_list_push(&s->argv, tool("AR", "ar"));
                if (st == CB_OK) st = cb_list_push(&s->argv, "rcs");
                if (st == CB_OK) st = cb_list_push(&s->argv, output.data);
                if (st == CB_OK) st = cb_list_append(&s->argv, &objects);
                if (st == CB_OK) st = cb_list_append(&s->inputs, &objects);
            }
            cb_buf_free(&output);
        } else {
            /* Executable: objects, then archives of library dependencies. */
            st = cb_target_output(m, ti, &output);
            for (k = 0; st == CB_OK && k < t->dep_count; ++k) {
                const cb_target *d = &m->targets[t->dep_ids[k]];
                cb_buf archive;
                if (d->kind != CB_KIND_LIB) continue;
                st = cb_target_output(m, t->dep_ids[k], &archive);
                if (st == CB_OK) st = cb_list_push(&inputs_extra, archive.data);
                cb_buf_free(&archive);
            }
            if (st == CB_OK) st = plan_push(out, &s);
            if (st == CB_OK) {
                cb_list ldflags = {0};
                cb_list_init(&ldflags);
                s->kind = CB_STEP_LINK;
                s->target = ti;
                s->output = cb_strdup(output.data);
                st = cb_list_push(&s->argv, tool("CC", "cc"));
                if (st == CB_OK) st = cb_list_push(&s->argv, "-o");
                if (st == CB_OK) st = cb_list_push(&s->argv, output.data);
                if (st == CB_OK) st = cb_list_append(&s->argv, &objects);
                if (st == CB_OK) st = cb_list_append(&s->argv, &inputs_extra);
                if (st == CB_OK) st = cb_list_append(&s->inputs, &objects);
                if (st == CB_OK) st = cb_list_append(&s->inputs, &inputs_extra);
                for (k = 0; st == CB_OK && k < t->flags.count; ++k)
                    if (link_flag(t->flags.items[k])) st = cb_list_push(&s->argv, t->flags.items[k]);
                if (st == CB_OK) st = env_words("LDFLAGS", &ldflags);
                if (st == CB_OK) st = cb_list_append(&s->argv, &ldflags);
                cb_list_free(&ldflags);
            }
            cb_buf_free(&output);
        }
    }
    cb_list_free(&objects);
    cb_list_free(&inputs_extra);
    if (st != CB_OK) cb_plan_free(out);
    return st;
}
