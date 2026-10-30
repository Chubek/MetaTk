#include "MetaPP.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out) {
    fputs("Usage: metacpp [-E] [-P] [-I dir] [-D name[=value]] [-U name]\n"
          "               [-o output] [input|-]\n"
          "Preprocess C source (stdin by default). Output has no line markers.\n", out);
}
static char *read_stdin(size_t *length) {
    size_t size = 0, cap = 4096;
    char *s = (char *)malloc(cap);
    if (!s) return NULL;
    for (;;) {
        size_t n = fread(s + size, 1, cap - size - 1, stdin);
        if (memchr(s + size, '\0', n)) {
            fputs("metacpp: NUL byte in source\n", stderr); free(s); return NULL;
        }
        size += n;
        if (size > 64u * 1024u * 1024u) {
            fputs("metacpp: input exceeds 64 MiB limit\n", stderr); free(s); return NULL;
        }
        if (ferror(stdin)) { free(s); return NULL; }
        if (feof(stdin)) break;
        if (cap >= 128u * 1024u * 1024u) { free(s); return NULL; }
        cap *= 2;
        char *next = (char *)realloc(s, cap);
        if (!next) { free(s); return NULL; }
        s = next;
    }
    s[size] = '\0'; *length = size; return s;
}
int main(int argc, char **argv) {
    metapp_context *ctx = metapp_create();
    const char *input = NULL, *destination = NULL;
    char *output = NULL;
    int status = 0, options = 1;
    if (!ctx) { fputs("metacpp: out of memory\n", stderr); return 1; }
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (options && strcmp(arg, "--") == 0) { options = 0; continue; }
        if (options && strcmp(arg, "--help") == 0) { usage(stdout); goto done; }
        if (options && strcmp(arg, "--version") == 0) { puts("MetaPP 0.1.0"); goto done; }
        if (options && (strcmp(arg, "-E") == 0 || strcmp(arg, "-P") == 0)) continue;
        if (options && arg[0] == '-' && arg[1] && strchr("IDUo", arg[1])) {
            char option = arg[1];
            const char *value = arg + 2;
            if (!*value) {
                if (++i == argc) { fprintf(stderr, "metacpp: missing argument for %s\n", arg); status = 2; goto done; }
                value = argv[i];
            }
            int ok = 1;
            if (option == 'I') ok = metapp_add_include_path(ctx, value);
            else if (option == 'U') ok = metapp_undefine(ctx, value);
            else if (option == 'o') destination = value;
            else {
                size_t n = strlen(value);
                char *definition = (char *)malloc(n + 3);
                if (!definition) { status = 1; goto done; }
                memcpy(definition, value, n + 1);
                char *equal = strchr(definition, '=');
                if (equal) *equal = ' ';
                else { definition[n] = ' '; definition[n + 1] = '1'; definition[n + 2] = '\0'; }
                ok = metapp_define(ctx, definition); free(definition);
            }
            if (!ok) { fputs(metapp_diagnostics(ctx), stderr); status = 1; goto done; }
        } else if (options && arg[0] == '-' && arg[1]) {
            fprintf(stderr, "metacpp: unknown option %s\n", arg); status = 2; goto done;
        } else {
            if (input) { fputs("metacpp: expected one input\n", stderr); status = 2; goto done; }
            input = arg;
        }
    }
    if (!input || strcmp(input, "-") == 0) {
        size_t length = 0;
        char *source = read_stdin(&length);
        if (!source) { fputs("metacpp: failed to read stdin\n", stderr); status = 1; goto done; }
        status = metapp_preprocess_n(ctx, source, length, "<stdin>", &output) ? 0 : 1;
        free(source);
    } else status = metapp_preprocess_file(ctx, input, &output) ? 0 : 1;
    fputs(metapp_diagnostics(ctx), stderr);
    if (!status) {
        FILE *out = destination ? fopen(destination, "wb") : stdout;
        if (!out) { perror(destination); status = 1; goto done; }
        if (fputs(output, out) == EOF) status = 1;
        if (destination) { if (fclose(out)) status = 1; }
        else if (fflush(out)) status = 1;
        if (status) fputs("metacpp: output write failed\n", stderr);
    }
done:
    metapp_free(output); metapp_destroy(ctx); return status;
}
