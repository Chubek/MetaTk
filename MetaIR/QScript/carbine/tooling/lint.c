/* Carbine tooling: style checks for C sources and QScript programs.
 *
 *   trailing whitespace       line ends in space or tab
 *   tab indentation           leading tab (tabs are reported, not expanded)
 *   CRLF line ending          carriage return before the newline
 *   long line                 more than CB_LINT_MAX_COLUMNS bytes
 *   missing final newline     last line not terminated
 *   NUL byte                  the file is probably not text
 *
 * Diagnostics are printed as "path:line: message" on stdout. */
#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define CB_LINT_MAX_COLUMNS 100

static void report(const char *path, size_t line, const char *what, size_t *issues) {
    printf("%s:%zu: %s\n", path, line, what);
    ++*issues;
}

cb_status cb_lint_file(const char *path, size_t *issues) {
    FILE *f = fopen(path, "rb");
    char *line = NULL;
    size_t cap = 0, lineno = 0;
    ssize_t n;
    int last_terminated = 1;

    *issues = 0;
    if (!f) {
        cb_set_error("cannot open %s: %s", path, strerror(errno));
        return CB_IO_ERROR;
    }
    while ((n = getline(&line, &cap, f)) != -1) {
        size_t len = (size_t)n, body;
        ++lineno;
        last_terminated = len > 0 && line[len - 1] == '\n';
        body = len;
        if (body && line[body - 1] == '\n') --body;
        if (body && line[body - 1] == '\r') { --body; report(path, lineno, "CRLF line ending", issues); }
        if (memchr(line, '\0', len)) { report(path, lineno, "NUL byte", issues); continue; }
        if (body && (line[body - 1] == ' ' || line[body - 1] == '\t'))
            report(path, lineno, "trailing whitespace", issues);
        if (body && line[0] == '\t') report(path, lineno, "tab indentation", issues);
        if (body > CB_LINT_MAX_COLUMNS) report(path, lineno, "long line", issues);
    }
    if (!last_terminated && lineno > 0) report(path, lineno, "missing final newline", issues);
    free(line);
    fclose(f);
    return CB_OK;
}
