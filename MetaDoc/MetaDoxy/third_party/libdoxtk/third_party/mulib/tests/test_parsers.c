#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PEG
#define MULIB_IMPL_REGEX
#include "mulib-json.h"
#include "mulib-peg.h"
#include "mulib-regex.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void test_parsec(void) {
    mulib_parsec_parser_t a = mulib_parsec_byte('a'), b = mulib_parsec_byte('b');
    mulib_parsec_parser_t first_parts[2], alternatives[2], root_parts[3], root;
    mulib_parsec_parser_t first, choice, empty = mulib_parsec_empty(), repeat;
    mulib_parsec_result_t r;
    mulib_parsec_options_t options = {0, 0, 0};
    const mulib_parsec_parser_t *target;
    unsigned char bitmap[32] = {0};
    first_parts[0] = mulib_parsec_capture(&a, 1); first_parts[1] = mulib_parsec_byte('x');
    first = mulib_parsec_sequence(first_parts, 2);
    alternatives[0] = first; alternatives[1] = mulib_parsec_capture(&a, 2);
    choice = mulib_parsec_choice(alternatives, 2);
    root_parts[0] = choice; root_parts[1] = b; root_parts[2] = mulib_parsec_end();
    root = mulib_parsec_sequence(root_parts, 3);
    assert(mulib_parsec_run(&root, "ab", 2, NULL, &r) == 1);
    assert(r.count == 1 && r.captures[0].tag == 2 && r.captures[0].first == 0);
    mulib_parsec_result_free(&r);
    root_parts[0] = mulib_parsec_lookahead(&choice, 1); root_parts[1] = a;
    assert(mulib_parsec_run(&root, "a", 1, NULL, &r) == 1 && r.count == 0);
    mulib_parsec_result_free(&r);
    root_parts[0] = mulib_parsec_lookahead(&b, 0);
    assert(mulib_parsec_run(&root, "a", 1, NULL, &r) == 1);
    mulib_parsec_result_free(&r);
    assert(mulib_parsec_run(&root, "b", 1, NULL, &r) == 0 && r.error.offset == 0);
    mulib_parsec_result_free(&r);
    repeat = mulib_parsec_repeat(&empty, 0, SIZE_MAX);
    assert(mulib_parsec_run(&repeat, NULL, 0, NULL, &r) == -1 && r.error.code == MULIB_PARSEC_NO_PROGRESS);
    mulib_parsec_result_free(&r);
    repeat = mulib_parsec_repeat(&empty, 0, 1);
    assert(mulib_parsec_run(&repeat, NULL, 0, NULL, &r) == 1);
    mulib_parsec_result_free(&r);
    target = &root; root = mulib_parsec_ref(&target); options.max_depth = 10;
    assert(mulib_parsec_run(&root, NULL, 0, &options, &r) == -1 && r.error.code == MULIB_PARSEC_LIMIT);
    mulib_parsec_result_free(&r);
    options.max_depth = 0; options.max_steps = 1;
    root = mulib_parsec_capture(&a, 0);
    assert(mulib_parsec_run(&root, "a", 1, &options, &r) == -1);
    mulib_parsec_result_free(&r);
    options.max_steps = 0; options.max_captures = 1;
    root_parts[0] = root; root_parts[1] = root;
    root = mulib_parsec_sequence(root_parts, 2);
    assert(mulib_parsec_run(&root, "aa", 2, &options, &r) == -1 && r.error.code == MULIB_PARSEC_LIMIT);
    mulib_parsec_result_free(&r);
    bitmap[0] = 1; root = mulib_parsec_set(bitmap);
    assert(mulib_parsec_run(&root, "\0", 1, NULL, &r) == 1 && r.consumed == 1);
    mulib_parsec_result_free(&r);
    root = mulib_parsec_literal("a\nbc", 4);
    assert(mulib_parsec_run(&root, "a\nbx", 4, NULL, &r) == 0);
    assert(r.error.offset == 3 && r.error.line == 2 && r.error.column == 2);
    mulib_parsec_result_free(&r);
    root = mulib_parsec_range(5, 1);
    assert(mulib_parsec_run(&root, "a", 1, NULL, &r) == -1 && r.error.code == MULIB_PARSEC_INVALID);
    mulib_parsec_result_free(&r);
    assert(mulib_parsec_run(&a, NULL, 1, NULL, &r) == -1);
    mulib_parsec_result_free(&r);
    first_parts[0] = a; first_parts[1] = b;
    first = mulib_parsec_sequence(first_parts, 2);
    root = mulib_parsec_named(first, "letter pair");
    assert(mulib_parsec_run(&root, "x", 1, NULL, &r) == 0);
    assert(!strcmp(r.error.expected, "letter pair"));
    mulib_parsec_result_free(&r);
    root = mulib_parsec_capture(&first, 10);
    root_parts[0] = mulib_parsec_capture(&a, 9); root_parts[1] = b;
    first = mulib_parsec_sequence(root_parts, 2);
    assert(mulib_parsec_run(&root, "ab", 2, NULL, &r) == 1 && r.count == 2);
    assert(r.captures[0].first == 0 && r.captures[1].first == 0 && r.captures[1].length == 2);
    mulib_parsec_result_free(&r);
}

