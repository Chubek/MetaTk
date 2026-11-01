#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_AST
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_CONFUSE
#define MULIB_IMPL_VISITOR
#define MULIB_IMPL_LISTENER
#define MULIB_IMPL_EVTSTREAM
#define MULIB_IMPL_LAZYSTREAM
#include "mulib-confuse.h"
#include "mulib-visitor.h"
#include "mulib-listener.h"
#include "mulib-lazystream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int action(const mulib_ast_t *node, void *user) {
    assert(node->production);
    ++*(int *)user; return 1;
}
static int stop_action(const mulib_ast_t *node, void *user) { (void)node; (void)user; return 0; }
static mulib_visit_result_t skip(const mulib_ast_t *node, void *user) {
    (void)node; ++*(int *)user; return MULIB_VISIT_SKIP;
}
static mulib_visit_result_t visit(const mulib_ast_t *node, void *user) {
    (void)node; ++*(int *)user; return MULIB_VISIT_DESCEND;
}
static int generate(void *user, void **out) {
    int *i = (int *)user; ++*i; *out = i; return 1;
}
static int even(void *value, void *user) { (void)user; return *(int *)value % 2 == 0; }
static int twice(void *value, void **out, void *user) {
    int *slot = (int *)user; *slot = *(int *)value * 2; *out = slot; return 1;
}
static int sum(void *acc, void *value, void *user) { (void)user; *(int *)acc += *(int *)value; return 1; }
static int broken(void *user, void **out) { ++*(int *)user; (void)out; return -1; }
static void test_stream(void) {
    int i = 0, mapped = 0, total = 0;
    void *out;
    mulib_lazystream_t *s = mulib_lazystream_source(generate, &i, NULL);
    s = mulib_lazystream_filter(s, even, NULL);
    s = mulib_lazystream_map(s, twice, &mapped);
    s = mulib_lazystream_drop(s, 1);
    s = mulib_lazystream_take(s, 3);
    assert(i == 0);
    assert(mulib_lazystream_fold(s, &total, sum, NULL) == 1);
    assert(total == 8 + 12 + 16 && i == 8);
    assert(mulib_lazystream_next(s, &out) == 0 && i == 8);
    mulib_lazystream_destroy(s);
    s = mulib_lazystream_take(mulib_lazystream_source(generate, &i, NULL), 0);
    assert(mulib_lazystream_next(s, &out) == 0 && i == 8);
    mulib_lazystream_destroy(s);
    i = 0; s = mulib_lazystream_source(broken, &i, NULL);
    assert(mulib_lazystream_next(s, &out) == -1);
    assert(mulib_lazystream_next(s, &out) == -1 && i == 1);
    mulib_lazystream_destroy(s);
    { void *a[] = {NULL, &i}; s = mulib_lazystream_array(a, 2);
        assert(mulib_lazystream_next(s, &out) == 1 && out == NULL);
        assert(mulib_lazystream_next(s, &out) == 1 && out == &i);
        assert(mulib_lazystream_next(s, &out) == 0); mulib_lazystream_destroy(s); }
}
static void test_grammar(void) {
    const char *peg = "start <- word (' ' word)* -> finish; word <- @(plus (range 'a' 'z'));";
    mulib_parser_error_t e;
    mulib_parser_t *p = mulib_parser_from_peg_ex(peg, &e);
    mulib_ast_walker_t *w;
    int calls = 0, visited = 0, entered = 0, left = 0;
    mulib_ast_walk_t a = {"finish", action, &calls};
    mulib_listener_t listener = {NULL, action, action, &entered};
    char *text;
    assert(p && e.code == MULIB_PARSER_OK);
    w = mulib_parser_parse(p, NULL, "hello world", 11, &a, 1, NULL, &e);
    assert(w && calls == 1 && !strcmp(w->root->child->text, "hello"));
    assert(!mulib_parser_parse(p, NULL, "hello!", 6, &a, 1, NULL, &e) && calls == 1);
    assert(e.code == MULIB_PARSER_MATCH && e.column == 6);
    mulib_parser_destroy(p);
    assert(mulib_visitor_walk(w->root, visit, &visited) == 1 && visited == 3);
    assert(mulib_listener_walk(w->root, &listener, 1) == 1 && entered == 6);
    { mulib_evtstream_t stream = mulib_evtstream_from_ast(w->root); mulib_event_t event;
        while (mulib_evtstream_next(&stream, &event) == 1) if (event.kind == MULIB_EVENT_LEAVE) ++left;
        assert(left == 3); }
    text = mulib_ast_render(w->root, MULIB_AST_JSON); assert(text && strstr(text, "\"production\":\"start\"")); free(text);
    text = mulib_ast_render(w->root, MULIB_AST_YAML); assert(text && !strncmp(text, "---\n{", 5)); free(text);
    text = mulib_ast_render(w->root, MULIB_AST_SEXPR); assert(text && !strncmp(text, "(\"start\"", 8)); free(text);
    mulib_ast_walker_destroy(w);
    p = mulib_parser_from_ebnf_ex("start = WORD, { ' ', WORD }, [ '!' ]; WORD = ? (plus (range 'a' 'z')) ?;", &e);
    assert(p);
    w = mulib_parser_parse(p, NULL, "hello world!", 12, NULL, 0, NULL, &e); assert(w);
    mulib_ast_walker_destroy(w); mulib_parser_destroy(p);
    assert(!mulib_parser_from_peg_ex("nonexistent-grammar.peg", &e) && e.code == MULIB_PARSER_IO);
    assert(!mulib_parser_from_peg_ex("", &e) && e.code == MULIB_PARSER_SYNTAX);
    assert(!mulib_parser_from_peg_ex("x <- missing;", &e) && e.code == MULIB_PARSER_SYNTAX);
    assert(!mulib_parser_from_peg_ex("x <- x 'a';", &e));
    assert(!mulib_parser_from_peg_ex("x <- ('')*;", &e));
    assert(!mulib_parser_from_peg_ex("x <- @(range 'z' 'a');", &e));
    assert(!mulib_parser_from_ebnf_ex("x = ['a';", &e));
    assert(!mulib_parser_from_ebnf_ex("x = 'a'; (* unterminated", &e));
    assert(!mulib_parser_from_ebnf_ex("x = 'a',;", &e));
    assert(!mulib_parser_from_peg_ex("x <- @(unknown 'a');", &e));
    p = mulib_parser_from_peg("x <- @(seq (and (lit 'a')) (plus (set 'abc')) (not (any)) (eof));"); assert(p);
    w = mulib_parser_parse(p, NULL, "abc", 3, NULL, 0, NULL, &e); assert(w);
    mulib_ast_walker_destroy(w); mulib_parser_destroy(p);
    p = mulib_parser_from_ebnf_ex("(* comment *) start = DIGIT, { DIGIT }; DIGIT = ? (range '0' '9') ?;", &e);
    assert(p); w = mulib_parser_parse(p, NULL, "123", 3, NULL, 0, NULL, &e); assert(w);
    mulib_ast_walker_destroy(w); mulib_parser_destroy(p);
    /* Captures from failed alternatives are rolled back before dispatch. */
    p = mulib_parser_from_peg("root <- (word '!' / word); word <- 'ok' -> matched;");
    assert(p); calls = 0; a.production = "matched";
    w = mulib_parser_parse(p, NULL, "ok", 2, &a, 1, NULL, &e); assert(w && calls == 1);
    visited = 0; assert(mulib_visitor_walk(w->root, skip, &visited) == 1 && visited == 1);
    mulib_ast_walker_destroy(w);
    { mulib_ast_walk_t abort_action = {"matched", stop_action, NULL};
        assert(!mulib_parser_parse(p, NULL, "ok", 2, &abort_action, 1, NULL, &e) && e.code == MULIB_PARSER_ACTION); }
    { mulib_parsec_options_t options = {0, 1, 0};
        assert(!mulib_parser_parse(p, NULL, "ok", 2, NULL, 0, &options, &e)); }
    assert(!mulib_parser_parse(p, "missing", "ok", 2, NULL, 0, NULL, &e));
    mulib_parser_destroy(p);
    p = mulib_parser_from_peg("root <- '(' root ')' / 'x';"); assert(p);
    w = mulib_parser_parse(p, NULL, "((x))", 5, NULL, 0, NULL, &e); assert(w);
    assert(w->root->child->offset == 1 && w->root->child->child->offset == 2);
    mulib_ast_walker_destroy(w); mulib_parser_destroy(p);
    { FILE *f = fopen("grammar.peg", "wb"); assert(f); fputs(peg, f); assert(!fclose(f));
        f = fopen("input.lang", "wb"); assert(f); fputs("hi there", f); assert(!fclose(f));
        p = mulib_parser_from_peg("grammar.peg"); assert(p);
        w = mulib_parser_parse_to_walker(p, "input.lang", &e); assert(w);
        mulib_ast_walker_destroy(w); mulib_parser_destroy(p);
        assert(!remove("grammar.peg")); assert(!remove("input.lang"));
        f = fopen("bad.ebnf", "wb"); assert(f); fputs("start = UNKNOWN;", f); assert(!fclose(f));
        assert(!mulib_parser_from_ebnf_ex("bad.ebnf", &e) && e.code == MULIB_PARSER_SYNTAX);
        assert(!remove("bad.ebnf")); }
}
static void test_ast(void) {
    mulib_ast_t *root = mulib_ast_create("root", "\xc2\xb5\0\xff", 4);
    mulib_ast_t *child = mulib_ast_create("child", "", 0), *n;
    char *text;
    size_t i;
    assert(root && child && mulib_ast_append(root, child));
    assert(!mulib_ast_append(child, root));
    assert(!mulib_ast_append(root, child));
    text = mulib_ast_render(root, MULIB_AST_JSON);
    assert(text && strstr(text, "\xc2\xb5\\u0000\\u00ff")); free(text);
    n = child;
    for (i = 2; i < 256; ++i) {
        child = mulib_ast_create("nested", "", 0); assert(child && mulib_ast_append(n, child)); n = child;
    }
    child = mulib_ast_create("too-deep", "", 0); assert(child && !mulib_ast_append(n, child));
    mulib_ast_destroy(child); mulib_ast_destroy(root);
}
static void test_confuse(void) {
    const char *source = "# comment\nport=8080; enabled=yes\nname=\"hello\\nworld\"\n"
        "ratio=1.25 // comment\nserver \"main\" { host=localhost ports={80,443,}; }\n"
        "server backup { host='other' } /* block */ empty=[]";
    mulib_parser_error_t e;
    mulib_confuse_t *c = mulib_confuse_parse(source, strlen(source), &e);
    const mulib_confuse_t *s, *list;
    if (!c) fprintf(stderr, "confuse: %s at %zu:%zu (%zu)\n", e.message, e.line, e.column, e.offset);
    assert(c);
    assert(mulib_confuse_getint(c, "port", -1) == 8080);
    assert(mulib_confuse_getbool(c, "enabled", 0));
    assert(mulib_confuse_getreal(c, "ratio", 0) == 1.25);
    assert(!strcmp(mulib_confuse_getstr(c, "name", ""), "hello\nworld"));
    s = mulib_confuse_get(c, "server", 0); assert(s && !strcmp(s->title, "main"));
    list = mulib_confuse_get(s, "ports", 0); assert(list && list->kind == MULIB_CONFUSE_LIST);
    assert(mulib_confuse_get(list, NULL, 1)->as.integer == 443);
    s = mulib_confuse_get(c, "server", 1); assert(s && !strcmp(s->title, "backup"));
    assert(!strcmp(mulib_confuse_getstr(s, "host", ""), "other"));
    mulib_confuse_destroy(c);
    assert(!mulib_confuse_parse("port={1,,2}", 11, &e));
    { const char *overflow = "x=99999999999999999999999999";
        assert(!mulib_confuse_parse(overflow, strlen(overflow), &e) && e.code == MULIB_PARSER_SYNTAX); }
    { const char nul[] = {'x', '=', '"', 'a', 0, 'b', '"'};
        assert(!mulib_confuse_parse(nul, sizeof nul, &e) && e.code == MULIB_PARSER_SYNTAX); }
    c = mulib_confuse_parse("", 0, &e); assert(c && !c->child); mulib_confuse_destroy(c);
    assert(!mulib_confuse_load("missing.conf", &e) && e.code == MULIB_PARSER_IO);
}
int main(void) { test_ast(); test_grammar(); test_confuse(); test_stream(); return 0; }
