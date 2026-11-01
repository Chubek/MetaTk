/** @file mulib-ast.h
 * @brief Owned production trees and deterministic S-expression/JSON/YAML output.
 * Define MULIB_IMPL_AST; explicitly implement STRBUF as well.
 */
#ifndef MULIB_AST_H
#define MULIB_AST_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_ast {
    char *production, *text, *action;
    size_t length, offset;
    struct mulib_ast *parent, *child, *tail, *next;
} mulib_ast_t;
typedef enum mulib_ast_format {
    MULIB_AST_SEXPR, MULIB_AST_JSON, MULIB_AST_YAML
} mulib_ast_format_t;
/** Copies both strings (text may contain NUL). NULL text requires zero length. */
mulib_ast_t *mulib_ast_create(const char *production, const char *text, size_t length);
/** Copy an optional semantic action name; NULL clears it. */
int mulib_ast_set_action(mulib_ast_t *node, const char *action);
/** Transfer an unattached tree on success; rejects cycles and depth > 256. */
int mulib_ast_append(mulib_ast_t *parent, mulib_ast_t *child);
/** Destroy an owned root (not an attached child). NULL is harmless. */
void mulib_ast_destroy(mulib_ast_t *root);
/** Caller frees output. YAML uses YAML 1.2's JSON flow notation. UTF-8 is
 * preserved; invalid high bytes are escaped as Latin-1 codepoints. S-expression
 * atoms use JSON string escapes, including \u0000 for embedded NUL bytes.
 */
char *mulib_ast_render(const mulib_ast_t *root, mulib_ast_format_t format);
typedef int (*mulib_ast_action_fn)(const mulib_ast_t *node, void *user);
typedef struct mulib_ast_walk {
    const char *production;
    mulib_ast_action_fn action;
    void *user;
} mulib_ast_walk_t;
typedef struct mulib_ast_walker {
    mulib_ast_t *root;
} mulib_ast_walker_t;
/** Postorder dispatch; 1 complete, 0 callback stopped, -1 invalid arguments.
 * Callbacks run only after a complete successful parse, never during backtracking.
 * A NULL production matches all nodes. The table and nodes are borrowed.
 */