static mulib_json_t *json(const char *s) {
    mulib_json_error_t e;
    mulib_json_t *v = mulib_json_parse(s, strlen(s), &e);
    assert(v && !e.message); return v;
}
static void test_json(void) {
    static const char *const invalid[] = {
        "", " ", "01", "-01", "+1", ".1", "1.", "1e", "1e+", "--1", "NaN", "Infinity",
        "true false", "nullx", "[1,]", "[,1]", "[1 2]", "{\"x\":1,}", "{\"x\" 1}", "{1:2}",
        "\"unterminated", "\"\\x41\"", "\"\\u12g4\"", "\"\\uD800\"", "\"\\uDC00\"", "\"\\uD800\\u0041\"",
        "\"\\uD800\\uD800\"", "\"line\nfeed\"", "1e9999", "\357\273\277null", "[", "{", "[true"
    };
    static const double numbers[] = {0, -0.0, DBL_MIN, DBL_MAX, 0.1, -123456789.5, 1e-200, 1e200};
    mulib_json_error_t e;
    mulib_json_t *v, *p, *a, *o, *child;
    size_t i, n;
    char *text, nesting[2 * (MULIB_JSON_MAX_DEPTH + 1) + 1];
    const char source[] = " {\"a\\u0000b\":\"\\u0000\\uD834\\uDD1E\",\"a\":1,\"a\":2,\"list\":[true,false,null,-1.25e+2]} \n";
    const char decoded[] = "\0\360\235\204\236";
    for (i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        assert(!mulib_json_parse(invalid[i], strlen(invalid[i]), &e));
        assert(e.message && e.offset <= strlen(invalid[i]) && e.line >= 1 && e.column >= 1);
    }
    { const char bad[][5] = { {'"', (char)0xc0, (char)0xaf, '"', 0}, {'"', (char)0xed, (char)0xa0, (char)0x80, '"'},
                              {'"', (char)0xf4, (char)0x90, (char)0x80, (char)0x80} };
      for (i = 0; i < 3; ++i) assert(!mulib_json_parse(bad[i], i ? 5 : 4, &e)); }
    assert(!mulib_json_parse("\"a\0b\"", 5, &e));
    assert(!mulib_json_parse(NULL, 1, &e));
    v = mulib_json_parse(source, sizeof source - 1, &e); assert(v && !e.message);
    assert(mulib_json_typeof(v) == MULIB_JSON_OBJECT && mulib_json_size(v) == 3);
    assert(mulib_json_numeric(mulib_json_get(v, "a")) == 2);
    p = mulib_json_get_n(v, "a\0b", 3); assert(p);
    assert(!memcmp(mulib_json_text(p, &n), decoded, sizeof decoded - 1) && n == sizeof decoded - 1);
    assert(mulib_json_key(v, 0, &n) && n == 3);
    assert(!mulib_json_get(v, "missing") && !mulib_json_at(v, 100));
    a = mulib_json_get(v, "list"); assert(mulib_json_size(a) == 4);
    assert(mulib_json_boolean(mulib_json_at(a, 0)) && !mulib_json_boolean(mulib_json_at(a, 1)));
    assert(mulib_json_typeof(mulib_json_at(a, 2)) == MULIB_JSON_NULL);
    assert(mulib_json_numeric(mulib_json_at(a, 3)) == -125);
    text = mulib_json_stringify(v, 1); assert(text && strchr(text, '\n'));
    p = json(text); free(text); mulib_json_destroy(v); mulib_json_destroy(p);
    for (i = 0; i < sizeof numbers / sizeof *numbers; ++i) {
        v = mulib_json_number(numbers[i]); assert(v);
        text = mulib_json_stringify(v, 0); assert(text); p = json(text);
        assert(mulib_json_numeric(p) == numbers[i]);
        if (numbers[i] == 0.0) assert(!!signbit(mulib_json_numeric(p)) == !!signbit(numbers[i]));
        free(text); mulib_json_destroy(v); mulib_json_destroy(p);
    }
    v = json("1e-9999"); assert(mulib_json_numeric(v) == 0); mulib_json_destroy(v);
    assert(!mulib_json_number(INFINITY) && !mulib_json_number(NAN));
    assert(!mulib_json_string_n("\300\200", 2));
    assert(mulib_json_typeof(NULL) == MULIB_JSON_NULL && mulib_json_size(NULL) == 0);
    assert(!mulib_json_text(NULL, &n) && n == 0 && mulib_json_numeric(NULL) == 0.0);
    assert(!mulib_json_key(NULL, 0, &n) && n == 0);
    a = mulib_json_new(MULIB_JSON_ARRAY); o = mulib_json_new(MULIB_JSON_OBJECT);
    child = mulib_json_string("child"); assert(a && o && child);
    assert(mulib_json_append(a, child) && !mulib_json_append(a, child));
    assert(!mulib_json_set(o, "alias", child));
    assert(mulib_json_set(o, "array", a));
    assert(!mulib_json_append(a, o) && !mulib_json_append(a, a));
    assert(mulib_json_set(o, "array", a));
    p = mulib_json_bool(1); assert(p && mulib_json_set_n(o, "n\0k", 3, p));
    p = mulib_json_bool(0); assert(p && mulib_json_set_n(o, "n\0k", 3, p));
    assert(mulib_json_size(o) == 2);
    mulib_json_destroy(o);
    text = mulib_json_stringify(NULL, 0); assert(text && !strcmp(text, "null")); free(text);
    for (i = 0; i < MULIB_JSON_MAX_DEPTH; ++i) nesting[i] = '[';
    for (i = 0; i < MULIB_JSON_MAX_DEPTH; ++i) nesting[MULIB_JSON_MAX_DEPTH + i] = ']';
    v = mulib_json_parse(nesting, 2 * MULIB_JSON_MAX_DEPTH, &e); assert(v); mulib_json_destroy(v);
    for (i = 0; i < MULIB_JSON_MAX_DEPTH + 1; ++i) nesting[i] = '[';
    for (i = 0; i < MULIB_JSON_MAX_DEPTH + 1; ++i) nesting[MULIB_JSON_MAX_DEPTH + 1 + i] = ']';
    assert(!mulib_json_parse(nesting, 2 * (MULIB_JSON_MAX_DEPTH + 1), &e));
    assert(e.message);
    assert(!mulib_json_parse("{\n\"a\":\n}", 9, &e) && e.line >= 2);
    {
        char deep[10000];
        memset(deep, '[', sizeof deep);
        assert(!mulib_json_parse(deep, sizeof deep, &e));
    }
    {
        mulib_strbuf_t deep;
        mulib_strbuf_init(&deep);
        for (i = 0; i < MULIB_JSON_MAX_DEPTH; ++i) assert(mulib_strbuf_append(&deep, "{\"a\":0,\"b\":"));
        assert(mulib_strbuf_append(&deep, "\"\\uD834\\uDD1E\""));
        for (i = 0; i < MULIB_JSON_MAX_DEPTH; ++i) assert(mulib_strbuf_putc(&deep, '}'));
        v = mulib_json_parse(deep.data, deep.length, &e); assert(v);
        mulib_json_destroy(v); mulib_strbuf_free(&deep);
    }
    /* Construction is not depth-limited; stringify is, and destruction must
     * safely release even a tree much deeper than the recursive parser limit. */
    v = mulib_json_new(MULIB_JSON_ARRAY); assert(v); p = v;
    for (i = 0; i < 10000; ++i) {
        child = mulib_json_new(MULIB_JSON_ARRAY);
        assert(child && mulib_json_append(p, child)); p = child;
    }
    assert(!mulib_json_stringify(v, 0)); mulib_json_destroy(v);
}

