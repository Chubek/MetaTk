/* Carbine tooling: the natives QScript programs call as carbine::NAME.
 *
 * Every native takes and returns integers (see Carbine-API.h). Target,
 * argument and file names never cross the boundary as strings: QScript
 * uses target indices, and a native that needs text does the work and
 * prints the result itself. Natives that fail return a non-OK status, which
 * traps the QScript program; the diagnostic is printed to stderr first. */
#define _GNU_SOURCE

#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ---------- helpers ---------- */

static int fail(const char *message) {
    cb_set_error("%s", message);
    fprintf(stderr, "carbine: %s\n", message);
    return CB_ERROR;
}

static int report(cb_status st) {
    if (st != CB_OK) {
        fprintf(stderr, "carbine: %s\n", cb_last_error()[0] ? cb_last_error() : cb_status_text(st));
        return CB_ERROR;
    }
    return CB_OK;
}

static cb_host *host(void) { return cb_host_state(); }

static int target_arg(int64_t index, size_t *out) {
    if (index < 0 || (size_t)index >= host()->model.count) {
        cb_set_error("target index %lld out of range", (long long)index);
        return CB_INVALID_ARGUMENT;
    }
    *out = (size_t)index;
    return CB_OK;
}

static int need_spec(void) {
    if (!host()->loaded) return fail("no Carbinefile loaded");
    return CB_OK;
}

static time_t mtime_of(const char *path) {
    struct stat sb;
    return stat(path, &sb) == 0 ? sb.st_mtime : 0;
}

static char *join_root(const char *rel) {
    const cb_model *m = &host()->model;
    size_t n = strlen(m->root) + 1 + strlen(rel) + 1;
    char *p = malloc(n);
    if (p) snprintf(p, n, "%s/%s", m->root, rel);
    return p;
}

/* ---------- arrays: int64 storage owned by C, indexed by handle ---------- */

typedef struct cb_array {
    int64_t *items;
    size_t len;
    int live;
} cb_array;

static cb_array *arrays;
static size_t array_count;

static cb_array *array_at(int64_t handle) {
    if (handle <= 0 || (size_t)handle > array_count || !arrays[handle - 1].live) return NULL;
    return &arrays[handle - 1];
}

static int n_arr_new(const int64_t *a, size_t n, int64_t *r, void *u) {
    size_t i, index = array_count;
    cb_array *slot;
    (void)n; (void)u;
    if (a[0] < 0 || a[0] > (1 << 24)) { cb_set_error("bad array length"); return report(CB_INVALID_ARGUMENT); }
    for (i = 0; i < array_count; ++i)
        if (!arrays[i].live) { index = i; break; }
    if (index == array_count) {
        /* Grow by 16 slots; the new ones start dead. */
        cb_array *grown = realloc(arrays, (array_count + 16) * sizeof *arrays);
        if (!grown) return report(CB_ERROR);
        arrays = grown;
        memset(arrays + array_count, 0, 16 * sizeof *arrays);
        array_count += 16;
    }
    slot = &arrays[index];
    slot->items = calloc(a[0] ? (size_t)a[0] : 1, sizeof *slot->items);
    if (!slot->items) return report(CB_ERROR);
    slot->len = (size_t)a[0];
    slot->live = 1;
    *r = (int64_t)index + 1;
    return CB_OK;
}

static int n_arr_get(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_array *arr = array_at(a[0]);
    (void)n; (void)u;
    if (!arr || a[1] < 0 || (size_t)a[1] >= arr->len) { cb_set_error("array index out of range"); return report(CB_INVALID_ARGUMENT); }
    *r = arr->items[a[1]];
    return CB_OK;
}

static int n_arr_set(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_array *arr = array_at(a[0]);
    (void)n; (void)u;
    if (!arr || a[1] < 0 || (size_t)a[1] >= arr->len) { cb_set_error("array index out of range"); return report(CB_INVALID_ARGUMENT); }
    arr->items[a[1]] = a[2];
    *r = a[2];
    return CB_OK;
}

