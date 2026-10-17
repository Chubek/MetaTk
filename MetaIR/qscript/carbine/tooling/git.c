/* Carbine tooling: version stamp from git (`carbine version`). */
#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <stdio.h>
#include <string.h>

/* Runs `git -C root describe --always --dirty --tags` and appends its first
 * output line to out. CB_NOT_FOUND if the directory is not a git work tree. */
cb_status cb_git_describe(const char *root, cb_buf *out) {
    char cmd[1200];
    char line[256];
    FILE *p;
    int status;

    if (strchr(root, '\'') != NULL) {
        cb_set_error("project path contains a quote: %s", root);
        return CB_INVALID_ARGUMENT;
    }
    snprintf(cmd, sizeof cmd,
             "git -C '%s' describe --always --dirty --tags 2>/dev/null", root);
    p = popen(cmd, "r");
    if (!p) {
        cb_set_error("cannot run git");
        return CB_ERROR;
    }
    if (!fgets(line, sizeof line, p)) {
        pclose(p);
        cb_set_error("no git version information in %s", root);
        return CB_NOT_FOUND;
    }
    status = pclose(p);
    if (status != 0) {
        cb_set_error("git describe failed in %s", root);
        return CB_NOT_FOUND;
    }
    line[strcspn(line, "\r\n")] = '\0';
    return cb_buf_puts(out, line);
}
