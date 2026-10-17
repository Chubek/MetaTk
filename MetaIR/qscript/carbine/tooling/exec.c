/* Carbine tooling: running plan steps without a generator (`carbine build`).
 *
 * Commands are started with fork/execvp, never through a shell, so an
 * argument is always exactly one argument. */
#define _GNU_SOURCE

#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Creates dir and its parents, relative to root. */
static int mkdir_p(const char *root, const char *dir) {
    char *path, *p;
    size_t n = strlen(root) + 1 + strlen(dir) + 1;
    if (dir[0] == '\0') return 0;
    path = malloc(n);
    if (!path) return -1;
    snprintf(path, n, "%s/%s", root, dir);
    for (p = path + strlen(root) + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(path, 0777) != 0 && errno != EEXIST) { free(path); return -1; }
        *p = '/';
    }
    if (mkdir(path, 0777) != 0 && errno != EEXIST) { free(path); return -1; }
    free(path);
    return 0;
}

static void print_argv(const cb_list *argv, const char *root) {
    size_t i;
    printf("(cd %s &&", root);
    for (i = 0; i < argv->count; ++i) {
        cb_buf q;
        cb_buf_init(&q);
        cb_fmt_shell(&q, argv->items[i]);
        printf(" %s", q.data ? q.data : "");
        cb_buf_free(&q);
    }
    printf(")\n");
    fflush(stdout);
}

int cb_exec(const cb_list *argv, const char *root, const cb_exec_opts *opts) {
    pid_t pid;
    int status = 0;
    size_t i;
    char **args;

    if (argv->count == 0) {
        cb_set_error("empty command");
        return -1;
    }
    if (opts && (opts->dry_run || opts->verbose)) print_argv(argv, root);
    if (opts && opts->dry_run) return 0;

    args = calloc(argv->count + 1, sizeof *args);
    if (!args) { cb_set_error("out of memory"); return -1; }
    for (i = 0; i < argv->count; ++i) args[i] = argv->items[i];

    pid = fork();
    if (pid < 0) {
        free(args);
        cb_set_error("fork failed: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        if (chdir(root) != 0) {
            fprintf(stderr, "carbine: cannot enter %s: %s\n", root, strerror(errno));
            _exit(127);
        }
        execvp(args[0], args);
        fprintf(stderr, "carbine: cannot run %s: %s\n", args[0], strerror(errno));
        _exit(127);
    }
    free(args);
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            cb_set_error("wait failed: %s", strerror(errno));
            return -1;
        }
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}

int cb_build_target(const cb_model *m, size_t target, const cb_exec_opts *opts) {
    cb_plan plan;
    size_t i;
    int rc = 0;
    cb_status st = cb_plan_target(m, target, &plan);

    if (st != CB_OK) return 1;
    for (i = 0; i < plan.count && rc == 0; ++i) {
        const cb_step *s = &plan.steps[i];
        char *dir = cb_strdup(s->output);
        char *slash = dir ? strrchr(dir, '/') : NULL;
        if (!dir) { rc = 1; break; }
        if (slash) {
            *slash = '\0';
            if (!(opts && opts->dry_run) && mkdir_p(m->root, dir) != 0) {
                cb_set_error("cannot create %s: %s", dir, strerror(errno));
                free(dir);
                rc = 1;
                break;
            }
        }
        free(dir);
        rc = cb_exec(&s->argv, m->root, opts);
        if (rc < 0) rc = 1;
    }
    cb_plan_free(&plan);
    return rc;
}