static int n_arr_len(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_array *arr = array_at(a[0]);
    (void)n; (void)u;
    if (!arr) { cb_set_error("bad array handle"); return report(CB_INVALID_ARGUMENT); }
    *r = (int64_t)arr->len;
    return CB_OK;
}

static int n_arr_free(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_array *arr = array_at(a[0]);
    (void)n; (void)u;
    if (!arr) { cb_set_error("bad array handle"); return report(CB_INVALID_ARGUMENT); }
    free(arr->items);
    arr->items = NULL;
    arr->len = 0;
    arr->live = 0;
    *r = 0;
    return CB_OK;
}

/* ---------- command line ---------- */

static int n_command(const int64_t *a, size_t n, int64_t *r, void *u) {
    (void)a; (void)n; (void)u;
    *r = host()->command;
    return CB_OK;
}

static int n_gen_backend(const int64_t *a, size_t n, int64_t *r, void *u) {
    (void)a; (void)n; (void)u;
    *r = host()->backend;
    return CB_OK;
}

static int n_option(const int64_t *a, size_t n, int64_t *r, void *u) {
    (void)n; (void)u;
    switch (a[0]) {
    case CB_OPT_KEEP_GOING: *r = host()->keep_going; return CB_OK;
    case CB_OPT_DRY_RUN: *r = host()->exec.dry_run; return CB_OK;
    case CB_OPT_VERBOSE: *r = host()->exec.verbose; return CB_OK;
    }
    cb_set_error("unknown option code %lld", (long long)a[0]);
    return report(CB_INVALID_ARGUMENT);
}

static int n_arg_count(const int64_t *a, size_t n, int64_t *r, void *u) {
    (void)a; (void)n; (void)u;
    *r = (int64_t)host()->positional.count;
    return CB_OK;
}

/* Target index named by positional argument i, or -1 when none matches. */
static int n_arg_target(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_host *h = host();
    long id;
    (void)n; (void)u;
    if (a[0] < 0 || (size_t)a[0] >= h->positional.count) { cb_set_error("argument %lld missing", (long long)a[0]); return report(CB_INVALID_ARGUMENT); }
    if (!h->loaded) return report(CB_ERROR);
    id = cb_model_find(&h->model, h->positional.items[a[0]]);
    *r = id;
    return CB_OK;
}

static int n_arg_show(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_host *h = host();
    (void)n; (void)u;
    if (a[0] < 0 || (size_t)a[0] >= h->positional.count) { cb_set_error("argument missing"); return report(CB_INVALID_ARGUMENT); }
    printf("%s\n", h->positional.items[a[0]]);
    *r = 0;
    return CB_OK;
}

/* ---------- Carbinefile ---------- */

static int n_spec_load(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_host *h = host();
    cb_status st;
    char *dir;
    (void)a; (void)n; (void)u;

    cb_model_free(&h->model);
    dir = cb_strdup(h->spec);
    if (!dir) return report(CB_ERROR);
    {
        char *slash = strrchr(dir, '/');
        if (slash) *slash = '\0';
        else strcpy(dir, ".");
    }
    cb_model_init(&h->model, dir);
    free(dir);
    st = cb_spec_load(&h->model, h->spec);
    if (st != CB_OK) return report(st);
    st = cb_model_resolve(&h->model);
    if (st != CB_OK) return report(st);
    h->loaded = 1;
    *r = (int64_t)h->model.count;
    return CB_OK;
}

static int n_target_count(const int64_t *a, size_t n, int64_t *r, void *u) {
    (void)a; (void)n; (void)u;
    if (need_spec() != CB_OK) return CB_ERROR;
    *r = (int64_t)host()->model.count;
    return CB_OK;
}

