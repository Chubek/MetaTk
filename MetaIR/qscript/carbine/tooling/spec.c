/* Carbine tooling: reading Carbinefiles with QScript's grammar engine.
 *
 * The syntax is defined in grammar/carbine.grammar, a QScript grammar
 * (Raku-style, see QScript-Grammar.h). It is read at run time from the path
 * in the host state (default: grammar/carbine.grammar next to the engine),
 * compiled with qs_grammar_compile, and parsed with qs_grammar_parse. The
 * match tree is turned into targets here.
 *
 * `import DIR;` reads DIR/Carbinefile. Source and include patterns in an
 * imported file are relative to its own directory, so they are rewritten
 * to be relative to the project root. */
#define _GNU_SOURCE

#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CARBINE_WITH_QSCRIPT

#include "qscript/include/QScript-Grammar.h"

#define CB_MAX_IMPORT_DEPTH 16

typedef struct spec_state {
    cb_model *m;
    qs_context *ctx;
    qs_grammar *grammar;
} spec_state;

static cb_status read_text(const char *path, char **text, size_t *length) {
    FILE *f = fopen(path, "rb");
    long size;
    char *buf;
    if (!f) {
        cb_set_error("cannot read %s: %s", path, strerror(errno));
        return CB_IO_ERROR;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        cb_set_error("cannot size %s", path);
        return CB_IO_ERROR;
    }
    buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return CB_ERROR; }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        cb_set_error("cannot read %s", path);
        return CB_IO_ERROR;
    }
    fclose(f);
    buf[size] = '\0';
    *text = buf;
    *length = (size_t)size;
    return CB_OK;
}

/* Copies the text of a match node into a new NUL-terminated string. */
static char *node_text(const qs_match *node) {
    size_t len = 0;
    const char *text = qs_match_text(node, &len);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
}

/* Removes the quotes from a quoted path token. */
static void unquote(char *s) {
    size_t n = strlen(s);
    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        memmove(s, s + 1, n - 2);
        s[n - 2] = '\0';
    }
}

/* Adds prefix to a pattern: "!x" with prefix "sub" gives "!sub/x". */
static cb_status push_prefixed(cb_list *list, const char *prefix, const char *pat) {
    cb_buf b;
    cb_status st;
    cb_buf_init(&b);
    if (*prefix == '\0') {
        st = cb_buf_puts(&b, pat);
    } else if (pat[0] == '!') {
        st = cb_buf_printf(&b, "!%s/%s", prefix, pat + 1);
    } else {
        st = cb_buf_printf(&b, "%s/%s", prefix, pat);
    }
    if (st == CB_OK) st = cb_list_push(list, b.data);
    cb_buf_free(&b);
    return st;
}

static cb_status add_statement(spec_state *s, cb_target *t, const qs_match *stmt, const char *prefix) {
    const char *sym = qs_match_sym(stmt);
    size_t i;
    cb_list *target_list = NULL;
    int prefixed = 0;

    if (strcmp(sym, "sources") == 0) { target_list = &t->sources; prefixed = 1; }
    else if (strcmp(sym, "deps") == 0) target_list = &t->deps;
    else if (strcmp(sym, "include") == 0) { target_list = &t->includes; prefixed = 1; }
    else if (strcmp(sym, "define") == 0) target_list = &t->defines;
    else if (strcmp(sym, "flags") == 0) target_list = &t->flags;
    else {
        cb_set_error("%s: unknown statement '%s' in target %s", t->declared_in, sym, t->name);
        return CB_INVALID_ARGUMENT;
    }
    (void)s;
    for (i = 0; i < qs_match_count(stmt); ++i) {
        const qs_match *child = qs_match_child(stmt, i);
        char *value;
        cb_status st;
        if (strcmp(qs_match_name(child), "path") != 0) continue;
        value = node_text(child);
        if (!value) return CB_ERROR;
        unquote(value);
        st = prefixed ? push_prefixed(target_list, prefix, value) : cb_list_push(target_list, value);
        free(value);
        if (st != CB_OK) return st;
    }
    return CB_OK;
}

static cb_status add_target(spec_state *s, const qs_match *item, const char *path, const char *prefix) {
    const qs_match *kind_node = qs_match_find(item, "kind");
    const qs_match *name_node = qs_match_find(item, "name");
    char *kind_text, *name;
    cb_kind kind;
    cb_target *t = NULL;
    size_t i;
    cb_status st;

    if (!kind_node || !name_node) {
        cb_set_error("%s: malformed target", path);
        return CB_INVALID_ARGUMENT;
    }
    kind_text = node_text(kind_node);
    name = node_text(name_node);
    if (!kind_text || !name) { free(kind_text); free(name); return CB_ERROR; }
    if (strcmp(kind_text, "lib") == 0) kind = CB_KIND_LIB;
    else if (strcmp(kind_text, "exe") == 0) kind = CB_KIND_EXE;
    else if (strcmp(kind_text, "program") == 0) kind = CB_KIND_PROGRAM;
    else {
        cb_set_error("%s: target %s has unknown kind '%s' (lib, exe or program)", path, name, kind_text);
        free(kind_text); free(name);
        return CB_INVALID_ARGUMENT;
    }
    free(kind_text);
    st = cb_model_add_target(s->m, name, kind, &t);
    if (st != CB_OK) { free(name); return st; }
    free(name);
    t->declared_in = cb_strdup(path);
    if (!t->declared_in) return CB_ERROR;

    for (i = 0; i < qs_match_count(item); ++i) {
        const qs_match *child = qs_match_child(item, i);
        if (strcmp(qs_match_name(child), "stmt") != 0) continue;
        st = add_statement(s, t, child, prefix);
        if (st != CB_OK) return st;
    }
    return CB_OK;
}

