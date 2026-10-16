/* Carbine host: runs the QScript engine (source/shell.qs) with the native
 * functions of libcarbine registered as carbine::NAME.
 *
 * Built only when QScript is available (CARBINE_WITH_QSCRIPT). The engine
 * sources are read from the directory given by --engine, $CARBINE_HOME/source
 * or ./source. The exit status is the value the engine returns. */
#include "internal.h"

#include "qscript/QScript.h"
#include "qscript/include/QScript-Library.h"
#include "qscript/include/QScript-Module.h"
#include "qscript/include/QScript-Runtime.h"
#include "qscript/include/QScript-FFI.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_value(int64_t value, void *user) {
    FILE *stream = user ? (FILE *)user : stdout;
    fprintf(stream, "%" PRId64 "\n", value);
}

int main(int argc, char **argv) {
    cb_host *h;
    qs_context *ctx;
    qs_module *module;
    qs_program *program = NULL;
    qs_run_info info;
    char *entry;
    size_t i, n;
    int status = 0;

    cb_natives_install();
    status = cb_host_parse(argc, argv);
    if (status != 0) return status;
    h = cb_host_state();
    if (h->command == CB_CMD_HELP) return 0;

    for (i = 0; i < h->modules.count; ++i) {
        if (cb_module_load(h->modules.items[i]) != CB_OK) {
            fprintf(stderr, "carbine: %s\n", cb_last_error());
            return 1;
        }
    }

    ctx = qs_create();
    if (!ctx) { fputs("carbine: out of memory\n", stderr); return 1; }
    qs_set_output(ctx, print_value, stdout);
    for (i = 0, n = cb_registry_count(); i < n; ++i) {
        const cb_registered *r = cb_registry_get(i);
        if (qs_register_function(ctx, r->native.name, r->native.arity,
                                 (qs_host_fn)r->native.fn, r->user) != QS_OK) {
            fprintf(stderr, "carbine: cannot register %s: %s\n", r->native.name, qs_error(ctx));
            qs_destroy(ctx);
            return 1;
        }
    }
    if (qs_add_library_path(ctx, h->engine) != QS_OK) {
        fprintf(stderr, "carbine: %s\n", qs_error(ctx));
        qs_destroy(ctx);
        return 1;
    }

    entry = malloc(strlen(h->engine) + sizeof "/shell.qs");
    if (!entry) { qs_destroy(ctx); return 1; }
    sprintf(entry, "%s/shell.qs", h->engine);
    module = qs_module_create();
    if (!module || qs_module_add_file(module, entry) != QS_OK) {
        fprintf(stderr, "carbine: cannot read engine entry %s: %s\n", entry, qs_error(ctx));
        free(entry);
        if (module) qs_module_destroy(module);
        qs_destroy(ctx);
        return 1;
    }
    free(entry);
    if (qs_module_compile(ctx, module, &program) != QS_OK) {
        fprintf(stderr, "carbine: %s\n", qs_error(ctx));
        qs_module_destroy(module);
        qs_destroy(ctx);
        return 1;
    }
    qs_module_destroy(module);

    memset(&info, 0, sizeof info);
    if (qs_run_ex(ctx, program, &info) != QS_OK) {
        if (*qs_error(ctx)) fprintf(stderr, "carbine: %s\n", qs_error(ctx));
        status = 1;
    } else {
        status = (int)info.result;
    }
    qs_program_destroy(program);
    qs_destroy(ctx);
    return status;
}
