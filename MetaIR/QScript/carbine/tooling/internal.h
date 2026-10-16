/* Carbine tooling: internal declarations shared by the C files in tooling/.
 * Nothing here is part of the public C API (include/). */
#ifndef CARBINE_INTERNAL_H
#define CARBINE_INTERNAL_H

#include "Carbine-API.h"
#include "Carbine-Module.h"

#include <stdio.h>

/* ---------- growable text buffer (api.c) ---------- */
typedef struct cb_buf {
    char *data;
    size_t len, cap;
} cb_buf;

void cb_buf_init(cb_buf *b);
void cb_buf_free(cb_buf *b);
cb_status cb_buf_add(cb_buf *b, const char *s, size_t n);
cb_status cb_buf_puts(cb_buf *b, const char *s);
cb_status cb_buf_printf(cb_buf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* ---------- string lists (api.c) ---------- */
typedef struct cb_list {
    char **items;
    size_t count, cap;
} cb_list;

void cb_list_init(cb_list *l);
void cb_list_free(cb_list *l);
cb_status cb_list_push(cb_list *l, const char *s);   /* copies s */
cb_status cb_list_append(cb_list *dst, const cb_list *src);

char *cb_strdup(const char *s);                       /* NULL on OOM */
void cb_set_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* ---------- escaping for each generator (format.c) ---------- */
cb_status cb_fmt_shell(cb_buf *b, const char *s);       /* one POSIX shell word */
cb_status cb_fmt_make(cb_buf *b, const char *s);        /* recipe text: $ -> $$ */
cb_status cb_fmt_make_path(cb_buf *b, const char *s);   /* rule target/prerequisite */
cb_status cb_fmt_ninja(cb_buf *b, const char *s);       /* variable value: $ -> $$ */
cb_status cb_fmt_ninja_path(cb_buf *b, const char *s);  /* build edge path */
cb_status cb_fmt_cmake(cb_buf *b, const char *s);       /* CMake argument */
cb_status cb_fmt_meson(cb_buf *b, const char *s);       /* meson single-quoted string */

/* ---------- tools (archive.c, git.c, lint.c, net.c) ---------- */
cb_status cb_archive_tar(const char *out, const char *root, const cb_list *files);
cb_status cb_git_describe(const char *root, cb_buf *out);
cb_status cb_lint_file(const char *path, size_t *issues);

typedef struct cb_url {
    char scheme[16];
    char host[256];
    char port[8];
    char user[128];
    char pass[128];
    char path[1024];
} cb_url;

cb_status cb_url_parse(const char *text, cb_url *u);
cb_status cb_http_get(const char *url, const char *dest);   /* httpx.c */
cb_status cb_ftp_get(const char *url, const char *dest);    /* ftp.c */
cb_status cb_fetch(const char *url, const char *dest);      /* httpx.c: dispatch */
/* Socket helpers shared by httpx.c and ftp.c. */
int cb_tcp_connect(const char *host, const char *port);     /* -1 on failure */
int cb_send_all(int fd, const char *data, size_t n);        /* 0 on success */
int cb_recv_all(int fd, cb_buf *b);                         /* appends to EOF */

/* ---------- model (model.c) ---------- */
typedef enum cb_kind {
    CB_KIND_LIB = 1,        /* static library of C sources */
    CB_KIND_EXE = 2,        /* executable of C sources, linked with its deps */
    CB_KIND_PROGRAM = 3     /* QScript program (.c15) built by `qscript build` */
} cb_kind;

typedef struct cb_target {
    char *name;
    cb_kind kind;
    char *declared_in;      /* Carbinefile that declared the target */
    cb_list sources;        /* patterns, as written; "!pat" excludes */
    cb_list deps;           /* target names as written */
    cb_list includes;
    cb_list defines;
    cb_list flags;
    /* Filled by cb_model_resolve. */
    size_t *dep_ids;
    size_t dep_count;
    cb_list files;          /* expanded sources, relative to the project root */
} cb_target;

typedef struct cb_model {
    char *root;             /* project root: the directory of the Carbinefile */
    char *project;          /* from `project NAME;`, or "carbine" */
    char *spec;            /* path of the top-level Carbinefile */
    cb_target *targets;
    size_t count, cap;
    int resolved;
} cb_model;

void cb_model_init(cb_model *m, const char *root);
void cb_model_free(cb_model *m);
cb_status cb_model_add_target(cb_model *m, const char *name, cb_kind kind, cb_target **out);
long cb_model_find(const cb_model *m, const char *name);
/* Resolves dependency names to ids and expands source globs. */
cb_status cb_model_resolve(cb_model *m);
const char *cb_kind_name(cb_kind kind);

/* ---------- build steps (model.c) ---------- */
typedef enum cb_step_kind {
    CB_STEP_COMPILE = 1,    /* cc -c src -o obj */
    CB_STEP_ARCHIVE = 2,    /* ar rcs lib objs */
    CB_STEP_LINK = 3,       /* cc -o exe objs libs */
    CB_STEP_PROGRAM = 4     /* qscript build srcs -o out */
} cb_step_kind;

typedef struct cb_step {
    cb_step_kind kind;
    size_t target;
    cb_list inputs;        /* root-relative */
    char *output;          /* root-relative */
    cb_list argv;          /* full command, cwd = project root */
} cb_step;

typedef struct cb_plan {
    cb_step *steps;
    size_t count, cap;
} cb_plan;

void cb_plan_init(cb_plan *p);
void cb_plan_free(cb_plan *p);
/* Steps for one target, in order. Tools come from $CC, $AR, $QSCRIPT,
 * $CFLAGS and $LDFLAGS when set. */
cb_status cb_plan_target(const cb_model *m, size_t target, cb_plan *out);
/* Output path of a target (root-relative). */
cb_status cb_target_output(const cb_model *m, size_t target, cb_buf *out);

/* ---------- generators (emit.c) ---------- */
typedef enum cb_backend {
    CB_BACKEND_NINJA = 1,
    CB_BACKEND_MAKE = 2,
    CB_BACKEND_CMAKE = 3,
    CB_BACKEND_MESON = 4
} cb_backend;

const char *cb_backend_file(cb_backend b);   /* e.g. "build.ninja" */
cb_status cb_emit(const cb_model *m, cb_backend backend, cb_buf *out);
/* Writes the generator's file into the project root. */
cb_status cb_emit_file(const cb_model *m, cb_backend backend);

/* ---------- execution (exec.c) ---------- */
typedef struct cb_exec_opts {
    int dry_run;            /* print commands instead of running them */
    int verbose;            /* print commands before running them */
} cb_exec_opts;

/* Runs argv with cwd = root. Returns the exit status, or -1 if it could
 * not be started (diagnostic in cb_last_error). */
int cb_exec(const cb_list *argv, const char *root, const cb_exec_opts *opts);
/* Runs every step of one target. Returns 0 or the first failing status. */
int cb_build_target(const cb_model *m, size_t target, const cb_exec_opts *opts);

/* ---------- Carbinefile front end (spec.c) ---------- */
/* Parses the Carbinefile at path, following `import` lines, into m, using the
 * grammar named by the host state. Needs a QScript build (CARBINE_WITH_QSCRIPT);
 * otherwise returns CB_UNSUPPORTED. */
cb_status cb_spec_load(cb_model *m, const char *path);

/* ---------- host state and command line (host.c) ---------- */
/* Command codes QScript receives from carbine::command(). */
typedef enum cb_command {
    CB_CMD_NONE = 0,
    CB_CMD_BUILD = 1,
    CB_CMD_GRAPH = 2,
    CB_CMD_CHECK = 3,
    CB_CMD_LINT = 4,
    CB_CMD_GEN = 5,
    CB_CMD_DIST = 6,
    CB_CMD_FETCH = 7,
    CB_CMD_VERSION = 8,
    CB_CMD_HELP = 9
} cb_command;

/* Option codes for carbine::option(). */
enum {
    CB_OPT_KEEP_GOING = 1,
    CB_OPT_DRY_RUN = 2,
    CB_OPT_VERBOSE = 3
};

typedef struct cb_host {
    cb_model model;
    int loaded;                 /* spec_load succeeded */
    cb_exec_opts exec;
    int keep_going;
    cb_command command;
    cb_backend backend;         /* for CB_CMD_GEN */
    cb_list positional;         /* arguments after the command */
    cb_list modules;            /* --module paths */
    char *spec;                 /* Carbinefile path */
    char *engine;               /* directory of the QScript engine sources */
    char *grammar;              /* the Carbinefile grammar, read at run time */
} cb_host;

cb_host *cb_host_state(void);
/* Parses argv into the host state. Returns 0, or 2 after a usage error. */
int cb_host_parse(int argc, char **argv);
void cb_host_usage(FILE *out);
/* Process-wide natives: tooling state, arrays and commands. */
void cb_natives_install(void);

/* ---------- native registry (module.c) ---------- */
/* Every native the host registers with QScript, built-in or from a module. */
typedef struct cb_registered {
    cb_native native;
    void *user;
} cb_registered;

cb_status cb_registry_add(const char *name, size_t arity, cb_native_fn fn, void *user);
size_t cb_registry_count(void);
const cb_registered *cb_registry_get(size_t index);

#endif