static int n_target_kind(const int64_t *a, size_t n, int64_t *r, void *u) {
    size_t t;
    (void)n; (void)u;
    if (need_spec() != CB_OK || target_arg(a[0], &t) != CB_OK) return report(CB_INVALID_ARGUMENT);
    *r = host()->model.targets[t].kind;
    return CB_OK;
}

static int n_target_ndeps(const int64_t *a, size_t n, int64_t *r, void *u) {
    size_t t;
    (void)n; (void)u;
    if (need_spec() != CB_OK || target_arg(a[0], &t) != CB_OK) return report(CB_INVALID_ARGUMENT);
    *r = (int64_t)host()->model.targets[t].dep_count;
    return CB_OK;
}

static int n_target_dep(const int64_t *a, size_t n, int64_t *r, void *u) {
    size_t t;
    const cb_target *tg;
    (void)n; (void)u;
    if (need_spec() != CB_OK || target_arg(a[0], &t) != CB_OK) return report(CB_INVALID_ARGUMENT);
    tg = &host()->model.targets[t];
    if (a[1] < 0 || (size_t)a[1] >= tg->dep_count) { cb_set_error("dependency index out of range"); return report(CB_INVALID_ARGUMENT); }
    *r = (int64_t)tg->dep_ids[a[1]];
    return CB_OK;
}

static int n_show_target(const int64_t *a, size_t n, int64_t *r, void *u) {
    size_t t;
    (void)n; (void)u;
    if (need_spec() != CB_OK || target_arg(a[0], &t) != CB_OK) return report(CB_INVALID_ARGUMENT);
    printf("%s\n", host()->model.targets[t].name);
    *r = 0;
    return CB_OK;
}

/* Newest source modification time of a target (0 if none exist yet). */
static int n_target_input_mtime(const int64_t *a, size_t n, int64_t *r, void *u) {
    size_t t, k;
    time_t newest = 0;
    (void)n; (void)u;
    if (need_spec() != CB_OK || target_arg(a[0], &t) != CB_OK) return report(CB_INVALID_ARGUMENT);
    for (k = 0; k < host()->model.targets[t].files.count; ++k) {
        char *p = join_root(host()->model.targets[t].files.items[k]);
        time_t mt = p ? mtime_of(p) : 0;
        free(p);
        if (mt > newest) newest = mt;
    }
    *r = (int64_t)newest;
    return CB_OK;
}

static int n_target_output_mtime(const int64_t *a, size_t n, int64_t *r, void *u) {
    size_t t;
    cb_buf out;
    char *p;
    time_t mt;
    (void)n; (void)u;
    if (need_spec() != CB_OK || target_arg(a[0], &t) != CB_OK) return report(CB_INVALID_ARGUMENT);
    if (cb_target_output(&host()->model, t, &out) != CB_OK) return report(CB_ERROR);
    p = join_root(out.data);
    mt = p ? mtime_of(p) : 0;
    free(p);
    cb_buf_free(&out);
    *r = (int64_t)mt;
    return CB_OK;
}

static int n_build_target(const int64_t *a, size_t n, int64_t *r, void *u) {
    size_t t;
    (void)n; (void)u;
    if (need_spec() != CB_OK || target_arg(a[0], &t) != CB_OK) return report(CB_INVALID_ARGUMENT);
    *r = cb_build_target(&host()->model, t, &host()->exec);
    return CB_OK;
}

/* Prints the edge t -> dependency k as "  name -> dependency". */
static int n_show_edge(const int64_t *a, size_t n, int64_t *r, void *u) {
    size_t t;
    const cb_target *tg;
    (void)n; (void)u;
    if (need_spec() != CB_OK || target_arg(a[0], &t) != CB_OK) return report(CB_INVALID_ARGUMENT);
    tg = &host()->model.targets[t];
    if (a[1] < 0 || (size_t)a[1] >= tg->dep_count) { cb_set_error("dependency index out of range"); return report(CB_INVALID_ARGUMENT); }
    printf("  %s -> %s\n", tg->name, host()->model.targets[tg->dep_ids[a[1]]].name);
    *r = 0;
    return CB_OK;
}