static cb_status load_file(spec_state *s, const char *path, const char *prefix, int depth);

static cb_status add_import(spec_state *s, const qs_match *item, const char *path, const char *prefix, int depth) {
    const qs_match *path_node = qs_match_find(item, "path");
    char *dir, *child_path, *child_prefix;
    cb_status st;
    size_t n;
    char *slash;

    if (!path_node) return CB_INVALID_ARGUMENT;
    dir = node_text(path_node);
    if (!dir) return CB_ERROR;
    unquote(dir);

    /* Child Carbinefile lives in <this file's dir>/<dir>/Carbinefile. */
    n = strlen(path) + strlen(dir) + sizeof "/Carbinefile";
    child_path = malloc(n);
    child_prefix = malloc(strlen(prefix) + strlen(dir) + 2);
    if (!child_path || !child_prefix) { free(dir); free(child_path); free(child_prefix); return CB_ERROR; }
    slash = strrchr(path, '/');
    if (slash) {
        size_t dl = (size_t)(slash - path) + 1;
        memcpy(child_path, path, dl);
        child_path[dl] = '\0';
        strcat(child_path, dir);
    } else {
        strcpy(child_path, dir);
    }
    strcat(child_path, "/Carbinefile");
    if (*prefix) sprintf(child_prefix, "%s/%s", prefix, dir);
    else strcpy(child_prefix, dir);
    st = load_file(s, child_path, child_prefix, depth + 1);
    free(dir);
    free(child_path);
    free(child_prefix);
    return st;
}

static cb_status load_file(spec_state *s, const char *path, const char *prefix, int depth) {
    char *text = NULL;
    size_t length = 0;
    const qs_match *root;
    qs_match *match;
    size_t i;
    cb_status st;

    if (depth > CB_MAX_IMPORT_DEPTH) {
        cb_set_error("%s: imports nested too deeply", path);
        return CB_INVALID_ARGUMENT;
    }
    st = read_text(path, &text, &length);
    if (st != CB_OK) return st;
    match = qs_grammar_parse(s->ctx, s->grammar, text, length, "TOP");
    if (!match) {
        cb_set_error("%s: %s", path, qs_error(s->ctx));
        free(text);
        return CB_INVALID_ARGUMENT;
    }
    root = match;
    st = CB_OK;
    for (i = 0; st == CB_OK && i < qs_match_count(root); ++i) {
        const qs_match *item = qs_match_child(root, i);
        const char *sym;
        if (strcmp(qs_match_name(item), "item") != 0) continue;
        sym = qs_match_sym(item);
        if (strcmp(sym, "project") == 0) {
            const qs_match *name = qs_match_find(item, "name");
            char *value = name ? node_text(name) : NULL;
            if (!value) { st = CB_INVALID_ARGUMENT; break; }
            free(s->m->project);
            s->m->project = value;
        } else if (strcmp(sym, "import") == 0) {
            st = add_import(s, item, path, prefix, depth);
        } else if (strcmp(sym, "target") == 0) {
            st = add_target(s, item, path, prefix);
        } else {
            cb_set_error("%s: unknown item '%s'", path, sym);
            st = CB_INVALID_ARGUMENT;
        }
    }
    qs_match_destroy(match);
    free(text);
    return st;
}

cb_status cb_spec_load(cb_model *m, const char *path) {
    spec_state s;
    cb_status st;
    char *grammar_text = NULL;
    size_t grammar_len = 0;
    const char *grammar_path = cb_host_state()->grammar;

    st = read_text(grammar_path, &grammar_text, &grammar_len);
    if (st != CB_OK) return st;
    s.m = m;
    s.ctx = qs_create();
    if (!s.ctx) { free(grammar_text); return CB_ERROR; }
    s.grammar = qs_grammar_compile(s.ctx, grammar_text, grammar_len);
    free(grammar_text);
    if (!s.grammar) {
        cb_set_error("%s does not compile: %s", grammar_path, qs_error(s.ctx));
        qs_destroy(s.ctx);
        return CB_INVALID_ARGUMENT;
    }
    st = load_file(&s, path, "", 0);
    qs_grammar_destroy(s.grammar);
    qs_destroy(s.ctx);
    return st;
}

#else  /* !CARBINE_WITH_QSCRIPT */

cb_status cb_spec_load(cb_model *m, const char *path) {
    (void)m;
    cb_set_error("this build has no QScript support, so %s cannot be read", path);
    return CB_UNSUPPORTED;
}

#endif