static mulib_peg_t *peg(const char *s) {
    mulib_peg_error_t e;
    mulib_peg_t *g = mulib_peg_compile(s, strlen(s), &e);
    assert(g && !e.message); return g;
}
static void test_peg(void) {
    static const char *const invalid[] = {
        "", "A <- 'x'", "A 'x';", "A <- B;", "A <- 'a'; A <- 'b';", "A <- A 'x' / '';",
        "A <- B; B <- A;", "A <- ''*;", "A <- ('x'?)+;", "A <- [z-a];", "A <- [];",
        "A <- '\\u1234';", "A <- '\\x0';", "A <- ('x';", "A <- 'x'**;", "A <- [abc;",
        "A <- !A;", "A <- B? 'x'; B <- A;"
    };
    mulib_peg_error_t e;
    mulib_parsec_result_t r;
    mulib_peg_t *g;
    size_t i;
    for (i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        assert(!mulib_peg_compile(invalid[i], strlen(invalid[i]), &e));
        assert(e.message && e.offset <= strlen(invalid[i]));
    }
    g = peg("# recursive balanced groups\n Start <- Space Item Space !.; Item <- '(' Space Item* ')' / [a-z]+ Space; Space <- [ \\t\\n]*;");
    assert(mulib_peg_run(g, NULL, " (a (b c)) ", 11, 1, NULL, &r) == 1);
    assert(r.count && r.captures[r.count - 1].tag == 0 && r.captures[r.count - 1].first == 0);
    mulib_parsec_result_free(&r);
    assert(mulib_peg_run(g, NULL, "(a", 2, 1, NULL, &r) == 0); mulib_parsec_result_free(&r);
    assert(mulib_peg_run(g, "absent", "", 0, 0, NULL, &r) == -1); mulib_parsec_result_free(&r);
    assert(!mulib_peg_rule_name(g, 100)); mulib_peg_destroy(g);
    g = peg("A <- &('ab' / 'a') ('ax' / 'ab') !'x';");
    assert(mulib_peg_run(g, NULL, "ab", 2, 1, NULL, &r) == 1 && r.count == 1);
    mulib_parsec_result_free(&r); mulib_peg_destroy(g);
    g = peg("A <- ('a' / 'ab') 'c';");
    assert(mulib_peg_run(g, NULL, "abc", 3, 1, NULL, &r) == 0); /* ordered PEG choice */
    mulib_parsec_result_free(&r); mulib_peg_destroy(g);
    g = peg("A <- '\\0\\xFF' [^a-z] .?;");
    assert(mulib_peg_run(g, NULL, "\0\3779", 3, 1, NULL, &r) == 1);
    mulib_parsec_result_free(&r); mulib_peg_destroy(g);
    g = peg("A <- 'a';");
    assert(mulib_peg_run(g, NULL, "ab", 2, 0, NULL, &r) == 1 && r.consumed == 1);
    mulib_parsec_result_free(&r); mulib_peg_destroy(g);
    /* Force rule-array growth before resolving forward references. */
    g = peg("A<-J; B<-'b'; C<-'c'; D<-'d'; E<-'e'; F<-'f'; G<-'g'; H<-'h'; I<-'i'; J<-'j';");
    assert(mulib_peg_run(g, NULL, "j", 1, 1, NULL, &r) == 1 && r.count == 2);
    mulib_parsec_result_free(&r); mulib_peg_destroy(g);
    assert(!mulib_peg_compile(NULL, 1, &e));
    {
        char deep[1024];
        memcpy(deep, "A<-", 3); memset(deep + 3, '!', sizeof deep - 3);
        assert(!mulib_peg_compile(deep, sizeof deep, &e) && e.message);
    }
}