/* Reports a positional argument that names no target. */
static int n_unknown_arg(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_host *h = host();
    (void)n; (void)u;
    if (a[0] < 0 || (size_t)a[0] >= h->positional.count) return report(CB_INVALID_ARGUMENT);
    fprintf(stderr, "carbine: no target named %s\n", h->positional.items[a[0]]);
    *r = 0;
    return CB_OK;
}

/* Fixed diagnostics for the engine. codes: 1 dependency cycle involving
 * (target t), 2 build failed (target t), 3 build finished, 4 Carbinefile
 * checked, 5 lint issues found. t < 0 prints no target name. */
static int n_message(const int64_t *a, size_t n, int64_t *r, void *u) {
    const char *text;
    (void)n; (void)u;
    switch (a[0]) {
    case 1: text = "dependency cycle involving"; break;
    case 2: text = "build failed"; break;
    case 3: text = "build finished"; break;
    case 4: text = "Carbinefile is valid"; break;
    case 5: text = "lint issues found"; break;
    default: cb_set_error("unknown message code"); return report(CB_INVALID_ARGUMENT);
    }
    if (a[1] >= 0 && (size_t)a[1] < host()->model.count && host()->loaded)
        printf("carbine: %s: %s\n", text, host()->model.targets[a[1]].name);
    else
        printf("carbine: %s\n", text);
    *r = 0;
    return CB_OK;
}

/* Prints a status line for a target (carbine::status). codes: 1 building, 2 up to date,
 * 3 failed, 4 skipped (a dependency failed). */
static int n_status(const int64_t *a, size_t n, int64_t *r, void *u) {
    size_t t;
    const char *what;
    (void)n; (void)u;
    if (need_spec() != CB_OK || target_arg(a[1], &t) != CB_OK) return report(CB_INVALID_ARGUMENT);
    switch (a[0]) {
    case 1: what = "building"; break;
    case 2: what = "up to date"; break;
    case 3: what = "failed"; break;
    case 4: what = "skipped"; break;
    default: cb_set_error("unknown status code"); return report(CB_INVALID_ARGUMENT);
    }
    printf("%s: %s\n", what, host()->model.targets[t].name);
    *r = 0;
    return CB_OK;
}

static int n_emit(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_status st;
    (void)n; (void)u;
    if (need_spec() != CB_OK) return CB_ERROR;
    if (a[0] < CB_BACKEND_NINJA || a[0] > CB_BACKEND_MESON) { cb_set_error("unknown backend"); return report(CB_INVALID_ARGUMENT); }
    st = cb_emit_file(&host()->model, (cb_backend)a[0]);
    if (st != CB_OK) return report(st);
    printf("wrote %s\n", cb_backend_file((cb_backend)a[0]));
    *r = 0;
    return CB_OK;
}

static int n_lint(const int64_t *a, size_t n, int64_t *r, void *u) {
    size_t t, k;
    size_t issues = 0;
    (void)a; (void)n; (void)u;
    if (need_spec() != CB_OK) return CB_ERROR;
    for (t = 0; t < host()->model.count; ++t) {
        const cb_list *files = &host()->model.targets[t].files;
        for (k = 0; k < files->count; ++k) {
            char *p = join_root(files->items[k]);
            size_t found = 0;
            if (!p) return report(CB_ERROR);
            if (cb_lint_file(p, &found) != CB_OK) {
                free(p);
                return report(CB_IO_ERROR);
            }
            free(p);
            issues += found;
        }
    }
    printf("%zu issue(s)\n", issues);
    *r = (int64_t)issues;
    return CB_OK;
}

