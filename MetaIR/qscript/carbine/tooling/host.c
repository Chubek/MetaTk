/* Carbine tooling: command line parsing and the process-wide host state.
 *
 * The state is a single cb_host. Natives read it; nothing else writes it
 * after cb_host_parse. Carbine is a one-shot tool, so there is no reentrancy
 * to protect. */
#define _GNU_SOURCE

#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static cb_host host;

cb_host *cb_host_state(void) { return &host; }

void cb_host_usage(FILE *out) {
    fputs(
        "usage: carbine [options] COMMAND [arguments]\n"
        "\n"
        "commands:\n"
        "  build [TARGET...]      build the named targets (default: all)\n"
        "  graph                  print the dependency graph\n"
        "  check                  validate the Carbinefile and its sources\n"
        "  lint                   style-check every source file\n"
        "  gen BACKEND            write build.ninja, Makefile, CMakeLists.txt\n"
        "                         or meson.build (BACKEND: ninja make cmake meson)\n"
        "  dist OUT.tar           archive every source file\n"
        "  fetch URL DEST         download a file (http:// or ftp://)\n"
        "  version                print the Carbine and git version\n"
        "  help                   show this message\n"
        "\n"
        "options:\n"
        "  -f FILE        Carbinefile to read (default: Carbinefile)\n"
        "  -C DIR         change to DIR first\n"
        "  -k             keep going after a failed target\n"
        "  -n             print the commands instead of running them\n"
        "  -v             print each command before running it\n"
        "  --engine DIR   QScript engine sources (default: $CARBINE_HOME/source or ./source)\n"
        "  --grammar FILE Carbinefile grammar (default: <engine>/../grammar/carbine.grammar)\n"
        "  --qscript PATH QScript executable used for .c15 programs\n"
        "  --module PATH  load a C extension module (repeatable)\n",
        out);
}

static int set_string(char **slot, const char *value) {
    char *copy = cb_strdup(value);
    if (!copy) return -1;
    free(*slot);
    *slot = copy;
    return 0;
}

static int backend_from_name(const char *name, cb_backend *out) {
    if (strcmp(name, "ninja") == 0) *out = CB_BACKEND_NINJA;
    else if (strcmp(name, "make") == 0) *out = CB_BACKEND_MAKE;
    else if (strcmp(name, "cmake") == 0) *out = CB_BACKEND_CMAKE;
    else if (strcmp(name, "meson") == 0) *out = CB_BACKEND_MESON;
    else return -1;
    return 0;
}

static int usage_error(const char *message) {
    fprintf(stderr, "carbine: %s\n", message);
    cb_host_usage(stderr);
    return 2;
}

int cb_host_parse(int argc, char **argv) {
    int i = 1;
    const char *command = NULL;
    const char *engine_env = getenv("CARBINE_HOME");

    cb_model_init(&host.model, ".");
    cb_list_init(&host.positional);
    cb_list_init(&host.modules);
    host.spec = cb_strdup("Carbinefile");
    if (engine_env && *engine_env) {
        size_t n = strlen(engine_env) + sizeof "/source";
        host.engine = malloc(n);
        if (!host.engine) return 2;
        snprintf(host.engine, n, "%s/source", engine_env);
    } else {
        host.engine = cb_strdup("source");
    }
    if (!host.spec || !host.engine) return 2;
    /* The grammar lives beside the engine directory: <parent>/grammar. */
    {
        char *slash = strrchr(host.engine, '/');
        size_t n = strlen(host.engine) + sizeof "/../grammar/carbine.grammar";
        host.grammar = malloc(n);
        if (!host.grammar) return 2;
        if (!slash) snprintf(host.grammar, n, "grammar/carbine.grammar");
        else snprintf(host.grammar, n, "%.*s/grammar/carbine.grammar", (int)(slash - host.engine), host.engine);
    }

    for (; i < argc; ++i) {
        const char *a = argv[i];
        if (strcmp(a, "-f") == 0 || strcmp(a, "-C") == 0 || strcmp(a, "--engine") == 0 ||
            strcmp(a, "--qscript") == 0 || strcmp(a, "--module") == 0 || strcmp(a, "--grammar") == 0) {
            if (i + 1 >= argc) return usage_error("option needs a value");
            ++i;
            if (strcmp(a, "-f") == 0) {
                if (set_string(&host.spec, argv[i]) != 0) return 2;
            } else if (strcmp(a, "-C") == 0) {
                if (chdir(argv[i]) != 0) {
                    fprintf(stderr, "carbine: cannot change to %s: %s\n", argv[i], strerror(errno));
                    return 2;
                }
            } else if (strcmp(a, "--engine") == 0) {
                if (set_string(&host.engine, argv[i]) != 0) return 2;
            } else if (strcmp(a, "--grammar") == 0) {
                if (set_string(&host.grammar, argv[i]) != 0) return 2;
            } else if (strcmp(a, "--qscript") == 0) {
                if (setenv("QSCRIPT", argv[i], 1) != 0) return 2;
            } else {
                if (cb_list_push(&host.modules, argv[i]) != CB_OK) return 2;
            }
            continue;
        }
        if (strcmp(a, "-k") == 0) { host.keep_going = 1; continue; }
        if (strcmp(a, "-n") == 0) { host.exec.dry_run = 1; continue; }
        if (strcmp(a, "-v") == 0) { host.exec.verbose = 1; continue; }
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            host.command = CB_CMD_HELP;
            cb_host_usage(stdout);
            return 0;
        }
        if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "carbine: unknown option %s\n", a);
            return 2;
        }
        command = a;
        ++i;
        break;
    }
    if (!command) return usage_error("no command given");

    if (strcmp(command, "build") == 0) host.command = CB_CMD_BUILD;
    else if (strcmp(command, "graph") == 0) host.command = CB_CMD_GRAPH;
    else if (strcmp(command, "check") == 0) host.command = CB_CMD_CHECK;
    else if (strcmp(command, "lint") == 0) host.command = CB_CMD_LINT;
    else if (strcmp(command, "gen") == 0) host.command = CB_CMD_GEN;
    else if (strcmp(command, "dist") == 0) host.command = CB_CMD_DIST;
    else if (strcmp(command, "fetch") == 0) host.command = CB_CMD_FETCH;
    else if (strcmp(command, "version") == 0) host.command = CB_CMD_VERSION;
    else if (strcmp(command, "help") == 0) host.command = CB_CMD_HELP;
    else {
        fprintf(stderr, "carbine: unknown command %s\n", command);
        return 2;
    }

    if (host.command == CB_CMD_GEN) {
        if (i >= argc) return usage_error("gen needs a backend: ninja, make, cmake or meson");
        if (backend_from_name(argv[i], &host.backend) != 0) {
            fprintf(stderr, "carbine: unknown backend %s\n", argv[i]);
            return 2;
        }
        ++i;
    }
    for (; i < argc; ++i)
        if (cb_list_push(&host.positional, argv[i]) != CB_OK) return 2;

    if (host.command == CB_CMD_FETCH && host.positional.count != 2)
        return usage_error("fetch needs a URL and a destination");
    if (host.command == CB_CMD_DIST && host.positional.count != 1)
        return usage_error("dist needs an output file");
    if (host.command == CB_CMD_HELP) {
        cb_host_usage(stdout);
        return 0;
    }
    return 0;
}