static mulib_regex_t *regex(const char *s) {
    mulib_regex_error_t e;
    mulib_regex_t *r = mulib_regex_compile(s, strlen(s), &e);
    assert(r && !e.message); return r;
}
static void test_regex_emission_limit(void) {
    static const char *const bombs[] = {
        "((((){1000}){1000}){1000}){1000}",
        "(((){1000}){1000}){1000}",
        "(((a{0}){1000}){1000}){1000}",
        "((((ab){0}){1000}){1000}){1000}",
        "((((){1000}){1000}){1000}){1000}a"
    };
    struct regex_case { const char *pattern, *text; int full; };
    static const struct regex_case controls[] = {
        {"(){1000}", "", 1}, {"(){1000}", "a", 0},
        {"((){1000}){2}", "", 1}, {"((){1000}){2}", "a", 0},
        {"((a{0}){1000}){2}", "", 1}, {"((a{0}){1000}){2}", "a", 0},
        {"((a?){2}){2}", "", 1}, {"((a?){2}){2}", "aaaa", 1},
        {"((a?){2}){2}", "aaaaa", 0}, {"((a?){2}){2}", "b", 0},
        {"((|ab){2}){2}", "", 1}, {"((|ab){2}){2}", "abababab", 1},
        {"((|ab){2}){2}", "aba", 0}, {"((|ab){2}){2}", "ababababab", 0},
        {"((a{0}|b){2}){2}", "", 1}, {"((a{0}|b){2}){2}", "bbbb", 1},
        {"((a{0}|b){2}){2}", "a", 0}, {"((a{0}|b){2}){2}", "bbbbb", 0},
        {"((()|a){2}){2}", "aaaa", 1}, {"((a{0}b?){2}){2}", "bbbb", 1},
        {"((a?){0}){1000}", "", 1}, {"((a?){0}){1000}", "a", 0},
        {"((((){1000}){1000}){1000}){0}a", "a", 1},
        {"((((){1000}){1000}){1000}){0}a", "", 0}
    };
    mulib_regex_error_t e;
    mulib_regex_match_t m;
    mulib_regex_t *re;
    size_t i;
    /* These used to perform billions/trillions of recursive emissions while
     * allocating only the accept state (plus a trailing literal in the last
     * case). Independent work accounting must stop every expansion. */
    for (i = 0; i < sizeof bombs / sizeof *bombs; ++i) {
        re = mulib_regex_compile(bombs[i], strlen(bombs[i]), &e);
        assert(!re && e.message && !strcmp(e.message, "regex compilation work limit"));
        assert(e.offset <= strlen(bombs[i]) && e.line == 1 && e.column == e.offset + 1);
    }
    assert(!mulib_regex_compile(bombs[0], strlen(bombs[0]), NULL));
    /* Nullable is not synonymous with epsilon-only. A resource guard must not
     * silently discard the consuming branches of these valid expressions. */
    for (i = 0; i < sizeof controls / sizeof *controls; ++i) {
        re = regex(controls[i].pattern);
        assert(mulib_regex_full_match(re, controls[i].text, strlen(controls[i].text)) == controls[i].full);
        mulib_regex_destroy(re);
    }
    re = regex("((a?){2}){2}");
    assert(mulib_regex_search(re, "baaaa!", 6, 1, &m) == 1 && m.offset == 1 && m.length == 4);
    mulib_regex_destroy(re);
}
static void test_regex(void) {
    static const char *const invalid[] = {
        "(", ")", "[", "[]", "[^]", "[z-a]", "[\\d-a]", "[[:alpha:]]", "a{2,1}", "a{1", "a{,2}",
        "a{1001}", "a{9999999999999999999999999999999}", "(a{1000}){1000}",
        "*", "a**", "a+?", "a++", "(?=a)", "\\1", "\\b", "\\x0", "\\"
    };
    struct regex_case { const char *pattern, *text; int full; };
    static const struct regex_case cases[] = {
        {"", "", 1}, {"", "a", 0}, {"a|ab", "ab", 1}, {"(ab|a)*b", "aab", 1},
        {"(a?)*", "aaa", 1}, {"(|a)+", "", 1}, {"a{2,4}", "aaa", 1}, {"a{2,4}", "a", 0},
        {"a{2,4}", "aaaaa", 0}, {"(ab){2,}", "ababab", 1}, {"a{0}", "", 1}, {"a{0}", "a", 0},
        {"^a$", "a", 1}, {"^a$", "a\n", 0}, {".", "\n", 0}, {"[^a]", "\n", 1},
        {"\\d+\\s\\w+", "123 a_B", 1}, {"\\D+", "abc!", 1}, {"\\S+", "abc_", 1},
        {"\\W+", "!?", 1}, {"[-a]+", "a--", 1}, {"[a-]+", "--a", 1}, {"\\[x\\]", "[x]", 1},
        {"(a|aa)*b", "aaaaaaaaaaaaaaaaaaaaac", 0}
    };
    mulib_regex_t *re;
    mulib_regex_match_t m;
    mulib_regex_error_t e;
    size_t i;
    for (i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        assert(!mulib_regex_compile(invalid[i], strlen(invalid[i]), &e));
        assert(e.message && e.offset <= strlen(invalid[i]));
    }
    for (i = 0; i < sizeof cases / sizeof *cases; ++i) {
        re = regex(cases[i].pattern);
        assert(mulib_regex_full_match(re, cases[i].text, strlen(cases[i].text)) == cases[i].full);
        mulib_regex_destroy(re);
    }
    re = regex("a|ab");
    assert(mulib_regex_search(re, "zzabaz", 6, 0, &m) == 1 && m.offset == 2 && m.length == 2);
    assert(mulib_regex_search(re, "zzabaz", 6, 3, &m) == 1 && m.offset == 4 && m.length == 1);
    assert(mulib_regex_search(re, "zzabaz", 6, 7, &m) == -1 && !m.length);
    mulib_regex_destroy(re);
    re = regex("a*$");
    assert(mulib_regex_search(re, "bbb", 3, 0, &m) == 1 && m.offset == 3 && m.length == 0);
    mulib_regex_destroy(re);
    re = regex("\\0[\\x80-\\xFF]");
    assert(mulib_regex_full_match(re, "\0\377", 2) == 1);
    assert(mulib_regex_search(re, "x\0\377y", 4, 0, &m) == 1 && m.offset == 1 && m.length == 2);
    mulib_regex_destroy(re);
    re = regex("^a");
    assert(mulib_regex_search(re, "ba", 2, 0, &m) == 0);
    assert(mulib_regex_search(re, "aa", 2, 1, &m) == 0);
    mulib_regex_destroy(re);
    re = mulib_regex_compile(NULL, 0, &e); assert(re);
    assert(mulib_regex_full_match(re, NULL, 0) == 1);
    mulib_regex_destroy(re);
    assert(!mulib_regex_compile(NULL, 1, &e));
    {
        char deep[1024]; memset(deep, '(', sizeof deep);
        assert(!mulib_regex_compile(deep, sizeof deep, &e) && e.message);
    }
}

static void test_random_malformed(void) {
    uint32_t state = 123456789;
    size_t i, j;
    char bytes[40];
    for (i = 0; i < 2000; ++i) {
        size_t n = i % sizeof bytes;
        mulib_json_t *v;
        mulib_regex_t *re;
        mulib_peg_t *g;
        for (j = 0; j < n; ++j) { state = state * UINT32_C(1664525) + UINT32_C(1013904223); bytes[j] = (char)(state >> 24); }
        v = mulib_json_parse(bytes, n, NULL); mulib_json_destroy(v);
        re = mulib_regex_compile(bytes, n, NULL);
        if (re) { assert(mulib_regex_search(re, bytes, n, 0, NULL) >= 0); mulib_regex_destroy(re); }
        g = mulib_peg_compile(bytes, n, NULL); mulib_peg_destroy(g);
    }
}
int main(void) {
    test_parsec(); test_json(); test_peg(); test_regex(); test_regex_emission_limit(); test_random_malformed();
    return 0;
}