static int n_dist(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_host *h = host();
    cb_list files;
    size_t t;
    cb_status st = CB_OK;
    const char *base;
    (void)a; (void)n; (void)u;
    if (need_spec() != CB_OK) return CB_ERROR;
    cb_list_init(&files);
    for (t = 0; t < h->model.count && st == CB_OK; ++t)
        st = cb_list_append(&files, &h->model.targets[t].files);
    /* The root is the Carbinefile's directory, so its basename is root-relative. */
    base = strrchr(h->spec, '/');
    base = base ? base + 1 : h->spec;
    if (st == CB_OK) st = cb_list_push(&files, base);
    if (st == CB_OK) st = cb_archive_tar(h->positional.items[0], h->model.root, &files);
    cb_list_free(&files);
    if (st != CB_OK) return report(st);
    printf("wrote %s\n", h->positional.items[0]);
    *r = 0;
    return CB_OK;
}

static int n_fetch(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_host *h = host();
    cb_status st;
    (void)a; (void)n; (void)u;
    st = cb_fetch(h->positional.items[0], h->positional.items[1]);
    if (st != CB_OK) return report(st);
    printf("fetched %s\n", h->positional.items[1]);
    *r = 0;
    return CB_OK;
}

static int n_version(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_buf v;
    (void)a; (void)n; (void)u;
    cb_buf_init(&v);
    if (cb_git_describe(".", &v) != CB_OK) {
        printf("carbine %s\n", cb_version());
    } else {
        printf("carbine %s (%s)\n", cb_version(), v.data);
    }
    cb_buf_free(&v);
    *r = 0;
    return CB_OK;
}

static int n_module_load(const int64_t *a, size_t n, int64_t *r, void *u) {
    cb_host *h = host();
    size_t i;
    (void)a; (void)n; (void)u;
    for (i = 0; i < h->modules.count; ++i)
        if (cb_module_load(h->modules.items[i]) != CB_OK) return report(CB_ERROR);
    *r = (int64_t)cb_module_count();
    return CB_OK;
}

/* ---------- table ---------- */

static const cb_native builtin[] = {
    {"carbine::arr_new", 1, n_arr_new},
    {"carbine::arr_get", 2, n_arr_get},
    {"carbine::arr_set", 3, n_arr_set},
    {"carbine::arr_len", 1, n_arr_len},
    {"carbine::arr_free", 1, n_arr_free},
    {"carbine::command", 0, n_command},
    {"carbine::gen_backend", 0, n_gen_backend},
    {"carbine::option", 1, n_option},
    {"carbine::arg_count", 0, n_arg_count},
    {"carbine::arg_target", 1, n_arg_target},
    {"carbine::arg_show", 1, n_arg_show},
    {"carbine::spec_load", 0, n_spec_load},
    {"carbine::target_count", 0, n_target_count},
    {"carbine::target_kind", 1, n_target_kind},
    {"carbine::target_ndeps", 1, n_target_ndeps},
    {"carbine::target_dep", 2, n_target_dep},
    {"carbine::show_target", 1, n_show_target},
    {"carbine::show_edge", 2, n_show_edge},
    {"carbine::unknown_arg", 1, n_unknown_arg},
    {"carbine::message", 2, n_message},
    {"carbine::target_input_mtime", 1, n_target_input_mtime},
    {"carbine::target_output_mtime", 1, n_target_output_mtime},
    {"carbine::build_target", 1, n_build_target},
    {"carbine::status", 2, n_status},
    {"carbine::emit", 1, n_emit},
    {"carbine::lint", 0, n_lint},
    {"carbine::dist", 0, n_dist},
    {"carbine::fetch", 0, n_fetch},
    {"carbine::version", 0, n_version},
    {"carbine::module_load", 0, n_module_load},
};

void cb_natives_install(void) {
    size_t i;
    static int installed;
    if (installed) return;
    installed = 1;
    for (i = 0; i < sizeof builtin / sizeof builtin[0]; ++i) {
        if (cb_registry_add(builtin[i].name, builtin[i].arity, builtin[i].fn, NULL) != CB_OK) {
            fprintf(stderr, "carbine: cannot register %s: %s\n", builtin[i].name, cb_last_error());
        }
    }
}
