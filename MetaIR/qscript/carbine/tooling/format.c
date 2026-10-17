/* Carbine tooling: quoting and escaping for each generated build language.
 *
 * A command is first made into one shell word per argument (cb_fmt_shell),
 * then escaped for the file format that carries it, so that a path with
 * spaces, dollar signs or quotes survives all three layers. */
#include "internal.h"

#include <string.h>

static int shell_safe(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           strchr("_./+-=:,@%", c) != NULL;
}

cb_status cb_fmt_shell(cb_buf *b, const char *s) {
    const char *p;
    int plain = *s != '\0';
    for (p = s; *p; ++p)
        if (!shell_safe((unsigned char)*p)) { plain = 0; break; }
    if (plain) return cb_buf_puts(b, s);
    if (cb_buf_puts(b, "'") != CB_OK) return CB_ERROR;
    for (p = s; *p; ++p) {
        if (*p == '\'') {
            if (cb_buf_puts(b, "'\\''") != CB_OK) return CB_ERROR;
        } else if (cb_buf_add(b, p, 1) != CB_OK) {
            return CB_ERROR;
        }
    }
    return cb_buf_puts(b, "'");
}

cb_status cb_fmt_make(cb_buf *b, const char *s) {
    const char *p;
    for (p = s; *p; ++p) {
        if (*p == '$') { if (cb_buf_puts(b, "$$") != CB_OK) return CB_ERROR; }
        else if (cb_buf_add(b, p, 1) != CB_OK) return CB_ERROR;
    }
    return CB_OK;
}

cb_status cb_fmt_make_path(cb_buf *b, const char *s) {
    const char *p;
    for (p = s; *p; ++p) {
        const char *esc = NULL;
        switch (*p) {
        case '$': esc = "$$"; break;
        case ' ': esc = "\\ "; break;
        case '#': esc = "\\#"; break;
        case ':': esc = "\\:"; break;
        default: break;
        }
        if (esc ? cb_buf_puts(b, esc) != CB_OK : cb_buf_add(b, p, 1) != CB_OK) return CB_ERROR;
    }
    return CB_OK;
}

cb_status cb_fmt_ninja(cb_buf *b, const char *s) { return cb_fmt_make(b, s); }

cb_status cb_fmt_ninja_path(cb_buf *b, const char *s) {
    const char *p;
    for (p = s; *p; ++p) {
        const char *esc = NULL;
        switch (*p) {
        case '$': esc = "$$"; break;
        case ' ': esc = "$ "; break;
        case ':': esc = "$:"; break;
        default: break;
        }
        if (esc ? cb_buf_puts(b, esc) != CB_OK : cb_buf_add(b, p, 1) != CB_OK) return CB_ERROR;
    }
    return CB_OK;
}

static int cmake_bare(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           strchr("_./+-=:,@%", c) != NULL;
}

cb_status cb_fmt_cmake(cb_buf *b, const char *s) {
    const char *p;
    int bare = *s != '\0';
    for (p = s; *p; ++p)
        if (!cmake_bare((unsigned char)*p)) { bare = 0; break; }
    if (bare) return cb_buf_puts(b, s);
    if (cb_buf_puts(b, "\"") != CB_OK) return CB_ERROR;
    for (p = s; *p; ++p) {
        const char *esc = NULL;
        if (*p == '\\') esc = "\\\\";
        else if (*p == '"') esc = "\\\"";
        else if (*p == '$') esc = "\\$";
        if (esc ? cb_buf_puts(b, esc) != CB_OK : cb_buf_add(b, p, 1) != CB_OK) return CB_ERROR;
    }
    return cb_buf_puts(b, "\"");
}

cb_status cb_fmt_meson(cb_buf *b, const char *s) {
    const char *p;
    if (cb_buf_puts(b, "'") != CB_OK) return CB_ERROR;
    for (p = s; *p; ++p) {
        if (*p == '\\' || *p == '\'') {
            if (cb_buf_add(b, "\\", 1) != CB_OK) return CB_ERROR;
        }
        if (cb_buf_add(b, p, 1) != CB_OK) return CB_ERROR;
    }
    return cb_buf_puts(b, "'");
}
