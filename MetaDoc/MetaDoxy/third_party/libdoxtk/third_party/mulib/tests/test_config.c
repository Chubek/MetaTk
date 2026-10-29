#define MULIB_IMPL_YAML
#define MULIB_IMPL_TOML
#define MULIB_IMPL_TEMPLATE
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-yaml.h"
#include "mulib-toml.h"
#include "mulib-template.h"
#include "mulib-strbuf.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int same_value(const mulib_json_t *a, const mulib_json_t *b)
{
    mulib_json_type type = mulib_json_typeof(a);
    size_t i, an, bn;
    const char *ap, *bp;
    if (type != mulib_json_typeof(b)) return 0;
    switch (type) {
    case MULIB_JSON_NULL: return 1;
    case MULIB_JSON_BOOL: return mulib_json_boolean(a) == mulib_json_boolean(b);
    case MULIB_JSON_NUMBER: return mulib_json_numeric(a) == mulib_json_numeric(b);
    case MULIB_JSON_STRING:
        ap = mulib_json_text(a, &an); bp = mulib_json_text(b, &bn);
        return an == bn && memcmp(ap, bp, an) == 0;
    case MULIB_JSON_ARRAY:
        if (mulib_json_size(a) != mulib_json_size(b)) return 0;
        for (i = 0; i < mulib_json_size(a); ++i)
            if (!same_value(mulib_json_at(a, i), mulib_json_at(b, i))) return 0;
        return 1;
    case MULIB_JSON_OBJECT:
        if (mulib_json_size(a) != mulib_json_size(b)) return 0;
        for (i = 0; i < mulib_json_size(a); ++i) {
            const mulib_json_t *member;
            ap = mulib_json_key(a, i, &an);
            member = mulib_json_get_n(b, ap, an);
            if (!member || !same_value(mulib_json_at(a, i), member)) return 0;
        }
        return 1;
    }
    return 0;
}
static void string_is(const mulib_json_t *v, const char *bytes, size_t length)
{
    size_t n;
    const char *text = mulib_json_text(v, &n);
    assert(text && n == length && memcmp(text, bytes, length) == 0);
}
static mulib_json_t *yaml_ok(const char *text)
{
    mulib_yaml_error_t e;
    mulib_json_t *v = mulib_yaml_parse(text, strlen(text), &e);
    if (!v) fprintf(stderr, "YAML at %zu:%zu: %s\n%s\n", e.line, e.column, e.message, text);
    assert(v && !e.message && e.offset == 0);
    return v;
}
static mulib_json_t *toml_ok(const char *text)
{
    mulib_toml_error_t e;
    mulib_json_t *v = mulib_toml_parse(text, strlen(text), &e);
    if (!v) fprintf(stderr, "TOML at %zu:%zu: %s\n%s\n", e.line, e.column, e.message, text);
    assert(v && !e.message && e.offset == 0);
    return v;
}
static void yaml_bad(const char *text)
{
    mulib_yaml_error_t e;
    mulib_json_t *v = mulib_yaml_parse(text, strlen(text), &e);
    if (v) fprintf(stderr, "YAML unexpectedly accepted: %s\n", text);
    assert(!v && e.message && e.line >= 1 && e.column >= 1 && e.offset <= strlen(text));
    assert(!mulib_yaml_parse(text, strlen(text), NULL));
}
static void toml_bad(const char *text)
{
    mulib_toml_error_t e;
    mulib_json_t *v = mulib_toml_parse(text, strlen(text), &e);
    if (v) fprintf(stderr, "TOML unexpectedly accepted: %s\n", text);
    assert(!v && e.message && e.line >= 1 && e.column >= 1 && e.offset <= strlen(text));
    assert(!mulib_toml_parse(text, strlen(text), NULL));
}
static void yaml_tests(void)
{
    static const char *bad[] = {
        "a: 1\na: 2", "a: 1\n\"a\": 2", "a:\n\tbad: 1", "a: [1, 2",
        "a: [a,,b]", "a: {x: 1, x: 2}", "a: {x: 1 y: 2}", "a: [1, 2}",
        "a: {x:1}", "a: {x:true}", "a: {x #comment: 1}", "a: [foo[bar]", "a: {a[b]: 1}",
        ",foo: 1", "outer:\n  ,foo: 1", "- ,foo: 1", "{- a: 1}", "{-\ta: 1}", "outer: {- a: 1}",
        "a: [x:]", "a: [x:,y]", "a: [x: y]",
        "a: \"x\" garbage", "a: \"x\"#comment", "a: \"\\q\"", "a: 'unterminated",
        "a: \"\\uD800\"", "a: \"\\uDC00\"", "a: \"\\U00110000\"", "a: \"\\xGG\"",
        "a: &anchor hi", "a: *anchor", "a: !!str 1", "a: |\n  text", "a: >\n  text",
        "<<: {x: 1}", "? [a, b]\n: c", "%YAML 1.2\n---\na: 1", "a: .inf",
        "a: -.Inf", "a: .NaN", "a: 0xFF", "a: -0xFF", "a: +0o77", "a: 0o77", "a: 9007199254740992",
        "a: 1e9999", "a: 1e", "a: [1,\n  2]", "a: text\n  more text",
        "a:\n   b: 1\n  c: 2", "- - x", "a: 1\n- b", "a: 1\nb", "a: ]",
        "---\na: 1\n---\nb: 2", "---\n---", "...\na: 1", "--- a: b", "--- value",
        "a: b\n--- c: d", "... a: b", "- name: a\n other: b"
    };
    mulib_json_t *v, *list, *child;
    mulib_yaml_error_t e;
    size_t i;
    v = mulib_yaml_parse(NULL, 0, &e);
    assert(v && mulib_json_typeof(v) == MULIB_JSON_NULL && !e.message);
    mulib_json_destroy(v);
    {
        char *text = mulib_yaml_stringify(NULL, &e);
        assert(text && strcmp(text, "null\n") == 0 && !e.message); free(text);
    }
    v = yaml_ok(" # comment\n\n--- # document\n... # end\n");
    assert(mulib_json_typeof(v) == MULIB_JSON_NULL); mulib_json_destroy(v);
    v = yaml_ok("\xef\xbb\xbf---\r\nserver:\r\n  enabled: true # yes\r\n  port: +0080\r\n"
                "  ratio: .5\r\n  whole: 1.\r\n  negative: -.25\r\n  text: yes\r\n"
                "  url: https://example.test/a#b\r\nworkers:\r\n  - name: alpha\r\n"
                "    tasks:\r\n      - [one, {two: 2,},]\r\n      -\r\n"
                "  - name: beta\r\n    active: FALSE\r\n... # end\r\n");
    child = mulib_json_get(v, "server");
    assert(mulib_json_boolean(mulib_json_get(child, "enabled")));
    assert(mulib_json_numeric(mulib_json_get(child, "port")) == 80);
    assert(mulib_json_numeric(mulib_json_get(child, "ratio")) == .5);
    assert(mulib_json_numeric(mulib_json_get(child, "whole")) == 1);
    assert(mulib_json_numeric(mulib_json_get(child, "negative")) == -.25);
    string_is(mulib_json_get(child, "text"), "yes", 3);
    string_is(mulib_json_get(child, "url"), "https://example.test/a#b", strlen("https://example.test/a#b"));
    list = mulib_json_get(v, "workers"); assert(mulib_json_size(list) == 2);
    assert(mulib_json_size(mulib_json_get(mulib_json_at(list, 0), "tasks")) == 2);
    mulib_json_destroy(v);
    v = yaml_ok("values:\n- one\n- name: two\n  nested:\n  - 3\nafter: null\n");
    assert(mulib_json_size(mulib_json_get(v, "values")) == 2);
    assert(mulib_json_typeof(mulib_json_get(v, "after")) == MULIB_JSON_NULL);
    mulib_json_destroy(v);
    v = yaml_ok("{a:b: 1, http://example.test: 2, list:[3], \"quoted\":4}");
    assert(mulib_json_numeric(mulib_json_get(v, "a:b")) == 1);
    assert(mulib_json_numeric(mulib_json_get(v, "http://example.test")) == 2);
    assert(mulib_json_numeric(mulib_json_get(v, "quoted")) == 4);
    mulib_json_destroy(v);
    v = yaml_ok("\",foo\": 1\n'- a': 2\n-foo: 3\nfoo,bar: 4\n-: 5\nfoo-bar: 6\n");
    assert(mulib_json_size(v) == 6);
    assert(mulib_json_numeric(mulib_json_get(v, ",foo")) == 1);
    assert(mulib_json_numeric(mulib_json_get(v, "- a")) == 2);
    assert(mulib_json_numeric(mulib_json_get(v, "-foo")) == 3);
    assert(mulib_json_numeric(mulib_json_get(v, "foo,bar")) == 4);
    assert(mulib_json_numeric(mulib_json_get(v, "-")) == 5);
    assert(mulib_json_numeric(mulib_json_get(v, "foo-bar")) == 6);
    mulib_json_destroy(v);
    v = yaml_ok("{',foo': 1, \"- a\": 2, -foo: 3, -: 4, foo-bar: 5}");
    assert(mulib_json_size(v) == 5);
    assert(mulib_json_numeric(mulib_json_get(v, ",foo")) == 1);
    assert(mulib_json_numeric(mulib_json_get(v, "- a")) == 2);
    assert(mulib_json_numeric(mulib_json_get(v, "-foo")) == 3);
    assert(mulib_json_numeric(mulib_json_get(v, "-")) == 4);
    assert(mulib_json_numeric(mulib_json_get(v, "foo-bar")) == 5);
    mulib_json_destroy(v);
    v = yaml_ok("\"\": 'it''s fine'\n\"a\\u0000b\": \"\\uD83D\\uDE80\\x21\\0\"\n"
                "\"日本語\": \"café\\U0001f680\"\n");
    string_is(mulib_json_get(v, ""), "it's fine", 9);
    string_is(mulib_json_get_n(v, "a\0b", 3), "\xf0\x9f\x9a\x80!\0", 6);
    string_is(mulib_json_get(v, "日本語"), "café\xf0\x9f\x9a\x80", strlen("café") + 4);
    mulib_json_destroy(v);
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) yaml_bad(bad[i]);
    assert(!mulib_yaml_parse(NULL, 1, &e) && e.message);
    assert(!mulib_yaml_parse("a\0b", 3, &e));
    assert(!mulib_yaml_parse("a: \xc0\x80", 5, &e));
    assert(!mulib_yaml_parse("a: \xf0\x9f", 5, &e));
    assert(!mulib_yaml_parse("a: '\x7f'", 6, &e));
    assert(!mulib_yaml_parse("a: '\xc2\x80'", 7, &e));
    assert(!mulib_yaml_parse("a: '\xef\xbf\xbe'", 8, &e));
    assert(!mulib_yaml_parse("a: 1\rb: 2", 9, &e));
    {
        char *source = (char *)malloc(16);
        assert(source); memcpy(source, "name: copied", 13);
        v = mulib_yaml_parse(source, 12, &e); assert(v);
        memset(source, '?', 16); free(source);
        string_is(mulib_json_get(v, "name"), "copied", 6);
        mulib_json_destroy(v);
    }
    {
        mulib_strbuf_t b;
        mulib_strbuf_init(&b);
        for (i = 0; i < 128; ++i) assert(mulib_strbuf_putc(&b, '['));
        assert(mulib_strbuf_putc(&b, '0'));
        for (i = 0; i < 128; ++i) assert(mulib_strbuf_putc(&b, ']'));
        v = yaml_ok(b.data); mulib_json_destroy(v);
        assert(mulib_strbuf_putc(&b, ']'));
        yaml_bad(b.data);
        mulib_strbuf_free(&b);
        mulib_strbuf_init(&b);
        for (i = 0; i < 129; ++i) assert(mulib_strbuf_putc(&b, '['));
        assert(mulib_strbuf_putc(&b, '0'));
        for (i = 0; i < 129; ++i) assert(mulib_strbuf_putc(&b, ']'));
        yaml_bad(b.data); mulib_strbuf_free(&b);
    }
}
static void yaml_writer_depth_tests(void)
{
    const mulib_json_type types[] = { MULIB_JSON_ARRAY, MULIB_JSON_OBJECT };
    size_t parent_kind, leaf_kind, depth;
    int empty;
    for (parent_kind = 0; parent_kind < 2; ++parent_kind) {
        for (leaf_kind = 0; leaf_kind < 2; ++leaf_kind) {
            for (empty = 0; empty < 2; ++empty) {
                for (depth = 127; depth <= 129; ++depth) {
                    mulib_json_t *root = mulib_json_new(types[parent_kind]), *tail = root, *copy;
                    mulib_yaml_error_t error;
                    mulib_strbuf_t source;
                    char *text;
                    size_t i;
                    assert(root);
                    mulib_strbuf_init(&source);
                    for (i = 1; i < depth; ++i) {
                        mulib_json_t *child = mulib_json_new(types[i + 1 == depth ? leaf_kind : parent_kind]);
                        assert(child);
                        if (types[parent_kind] == MULIB_JSON_ARRAY) {
                            assert(mulib_json_append(tail, child));
                            assert(mulib_strbuf_putc(&source, '['));
                        } else {
                            assert(mulib_json_set(tail, "next", child));
                            assert(mulib_strbuf_append(&source, "{next: "));
                        }
                        tail = child;
                    }
                    if (!empty) {
                        mulib_json_t *value = mulib_json_new(MULIB_JSON_NULL);
                        assert(value);
                        assert(types[leaf_kind] == MULIB_JSON_ARRAY ? mulib_json_append(tail, value) :
                                                                   mulib_json_set(tail, "next", value));
                    }
                    assert(mulib_strbuf_append(&source, types[leaf_kind] == MULIB_JSON_ARRAY ?
                                              (empty ? "[]" : "[null]") : (empty ? "{}" : "{next: null}")));
                    for (i = 1; i < depth; ++i)
                        assert(mulib_strbuf_putc(&source, types[parent_kind] == MULIB_JSON_ARRAY ? ']' : '}'));
                    text = mulib_yaml_stringify(root, &error);
                    if (depth <= 128) {
                        assert(text && !error.message);
                        copy = mulib_yaml_parse(text, strlen(text), &error);
                        assert(copy && !error.message && same_value(root, copy));
                        mulib_json_destroy(copy);
                        copy = mulib_yaml_parse(source.data, source.length, &error);
                        assert(copy && !error.message && same_value(root, copy));
                        mulib_json_destroy(copy);
                    } else {
                        assert(!text && error.message && strcmp(error.message, "YAML nesting exceeds 128") == 0);
                        assert(error.offset == 0 && error.line == 1 && error.column == 1);
                        assert(!mulib_yaml_stringify(root, NULL));
                        assert(!mulib_yaml_parse(source.data, source.length, &error));
                        assert(error.message && strcmp(error.message, "YAML nesting exceeds 128") == 0);
                    }
                    free(text); mulib_strbuf_free(&source); mulib_json_destroy(root);
                }
            }
        }
    }
}
static void toml_tests(void)
{
    static const char *bad[] = {
        "a=1\na=2", "a=1\n[a]", "a.b=1\n[a]", "[a]\nx=1\n[a]", "a={}\na.b=1",
        "[a.b]\nx=1\n[a]\nb.y=2", "[a.b.c]\nx=1\n[a]\nb.c.y=2",
        "[a.b.c]\nx=1\n[a]\nb.y=2\n[a.b]\nz=3",
        "a={b={}}\n[a.b.c]", "a=[]\n[[a]]", "[[a]]\n[a]", "[a]\n[[a]]",
        "a.b=1\na.b.c=2", "a={x=1,x=2}", "a={x.y=1,x={}}", "a={x=1,}",
        "a={\nx=1\n}", "a={x=1 #no\n}", "a=[1 2]", "a=[1,2}", "a=[,]", "a=[1,,2]",
        "a=", "a= #empty", "a=\n1", "a='x' 'y'", "a=\"a\nb\"", "a='a\nb'",
        "a=\"\\q\"", "a=\"\\uD800\"", "a=\"\\U00110000\"", "a=\"\\u12\"",
        "a='unclosed", "a=\"\"\"unclosed", "a=\"\"\"\\  x\"\"\"", "'''key'''=1",
        "a=true false", "a=True", "a=null", "a=unquoted", "a=inf", "a=-inf", "a=nan",
        "a=1979-05-27", "a=07:32:00", "a=1979-05-27T07:32:00Z", "a=01", "a=0_1",
        "a=.5", "a=1.", "a=1e", "a=1e+", "a=1e99999", "a=9007199254740992",
        "a=0x20000000000000", "a=+0x1", "a=0b102", "a=0o8", "a=0x", "a=1_", "a=_1",
        "a=1__2", "a=1_.2", "a=1._2", "a=1e_2", "[a", "[]", "[[a]", "[a] garbage",
        "a..b=1", ".a=1", "a.=1", "=1", "a:b=1", "[\"a\"\n]", "a=1\rb=2"
    };
    mulib_toml_error_t e;
    mulib_json_t *v, *a, *items;
    size_t i;
    v = mulib_toml_parse(NULL, 0, &e);
    assert(v && mulib_json_typeof(v) == MULIB_JSON_OBJECT && mulib_json_size(v) == 0 && !e.message);
    {
        char *empty = mulib_toml_stringify(v, &e);
        assert(empty && empty[0] == 0 && !e.message); free(empty);
    }
    mulib_json_destroy(v);
    v = toml_ok("\xef\xbb\xbf# config\r\n\"\" = 'empty key'\r\n"
                "\"a.b\" . c = +123_456\r\nhex=0xDEAD_beef\r\noct=0o7_55\r\nbin=0b10_10\r\n"
                "fraction=-1_2.5_0e+0_1\r\narray=[1, # comment\r\n 'two', [true,],]\r\n"
                "inline={ dotted.key = 1, another = { flag=false } }\r\n"
                "[parent.child]\r\nname='first'\r\n[parent]\r\nother=true\r\n"
                "[[workers]]\r\nname='alpha'\r\n[workers.options]\r\nx=1\r\n"
                "[[workers]]\r\nname='beta'\r\n[workers.options]\r\nx=2\r\n");
    string_is(mulib_json_get(v, ""), "empty key", 9);
    assert(mulib_json_numeric(mulib_json_get(mulib_json_get(v, "a.b"), "c")) == 123456);
    assert(mulib_json_numeric(mulib_json_get(v, "hex")) == 3735928559.0);
    assert(mulib_json_numeric(mulib_json_get(v, "oct")) == 493);
    assert(mulib_json_numeric(mulib_json_get(v, "bin")) == 10);
    assert(mulib_json_numeric(mulib_json_get(v, "fraction")) == -125);
    assert(mulib_json_size(mulib_json_get(v, "array")) == 3);
    items = mulib_json_get(v, "workers"); assert(mulib_json_size(items) == 2);
    assert(mulib_json_numeric(mulib_json_get(mulib_json_get(mulib_json_at(items, 1), "options"), "x")) == 2);
    mulib_json_destroy(v);
    v = toml_ok("basic=\"café\\U0001F680\\u0000\"\n\"a\\u0000b\"='日本語'\n"
                "literal='C:\\Users\\name'\nml=\"\"\"\nhello\r\nworld\"\"\"\n"
                "trim=\"\"\"\none\\   \n   two\"\"\"\n"
                "quotes=\"\"\"x\"\"y\"\"\"\"\"\nraw='''\nline\r\n\\raw'''\n");
    string_is(mulib_json_get(v, "basic"), "café\xf0\x9f\x9a\x80\0", strlen("café") + 5);
    string_is(mulib_json_get_n(v, "a\0b", 3), "日本語", strlen("日本語"));
    string_is(mulib_json_get(v, "literal"), "C:\\Users\\name", 13);
    string_is(mulib_json_get(v, "ml"), "hello\nworld", 11);
    string_is(mulib_json_get(v, "trim"), "onetwo", 6);
    string_is(mulib_json_get(v, "quotes"), "x\"\"y\"\"", 6);
    string_is(mulib_json_get(v, "raw"), "line\n\\raw", 9);
    mulib_json_destroy(v);
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) toml_bad(bad[i]);
    assert(!mulib_toml_parse(NULL, 1, &e));
    assert(!mulib_toml_parse("a='x\0y'", 7, &e));
    assert(!mulib_toml_parse("a='\xed\xa0\x80'", 7, &e));
    {
        char *source = (char *)malloc(16);
        assert(source); memcpy(source, "name='copied'", 14);
        v = mulib_toml_parse(source, 13, &e); assert(v);
        memset(source, '?', 16); free(source);
        string_is(mulib_json_get(v, "name"), "copied", 6);
        mulib_json_destroy(v);
    }
    a = mulib_json_new(MULIB_JSON_ARRAY); assert(a);
    assert(!mulib_toml_stringify(a, &e) && e.message); mulib_json_destroy(a);
    assert(!mulib_toml_stringify(NULL, &e) && e.message);
    v = mulib_json_new(MULIB_JSON_OBJECT); a = mulib_json_new(MULIB_JSON_NULL);
    assert(v && a && mulib_json_set(v, "null", a));
    assert(!mulib_toml_stringify(v, &e) && e.message); mulib_json_destroy(v);
    {
        mulib_strbuf_t b;
        mulib_strbuf_init(&b); assert(mulib_strbuf_append(&b, "x="));
        for (i = 0; i < 127; ++i) assert(mulib_strbuf_putc(&b, '['));
        assert(mulib_strbuf_putc(&b, '0'));
        for (i = 0; i < 127; ++i) assert(mulib_strbuf_putc(&b, ']'));
        v = toml_ok(b.data); mulib_json_destroy(v); mulib_strbuf_free(&b);
        mulib_strbuf_init(&b); assert(mulib_strbuf_append(&b, "x="));
        for (i = 0; i < 128; ++i) assert(mulib_strbuf_putc(&b, '['));
        assert(mulib_strbuf_putc(&b, '0'));
        for (i = 0; i < 128; ++i) assert(mulib_strbuf_putc(&b, ']'));
        toml_bad(b.data); mulib_strbuf_free(&b);
    }
}
static uint32_t random_state = UINT32_C(0x12345678);
static uint32_t next_random(void)
{ random_state = random_state * UINT32_C(1664525) + UINT32_C(1013904223); return random_state; }
static mulib_json_t *random_value(unsigned depth, int nulls)
{
    static const char text[] = "a\0\"\\\n<>&'é日本語\xf0\x9f\x9a\x80\x7f\xc2\x80\xc2\x85\xc2\x9f\xef\xbf\xbe\xef\xbf\xbf";
    unsigned type = (unsigned)(next_random() % (depth ? 6U : 4U));
    mulib_json_t *v;
    size_t i, count;
    if (type == 0) return nulls ? mulib_json_new(MULIB_JSON_NULL) : mulib_json_bool(0);
    if (type == 1) return mulib_json_bool((int)(next_random() & 1U));
    if (type == 2) return mulib_json_number(((double)(next_random() % 100000) - 50000.0) / 8.0);
    if (type == 3) return mulib_json_string_n(text, sizeof(text) - 1);
    v = mulib_json_new(type == 4 ? MULIB_JSON_ARRAY : MULIB_JSON_OBJECT); assert(v);
    count = next_random() % 5;
    for (i = 0; i < count; ++i) {
        mulib_json_t *child = random_value(depth - 1, nulls); assert(child);
        if (type == 4) assert(mulib_json_append(v, child));
        else {
            char key[3] = { (char)('a' + i), '\0', (char)('0' + i) };
            assert(mulib_json_set_n(v, key, sizeof(key), child));
        }
    }
    return v;
}
static void roundtrip_tests(void)
{
    size_t i;
    for (i = 0; i < 200; ++i) {
        mulib_json_t *v = random_value(5, 1), *copy;
        mulib_yaml_error_t e;
        char *text;
        assert(v); text = mulib_yaml_stringify(v, &e); assert(text && !e.message);
        copy = mulib_yaml_parse(text, strlen(text), &e);
        assert(copy && same_value(v, copy));
        free(text); mulib_json_destroy(copy); mulib_json_destroy(v);
    }
    for (i = 0; i < 200; ++i) {
        mulib_json_t *v = mulib_json_new(MULIB_JSON_OBJECT), *child = random_value(5, 0), *copy;
        mulib_toml_error_t e;
        char *text;
        assert(v && child && mulib_json_set(v, "", child));
        text = mulib_toml_stringify(v, &e); assert(text && !e.message);
        copy = mulib_toml_parse(text, strlen(text), &e);
        assert(copy && same_value(v, copy));
        free(text); mulib_json_destroy(copy); mulib_json_destroy(v);
    }
    {
        mulib_json_t *v = mulib_json_new(MULIB_JSON_OBJECT), *copy;
        char *yaml, *toml;
        assert(v && mulib_json_set(v, "huge", mulib_json_number(1e100)) &&
               mulib_json_set(v, "large_integer_double", mulib_json_number(9007199254740992.0)) &&
               mulib_json_set(v, "small", mulib_json_number(1e-100)));
        yaml = mulib_yaml_stringify(v, NULL); toml = mulib_toml_stringify(v, NULL);
        assert(yaml && toml);
        copy = yaml_ok(yaml); assert(same_value(v, copy)); mulib_json_destroy(copy);
        copy = toml_ok(toml); assert(same_value(v, copy)); mulib_json_destroy(copy);
        free(yaml); free(toml); mulib_json_destroy(v);
    }
}
static void render_is(const char *source, const mulib_json_t *context, const char *expected)
{
    mulib_template_error_t e;
    size_t n;
    char *result = mulib_template_render(source, strlen(source), context, &n, &e);
    if (!result) fprintf(stderr, "Template at %zu:%zu: %s\n%s\n", e.line, e.column, e.message, source);
    assert(result && !e.message && n == strlen(expected) && memcmp(result, expected, n + 1) == 0);
    free(result);
}
static void template_tests(void)
{
    const char json[] = "{\"name\":\"root\",\"html\":\"<>&\\\"'\",\"truth\":true,"
                        "\"zero\":0,\"empty\":[],\"items\":[{\"name\":\"é\",\"ok\":true},"
                        "{\"name\":\"日本語\",\"ok\":false}],\"obj\":{\"a\":1,\"b\":2}}";
    static const char *bad[] = {
        "{{", "{{x", "{{}}", "{{{x}}", "{{#unknown x}}", "{{#if}}", "{{#if x}}",
        "{{else}}", "{{/if}}", "{{#if x}}{{/each}}", "{{#each x}}{{/if}}",
        "{{#if x}}{{else}}{{else}}{{/if}}", "{{#if missing}}{{bad path}}{{/if}}",
        "{{#each empty}}{{#if x}}{{/each}}{{/if}}", "{{#if x}}{{else bad}}{{/if}}",
        "{{a..b}}", "{{a.}}", "{{@unknown}}", "{{../}}", "{{@root.}}", "{{x[0]}}"
    };
    mulib_json_t *context = mulib_json_parse(json, sizeof(json) - 1, NULL);
    mulib_template_error_t e;
    size_t i, n;
    char *before, *after, *result;
    assert(context);
    before = mulib_json_stringify(context, 0); assert(before);
    render_is("", context, "");
    render_is("{{html}}|{{{html}}}", context, "&lt;&gt;&amp;&quot;&#39;|<>&\"'");
    render_is("{{! comment}}hi {{missing}} {{name}}", context, "hi  root");
    render_is("{{#if truth}}T{{#if zero}}bad{{else}}Z{{/if}}{{else}}bad{{/if}}", context, "TZ");
    render_is("{{#if empty}}bad{{else}}empty{{/if}}", context, "empty");
    render_is("{{#each items}}{{@index}}={{name}}/{{../name}}/{{@root.name}}:"
              "{{#if ok}}yes{{else}}no{{/if}};{{else}}bad{{/each}}", context,
              "0=é/root/root:yes;1=日本語/root/root:no;");
    render_is("{{#each obj}}{{@key}}:{{.}};{{/each}}", context, "a:1;b:2;");
    render_is("{{#each empty}}bad{{else}}{{#if truth}}none{{/if}}{{/each}}", context, "none");
    render_is("{{items.1.name}}/{{this.name}}/{{@index}}/{{@key}}/{{../name}}", context, "日本語/root///");
    render_is("{{#each name}}bad{{else}}not a container{{/each}}", context, "not a container");
    render_is("{{missing}}", NULL, "");
    render_is("{{#each items}}{{#each @root.obj}}{{../../name}}/{{../name}};{{/each}}{{/each}}",
              context, "root/é;root/é;root/日本語;root/日本語;");
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        n = 999;
        result = mulib_template_render(bad[i], strlen(bad[i]), context, &n, &e);
        if (result) fprintf(stderr, "Template unexpectedly accepted: %s\n", bad[i]);
        assert(!result && n == 0 && e.message && e.line >= 1 && e.column >= 1);
        assert(!mulib_template_render(bad[i], strlen(bad[i]), context, NULL, NULL));
    }
    assert(!mulib_template_render(NULL, 1, context, &n, &e) && n == 0);
    assert(!mulib_template_render("\xc0\x80", 2, context, &n, &e));
    result = mulib_template_render(NULL, 0, NULL, &n, &e);
    assert(result && n == 0 && result[0] == 0 && !e.message); free(result);
    after = mulib_json_stringify(context, 0); assert(after && strcmp(before, after) == 0);
    free(after); free(before);
    assert(mulib_json_set(context, "日本語", mulib_json_string("✓")));
    render_is("{{日本語}}", context, "✓");
    {
        const char source[] = "A\0{{nul}}";
        assert(mulib_json_set(context, "nul", mulib_json_string_n("x\0y", 3)));
        result = mulib_template_render(source, sizeof(source) - 1, context, &n, &e);
        assert(result && n == 5 && memcmp(result, "A\0x\0y", 5) == 0 && result[n] == 0);
        free(result);
    }
    {
        mulib_strbuf_t b;
        mulib_strbuf_init(&b);
        for (i = 0; i < 128; ++i) assert(mulib_strbuf_append(&b, "{{#if truth}}"));
        assert(mulib_strbuf_append(&b, "yes"));
        for (i = 0; i < 128; ++i) assert(mulib_strbuf_append(&b, "{{/if}}"));
        render_is(b.data, context, "yes"); mulib_strbuf_free(&b);
        mulib_strbuf_init(&b);
        for (i = 0; i < 129; ++i) assert(mulib_strbuf_append(&b, "{{#if truth}}"));
        result = mulib_template_render(b.data, b.length, context, &n, &e);
        assert(!result && e.message && n == 0); mulib_strbuf_free(&b);
    }
    mulib_json_destroy(context);
}
static void mutation_tests(void)
{
    const char yaml[] = "a: [1, {b: 'text'}, null]\nitems:\n  - name: first\n    flag: true\n";
    const char toml[] = "a=[1, {b='text'}, false]\n[items]\nname='first'\nflag=true\n";
    const char template_source[] = "a{{#if flag}}b{{#each items}}{{name}}{{else}}c{{/each}}{{else}}d{{/if}}e";
    const char alphabet[] = "abc0129[]{}:,.+-_&*!?|>\"'\\#= \t\r\n\0\xff";
    size_t i;
    for (i = 0; i < 6000; ++i) {
        char bytes[128];
        unsigned module = (unsigned)(next_random() % 3);
        const char *seed = module == 0 ? yaml : module == 1 ? toml : template_source;
        size_t length = strlen(seed), j, changes = 1 + next_random() % 4;
        memcpy(bytes, seed, length);
        if (next_random() % 4 == 0) length = next_random() % (length + 1);
        for (j = 0; j < changes && length; ++j)
            bytes[next_random() % length] = alphabet[next_random() % (sizeof(alphabet) - 1)];
        if (module == 0) {
            mulib_json_t *value = mulib_yaml_parse(bytes, length, NULL);
            if (value) {
                char *text = mulib_yaml_stringify(value, NULL);
                mulib_json_t *copy;
                assert(text); copy = mulib_yaml_parse(text, strlen(text), NULL);
                assert(copy && same_value(value, copy));
                free(text); mulib_json_destroy(copy); mulib_json_destroy(value);
            }
        } else if (module == 1) {
            mulib_json_t *value = mulib_toml_parse(bytes, length, NULL);
            if (value) {
                char *text = mulib_toml_stringify(value, NULL);
                mulib_json_t *copy;
                assert(text); copy = mulib_toml_parse(text, strlen(text), NULL);
                assert(copy && same_value(value, copy));
                free(text); mulib_json_destroy(copy); mulib_json_destroy(value);
            }
        } else {
            char *text = mulib_template_render(bytes, length, NULL, NULL, NULL);
            free(text);
        }
    }
}
int main(void)
{
    yaml_tests();
    yaml_writer_depth_tests();
    toml_tests();
    roundtrip_tests();
    template_tests();
    mutation_tests();
    puts("config tests passed");
    return 0;
}