int mulib_ast_walk(const mulib_ast_t *root, const mulib_ast_walk_t *actions, size_t count);
int mulib_ast_walker_walk(const mulib_ast_walker_t *walker, const mulib_ast_walk_t *actions, size_t count);
void mulib_ast_walker_destroy(mulib_ast_walker_t *walker);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_AST) && !defined(MULIB_AST_IMPLEMENTED)
#define MULIB_AST_IMPLEMENTED
#include "mulib-strbuf.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
mulib_ast_t *mulib_ast_create(const char *name, const char *text, size_t length) {
    mulib_ast_t *n;
    size_t len;
    if (!name || (!text && length) || length == SIZE_MAX) return NULL;
    len = strlen(name);
    n = (mulib_ast_t *)calloc(1, sizeof *n);
    if (!n) return NULL;
    n->production = (char *)malloc(len + 1);
    n->text = (char *)malloc(length + 1);
    if (!n->production || !n->text) { free(n->production); free(n->text); free(n); return NULL; }
    memcpy(n->production, name, len + 1);
    if (length) memcpy(n->text, text, length);
    n->text[length] = 0; n->length = length;
    return n;
}
static size_t mulib_ast_height_(const mulib_ast_t *n) {
    size_t height = 1;
    const mulib_ast_t *c;
    for (c = n->child; c; c = c->next) {
        size_t h = 1 + mulib_ast_height_(c);
        if (h > height) height = h;
    }
    return height;
}
int mulib_ast_set_action(mulib_ast_t *n, const char *action) {
    char *copy = NULL;
    if (!n) return 0;
    if (action) {
        copy = (char *)malloc(strlen(action) + 1);
        if (!copy) return 0;
        strcpy(copy, action);
    }
    free(n->action); n->action = copy; return 1;
}
int mulib_ast_append(mulib_ast_t *p, mulib_ast_t *c) {
    mulib_ast_t *a;
    size_t depth = 0;
    if (!p || !c || c->parent || c->next) return 0;
    for (a = p; a; a = a->parent) { if (a == c) return 0; ++depth; }
    if (depth + mulib_ast_height_(c) > 256) return 0;
    c->parent = p;
    if (p->tail) p->tail->next = c; else p->child = c;
    p->tail = c;
    return 1;
}
void mulib_ast_destroy(mulib_ast_t *n) {
    if (!n || n->parent) return;
    while (n->child) {
        mulib_ast_t *c = n->child;
        n->child = c->next; c->parent = NULL; c->next = NULL;
        mulib_ast_destroy(c);
    }
    free(n->production); free(n->text); free(n->action); free(n);
}
static int mulib_ast_quote_(mulib_strbuf_t *b, const char *s, size_t len) {
    size_t i;
    if (!mulib_strbuf_putc(b, '"')) return 0;
    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 194 && c <= 244) {
            size_t width = c < 224 ? 2 : c < 240 ? 3 : 4, j;
            int valid = width <= len - i;
            for (j = 1; valid && j < width; ++j)
                if ((unsigned char)s[i + j] < 128 || (unsigned char)s[i + j] > 191) valid = 0;
            if (valid && ((c == 224 && (unsigned char)s[i + 1] < 160) ||
                          (c == 237 && (unsigned char)s[i + 1] >= 160) ||
                          (c == 240 && (unsigned char)s[i + 1] < 144) ||
                          (c == 244 && (unsigned char)s[i + 1] > 143))) valid = 0;
            if (valid) {
                if (!mulib_strbuf_append_n(b, s + i, width)) return 0;
                i += width - 1; continue;
            }
        }
        if (c == '"' || c == '\\') {
            if (!mulib_strbuf_putc(b, '\\') || !mulib_strbuf_putc(b, (char)c)) return 0;
        } else if (c < 32 || c >= 127) {
            char escape[7];
            /* Invalid UTF-8 remains representable as escaped byte values. */
            (void)snprintf(escape, sizeof escape, "\\u%04x", (unsigned)c);
            if (!mulib_strbuf_append(b, escape)) return 0;
        } else if (!mulib_strbuf_putc(b, (char)c)) return 0;
    }
    return mulib_strbuf_putc(b, '"');
}
static int mulib_ast_emit_(mulib_strbuf_t *b, const mulib_ast_t *n, int sexpr) {
    const mulib_ast_t *c;
    char span[96];
    if (!mulib_strbuf_append(b, sexpr ? "(" : "{\"production\":" ) ||
        !mulib_ast_quote_(b, n->production, strlen(n->production)) ||
        !mulib_strbuf_append(b, sexpr ? " " : ",\"text\":" ) ||
        !mulib_ast_quote_(b, n->text, n->length)) return 0;
    if (!sexpr) {
        (void)snprintf(span, sizeof span, ",\"offset\":%zu,\"length\":%zu,\"children\":[", n->offset, n->length);
        if (!mulib_strbuf_append(b, span)) return 0;
    }
    for (c = n->child; c; c = c->next) {
        if ((sexpr || c != n->child) && !mulib_strbuf_putc(b, sexpr ? ' ' : ',')) return 0;
        if (!mulib_ast_emit_(b, c, sexpr)) return 0;
    }
    return mulib_strbuf_append(b, sexpr ? ")" : "]}");
}
char *mulib_ast_render(const mulib_ast_t *n, mulib_ast_format_t format) {
    mulib_strbuf_t b;
    if (!n || format < MULIB_AST_SEXPR || format > MULIB_AST_YAML) return NULL;
    mulib_strbuf_init(&b);
    if ((format == MULIB_AST_YAML && !mulib_strbuf_append(&b, "---\n")) ||
        !mulib_ast_emit_(&b, n, format == MULIB_AST_SEXPR)) { mulib_strbuf_free(&b); return NULL; }
    return b.data;
}
int mulib_ast_walk(const mulib_ast_t *root, const mulib_ast_walk_t *actions, size_t count) {
    const mulib_ast_t *n = root;
    size_t i;
    if (!root || (!actions && count)) return -1;
    for (;;) {
        while (n->child) n = n->child;
        for (;;) {
            for (i = 0; i < count; ++i)
                if (actions[i].action && (!actions[i].production || !strcmp(actions[i].production, n->production) ||
                    (n->action && !strcmp(actions[i].production, n->action))))
                    if (!actions[i].action(n, actions[i].user)) return 0;
            if (n == root) return 1;
            if (n->next) { n = n->next; break; }
            n = n->parent;
        }
    }
}
int mulib_ast_walker_walk(const mulib_ast_walker_t *w, const mulib_ast_walk_t *a, size_t n) {
    return w ? mulib_ast_walk(w->root, a, n) : -1;
}
void mulib_ast_walker_destroy(mulib_ast_walker_t *w) {
    if (w) { mulib_ast_destroy(w->root); free(w); }
}
#ifdef __cplusplus
}
#endif
#endif
