#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static size_t language_budget = SIZE_MAX;
static int language_allow(void) {
    if (language_budget == SIZE_MAX) return 1;
    if (!language_budget) return 0;
    --language_budget; return 1;
}
static void *language_malloc(size_t n) { return language_allow() ? malloc(n) : NULL; }
static void *language_calloc(size_t n, size_t s) { return language_allow() ? calloc(n, s) : NULL; }
static void *language_realloc(void *p, size_t n) { return language_allow() ? realloc(p, n) : NULL; }
#define malloc language_malloc
#define calloc language_calloc
#define realloc language_realloc
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_MICROJS
#define MULIB_IMPL_DATALOG
#define MULIB_IMPL_PERLPOD
#define MULIB_IMPL_MICROAUDIO
#define MULIB_IMPL_BROWSERTOOLS
#include "mulib-microjs.h"
#include "mulib-datalog.h"
#include "mulib-perlpod.h"
#include "mulib-microaudio.h"
#include "mulib-browsertools.h"
#undef malloc
#undef calloc
#undef realloc

static mulib_microjs_status_t js_eval(mulib_microjs_t *v, const char *s, mulib_microjs_value_t *out) {
    mulib_microjs_error_t e;
    mulib_microjs_status_t rc = mulib_microjs_eval(v, s, strlen(s), out, &e);
    assert(e.code == rc);
    if (rc) assert(e.message && e.offset <= strlen(s) && e.line && e.column);
    return rc;
}
static void test_microjs(void) {
    mulib_microjs_t *v = mulib_microjs_create(NULL);
    mulib_microjs_value_t r = {0}; mulib_microjs_limits_t l;
    const char *bad[] = {"let", "let x", "let true=1", "(1", "1+", "1 2", "1 == 1", "'x", "'\\u0041'", "while (true) {}", "1e+", "1.", "[1]", "/* comment */", "true && (1+)", "true ? 1 : 2"};
    size_t i;
    assert(v);
    assert(js_eval(v, "", &r) == MULIB_MICROJS_OK && r.type == MULIB_MICROJS_NULL);
    assert(js_eval(v, "let x = 2 + 3 * 4; x = x / 2; x + 1", &r) == MULIB_MICROJS_OK && r.number == 8);
    mulib_microjs_value_free(&r);
    assert(js_eval(v, "let s = 'hi'; s = s + \"!\\n\"; s", &r) == MULIB_MICROJS_OK && !strcmp(r.string, "hi!\n"));
    mulib_microjs_value_free(&r);
    assert(js_eval(v, "x===7 && !(2>=3) && 'a' !== 'b'", &r) == MULIB_MICROJS_OK && r.boolean);
    mulib_microjs_value_free(&r);
    assert(js_eval(v, "false && missing/0 || true", &r) == MULIB_MICROJS_OK && r.boolean);
    mulib_microjs_value_free(&r);
    assert(js_eval(v, "null === null && 1 !== true", &r) == MULIB_MICROJS_OK && r.boolean);
    mulib_microjs_value_free(&r);
    assert(js_eval(v, "-5 % 2 + +3", &r) == MULIB_MICROJS_OK && r.number == 2);
    mulib_microjs_value_free(&r);
    assert(js_eval(v, "1.25e2 + 1e-2", &r) == MULIB_MICROJS_OK && fabs(r.number - 125.01) < 1e-9);
    mulib_microjs_value_free(&r);
    assert(js_eval(v, "x=1/0", &r) == MULIB_MICROJS_ARITHMETIC);
    assert(js_eval(v, "x", &r) == MULIB_MICROJS_OK && r.number == 7); mulib_microjs_value_free(&r);
    assert(js_eval(v, "let x=1", &r) == MULIB_MICROJS_NAME);
    assert(js_eval(v, "absent=1", &r) == MULIB_MICROJS_NAME);
    assert(js_eval(v, "'a'+1", &r) == MULIB_MICROJS_TYPE);
    assert(js_eval(v, "1 && true", &r) == MULIB_MICROJS_TYPE);
    assert(js_eval(v, "true && 1", &r) == MULIB_MICROJS_TYPE);
    assert(js_eval(v, "!1", &r) == MULIB_MICROJS_TYPE);
    assert(js_eval(v, "1e999", &r) == MULIB_MICROJS_ARITHMETIC);
    assert(js_eval(v, "1e308 * 10", &r) == MULIB_MICROJS_ARITHMETIC);
    for (i = 0; i < sizeof bad / sizeof *bad; ++i) assert(js_eval(v, bad[i], &r) != MULIB_MICROJS_OK);
    { const char nul[] = {'1', 0, '+', '2'}; assert(mulib_microjs_eval(v, nul, sizeof nul, &r, NULL) == MULIB_MICROJS_SYNTAX); }
    assert(mulib_microjs_eval(v, NULL, 0, &r, NULL) == MULIB_MICROJS_OK);
    assert(mulib_microjs_eval(v, NULL, 1, &r, NULL) == MULIB_MICROJS_INVALID);
    assert(js_eval(v, "s", &r) == MULIB_MICROJS_OK);
    mulib_microjs_destroy(v);
    assert(r.type == MULIB_MICROJS_STRING && !strcmp(r.string,"hi!\n")); mulib_microjs_value_free(&r);
    l = mulib_microjs_default_limits(); l.variables = 1; l.string_bytes = 3;
    v = mulib_microjs_create(&l); assert(v);
    assert(js_eval(v, "let a='abc'", &r) == MULIB_MICROJS_OK); mulib_microjs_value_free(&r);
    assert(js_eval(v, "a+'d'", &r) == MULIB_MICROJS_LIMIT);
    assert(js_eval(v, "let b=1", &r) == MULIB_MICROJS_LIMIT);
    assert(js_eval(v, "'abcd'", &r) == MULIB_MICROJS_LIMIT); mulib_microjs_destroy(v);
    l.steps = 1; v = mulib_microjs_create(&l); assert(v && js_eval(v, "1", &r) == MULIB_MICROJS_LIMIT); mulib_microjs_destroy(v);
    l.steps = 1000; l.depth = 2; v = mulib_microjs_create(&l);
    assert(v && js_eval(v, "((1))", &r) == MULIB_MICROJS_LIMIT); mulib_microjs_destroy(v);
    l.depth = 64; l.source_bytes = 1; v = mulib_microjs_create(&l);
    assert(v && js_eval(v, "12", &r) == MULIB_MICROJS_LIMIT); mulib_microjs_destroy(v);
    l.depth = 129; assert(!mulib_microjs_create(&l));
}
static mulib_datalog_atom_t dl_atom(const char *p, const mulib_datalog_term_t *t, size_t n) {
    mulib_datalog_atom_t a; a.predicate = p; a.terms = t; a.arity = n; return a;
}
static int dl_row(void *user, const char *const *tuple, size_t n) {
    size_t *rows = (size_t *)user;
    assert(n == 2 && !strcmp(tuple[0], "a") && (!strcmp(tuple[1], "b") || !strcmp(tuple[1], "c")));
    ++*rows; return 0;
}
static int dl_stop(void *user, const char *const *tuple, size_t n) { (void)user; (void)tuple; (void)n; return 1; }
static void test_datalog(void) {
    mulib_datalog_status_t status; mulib_datalog_t *d = mulib_datalog_create(NULL, &status);
    mulib_datalog_term_t ab[] = {{"a",0},{"b",0}}, bc[] = {{"b",0},{"c",0}}, cc[] = {{"c",0},{"c",0}};
    mulib_datalog_term_t xy[] = {{"X",1},{"Y",1}}, xz[] = {{"X",1},{"Z",1}}, zy[] = {{"Z",1},{"Y",1}};
    mulib_datalog_term_t ay[] = {{"a",0},{"Y",1}}, xx[] = {{"X",1},{"X",1}}, unknown[] = {{"missing",0},{"X",1}};
    mulib_datalog_atom_t a, h, body[2]; size_t count, rows = 0;
    assert(d && status == MULIB_DATALOG_OK);
    a = dl_atom("edge", ab, 2); assert(!mulib_datalog_fact(d, &a)); assert(!mulib_datalog_fact(d, &a));
    a = dl_atom("edge", bc, 2); assert(!mulib_datalog_fact(d, &a));
    a = dl_atom("edge", cc, 2); assert(!mulib_datalog_fact(d, &a));
    h = dl_atom("path", xy, 2); body[0] = dl_atom("edge", xy, 2);
    assert(!mulib_datalog_rule(d, &h, body, 1));
    body[0] = dl_atom("path", xz, 2); body[1] = dl_atom("edge", zy, 2);
    assert(!mulib_datalog_rule(d, &h, body, 2));
    assert(!mulib_datalog_saturate(d) && mulib_datalog_fact_count(d) == 7);
    assert(!mulib_datalog_saturate(d));
    a = dl_atom("path", ay, 2); assert(!mulib_datalog_query(d, &a, dl_row, &rows, &count) && count == 2 && rows == 2);
    assert(mulib_datalog_query(d, &a, dl_stop, NULL, &count) == MULIB_DATALOG_STOPPED && count == 1);
    a = dl_atom("path", xx, 2); assert(!mulib_datalog_query(d, &a, NULL, NULL, &count) && count == 1);
    a = dl_atom("path", unknown, 2); assert(!mulib_datalog_query(d, &a, NULL, NULL, &count) && !count);
    a = dl_atom("none", NULL, 0); assert(!mulib_datalog_query(d, &a, NULL, NULL, &count) && !count);
    assert(!mulib_datalog_fact(d, &a)); assert(!mulib_datalog_query(d, &a, NULL, NULL, &count) && count == 1);
    h = dl_atom("unsafe", xy, 2); body[0] = dl_atom("edge", xx, 2);
    assert(mulib_datalog_rule(d, &h, body, 1) == MULIB_DATALOG_UNSAFE);
    assert(mulib_datalog_fact(d, &h) == MULIB_DATALOG_INVALID);
    assert(mulib_datalog_rule(d, &h, body, 0) == MULIB_DATALOG_INVALID);
    a = dl_atom("x", NULL, 1); assert(mulib_datalog_fact(d, &a) == MULIB_DATALOG_INVALID);
    mulib_datalog_destroy(d);
    { mulib_datalog_limits_t l = mulib_datalog_default_limits();
      l.facts = 1; d = mulib_datalog_create(&l, NULL); assert(d);
      a = dl_atom("edge", ab, 2); assert(!mulib_datalog_fact(d, &a));
      h = dl_atom("path", xy, 2); body[0] = dl_atom("edge", xy, 2); assert(!mulib_datalog_rule(d, &h, body, 1));
      assert(mulib_datalog_saturate(d) == MULIB_DATALOG_LIMIT && mulib_datalog_fact_count(d) == 1); mulib_datalog_destroy(d);
      l.facts = 20; l.steps = 1; d = mulib_datalog_create(&l, NULL); assert(d);
      assert(!mulib_datalog_fact(d, &a) && !mulib_datalog_rule(d, &h, body, 1));
      assert(mulib_datalog_saturate(d) == MULIB_DATALOG_LIMIT && mulib_datalog_fact_count(d) == 1); mulib_datalog_destroy(d);
      l.steps = 1000; l.rounds = 1; d = mulib_datalog_create(&l, NULL); assert(d);
      assert(!mulib_datalog_fact(d, &a) && !mulib_datalog_rule(d, &h, body, 1));
      assert(mulib_datalog_saturate(d) == MULIB_DATALOG_LIMIT && mulib_datalog_fact_count(d) == 1); mulib_datalog_destroy(d);
      l.steps = 1; l.rounds = 3; d = mulib_datalog_create(&l, NULL); assert(d);
      assert(!mulib_datalog_rule(d, &h, body, 1) && !mulib_datalog_rule(d, &h, body, 1));
      assert(mulib_datalog_saturate(d) == MULIB_DATALOG_LIMIT && !mulib_datalog_fact_count(d)); mulib_datalog_destroy(d);
      l.symbol_bytes = 1; d = mulib_datalog_create(&l, NULL); assert(d && mulib_datalog_fact(d, &a) == MULIB_DATALOG_LIMIT); mulib_datalog_destroy(d);
      l.arity = 9; assert(!mulib_datalog_create(&l, &status) && status == MULIB_DATALOG_LIMIT); }
    d = mulib_datalog_create(NULL, NULL); assert(d && !mulib_datalog_saturate(d) && !mulib_datalog_fact_count(d)); mulib_datalog_destroy(d);
    { mulib_datalog_term_t terms[8]; mulib_datalog_limits_t l = mulib_datalog_default_limits();
      for (count = 0; count < 8; ++count) { terms[count].name = "v"; terms[count].variable = 0; }
      d = mulib_datalog_create(&l,NULL); assert(d); a = dl_atom("eight",terms,8);
      assert(!mulib_datalog_fact(d,&a)); assert(!mulib_datalog_query(d,&a,NULL,NULL,&count) && count == 1);
      a.arity = 9; assert(mulib_datalog_fact(d,&a) == MULIB_DATALOG_INVALID); mulib_datalog_destroy(d); }
}
static void test_perlpod(void) {
    const char source[] = "code ignored\n=head1 A B<title>\r\n\n=pod\nA B<bold I<inner>> C<x E<lt> y> E<amp> F<file> S<two words>.\nnext\n\n=over 4\n=item *\none\n=over\n=item label\ntwo\n=back\n=item second\nthree\n=back\n\n literal B<text> <&\n=cut\nignored\n";
    const char *bad[] = {"=head5 x", "=encoding utf8", "=pod x", "=cut x", "=head1", "=item x", "=back", "=over\nx", "=over", "=over 0", "=over 17", "=over a", "=pod\nB<open", "=pod\nL<unsupported>", "=pod\nE<unknown>", "=pod\nB<< x >>", "=over\n=cut", "=over\n=over"};
    char *text; size_t n, i; mulib_perlpod_error_t e;
    text = mulib_perlpod_render(source, sizeof source - 1, MULIB_PERLPOD_HTML, NULL, &n, &e);
    assert(text && !e.code && n == strlen(text));
    assert(strstr(text, "<h1>A <strong>title</strong></h1>") && strstr(text, "<strong>bold <em>inner</em></strong>") && strstr(text, "<code>x &lt; y</code> &amp;"));
    assert(strstr(text, "<ul>\n<li>\n<p>one</p>\n<ul>") && strstr(text, "<pre> literal B&lt;text&gt; &lt;&amp;\n</pre>"));
    assert(!strstr(text, "ignored")); free(text);
    text = mulib_perlpod_render(source, sizeof source - 1, MULIB_PERLPOD_TEXT, NULL, &n, &e);
    assert(text && strstr(text, "bold inner x < y & file two words. next") && !strstr(text, "<strong>")); free(text);
    for (i = 0; i < sizeof bad / sizeof *bad; ++i) {
        text = mulib_perlpod_render(bad[i], strlen(bad[i]), MULIB_PERLPOD_HTML, NULL, &n, &e);
        assert(!text && !n && e.code && e.message && e.offset <= strlen(bad[i]) && e.line && e.column);
    }
    text = mulib_perlpod_render(NULL, 0, MULIB_PERLPOD_HTML, NULL, &n, &e); assert(text && !*text && !n); free(text);
    assert(!mulib_perlpod_render("=pod\n\xff", 6, MULIB_PERLPOD_HTML, NULL, &n, &e));
    assert(!mulib_perlpod_render("=pod\n\0", 6, MULIB_PERLPOD_HTML, NULL, &n, &e));
    { const char unicode[] = "=pod\n\xe2\x98\x83 E<quot>E<apos>E<sol>E<verbar>E<gt>";
      text = mulib_perlpod_render(unicode,sizeof unicode-1,MULIB_PERLPOD_TEXT,NULL,&n,&e);
      assert(text && !strcmp(text,"\xe2\x98\x83 \"'/|>\n\n")); free(text); }
    { mulib_perlpod_limits_t l = {100, 9, 1};
      text = mulib_perlpod_render("=pod\nx", 6, MULIB_PERLPOD_HTML, &l, &n, &e); assert(text && n == 9); free(text);
      l.output_bytes = 8; assert(!mulib_perlpod_render("=pod\nx", 6, MULIB_PERLPOD_HTML, &l, &n, &e) && e.code == MULIB_PERLPOD_LIMIT);
      l.output_bytes = 100; assert(!mulib_perlpod_render("=pod\nB<I<x>>", 12, MULIB_PERLPOD_HTML, &l, &n, &e) && e.code == MULIB_PERLPOD_LIMIT);
      l.input_bytes = 1; assert(!mulib_perlpod_render("xx", 2, MULIB_PERLPOD_HTML, &l, &n, &e) && e.code == MULIB_PERLPOD_LIMIT); }
    { mulib_perlpod_limits_t l = {0,0,0};
      text = mulib_perlpod_render(NULL,0,MULIB_PERLPOD_TEXT,&l,&n,&e); assert(text && !n); free(text); }
}
static void test_microaudio(void) {
    { float samples[] = {0.125f, 0.25f, 0.375f, 0.5f};
      mulib_microaudio_pcm_t source = {samples, 3, 8000, 1};
      mulib_microaudio_pcm_t destination = {samples + 1, 3, 8000, 1};
      assert(mulib_microaudio_mix(&destination, 0, &source, 0, 3, 1) == MULIB_MICROAUDIO_OK);
      assert(samples[1] == 0.375f && samples[2] == 0.625f && samples[3] == 0.875f);
      samples[0] = 0.125f; samples[1] = 0.25f; samples[2] = 0.375f; samples[3] = 0.5f;
      assert(mulib_microaudio_mix(&source, 0, &destination, 0, 3, 1) == MULIB_MICROAUDIO_OK);
      assert(samples[0] == 0.375f && samples[1] == 0.625f && samples[2] == 0.875f);
    }
    mulib_microaudio_pcm_t a = {0}, b = {0}; unsigned char *wire = NULL; size_t n, i;
    mulib_microaudio_limits_t l = {8, 100};
    assert(!mulib_microaudio_create(4, 2, 8000, &l, &a));
    assert(!mulib_microaudio_sine(&a, 2000, 1, 0));
    assert(fabs(a.samples[2] - 1) < 1e-6 && fabs(a.samples[6] + 1) < 1e-6);
    assert(!mulib_microaudio_wav_encode(&a, &l, &wire, &n) && n == 60);
    assert(!mulib_microaudio_wav_decode(wire, n, &l, &b) && b.frames == 4 && b.channels == 2 && b.sample_rate == 8000);
    for (i = 0; i < 8; ++i) assert(fabs(a.samples[i] - b.samples[i]) <= 1.0 / 32768);
    { unsigned char *again = NULL; size_t size = 0;
      assert(!mulib_microaudio_wav_encode(&b,&l,&again,&size) && size == n && !memcmp(wire,again,n)); free(again); }
    assert(!mulib_microaudio_mix(&b, 0, &a, 0, 4, 2) && b.samples[2] == 1 && b.samples[6] == -1);
    assert(mulib_microaudio_mix(&b, 1, &a, 0, 4, 1) == MULIB_MICROAUDIO_INVALID);
    assert(mulib_microaudio_sine(&a, 4001, 1, 0) == MULIB_MICROAUDIO_INVALID);
    assert(mulib_microaudio_sine(&a, 1, NAN, 0) == MULIB_MICROAUDIO_INVALID);
    mulib_microaudio_free(&b);
    for (i = 0; i < n; ++i) assert(mulib_microaudio_wav_decode(wire, i, NULL, &b) != MULIB_MICROAUDIO_OK);
    wire[20] = 3; assert(mulib_microaudio_wav_decode(wire, n, NULL, &b) == MULIB_MICROAUDIO_UNSUPPORTED); wire[20] = 1;
    wire[32] = 3; assert(mulib_microaudio_wav_decode(wire, n, NULL, &b) == MULIB_MICROAUDIO_MALFORMED); wire[32] = 4;
    l.samples = 7; assert(mulib_microaudio_wav_decode(wire, n, &l, &b) == MULIB_MICROAUDIO_LIMIT);
    assert(mulib_microaudio_wav_encode(&a, &l, &wire, &n) == MULIB_MICROAUDIO_LIMIT);
    free(wire); mulib_microaudio_free(&a);
    assert(mulib_microaudio_create(SIZE_MAX, 2, 8000, NULL, &a) == MULIB_MICROAUDIO_LIMIT);
    assert(mulib_microaudio_create(1, 0, 8000, NULL, &a) == MULIB_MICROAUDIO_INVALID);
    l.samples = 0; l.bytes = 44;
    assert(mulib_microaudio_create(1,1,8000,&l,&a) == MULIB_MICROAUDIO_LIMIT);
    assert(!mulib_microaudio_create(0, 1, 8000, NULL, &a));
    assert(!mulib_microaudio_wav_encode(&a, &l, &wire, &n) && n == 44);
    assert(!mulib_microaudio_wav_decode(wire, n, NULL, &b) && !b.frames); free(wire);
    mulib_microaudio_free(&a); mulib_microaudio_free(&b);
    assert(!mulib_microaudio_create(4, 1, 8000, NULL, &a));
    a.samples[0] = 0.1f; a.samples[1] = 0.2f; a.samples[2] = 0.3f; a.samples[3] = 0.4f;
    assert(!mulib_microaudio_mix(&a, 1, &a, 0, 3, 1));
    assert(fabs(a.samples[1] - 0.3) < 1e-6 && fabs(a.samples[2] - 0.5) < 1e-6 && fabs(a.samples[3] - 0.7) < 1e-6);
    a.samples[0] = NAN; assert(mulib_microaudio_wav_encode(&a, NULL, &wire, &n) == MULIB_MICROAUDIO_INVALID); mulib_microaudio_free(&a);
    { /* PCM8, odd data padding and an unknown odd-sized chunk. */
      unsigned char pcm8[] = {'R','I','F','F',48,0,0,0,'W','A','V','E',
          'J','U','N','K',1,0,0,0,42,0,'f','m','t',' ',16,0,0,0,1,0,1,0,64,31,0,0,64,31,0,0,1,0,8,0,
          'd','a','t','a',1,0,0,0,255,0};
      assert(!mulib_microaudio_wav_decode(pcm8, sizeof pcm8, NULL, &a)); assert(a.frames == 1 && a.samples[0] == 127.0f/128); mulib_microaudio_free(&a); }
}
typedef struct browser_sink { char *wire; size_t length; int fail; } browser_sink;
static int browser_send(void *user, const char *s, size_t n) {
    browser_sink *sink = (browser_sink *)user;
    if (sink->fail) return 1;
    free(sink->wire); sink->wire = (char *)malloc(n + 1); assert(sink->wire);
    memcpy(sink->wire, s, n); sink->wire[n] = 0; sink->length = n; return 0;
}
static mulib_browsertools_status_t browser_receive(mulib_browsertools_t *c, const char *s, mulib_browsertools_message_t *m) {
    return mulib_browsertools_receive(c, s, strlen(s), m);
}
static void test_browsertools(void) {
    browser_sink sink = {0}; mulib_browsertools_t *c = mulib_browsertools_create(browser_send, &sink, NULL);
    mulib_browsertools_message_t m = {0}; mulib_json_t *j; uint32_t id = 0; size_t i;
    const char *bad[] = {"", "[]", "{}", "{", "{\"id\":1}", "{\"id\":1.5,\"result\":{}}", "{\"id\":0,\"result\":{}}", "{\"id\":1,\"result\":{},\"error\":{}}", "{\"id\":1,\"result\":[]}", "{\"method\":\"\"}", "{\"method\":\"Event\",\"params\":[]}", "{\"id\":1,\"error\":{}}", "{\"id\":1,\"result\":{},\"sessionId\":null}"};
    assert(c && !mulib_browsertools_navigate(c, "https://x/\"?a=1&b=2", "s1", &id) && id == 1);
    j = mulib_json_parse(sink.wire, sink.length, NULL); assert(j);
    assert(!strcmp(mulib_json_text(mulib_json_get(j, "method"), NULL), "Page.navigate"));
    assert(!strcmp(mulib_json_text(mulib_json_get(mulib_json_get(j,"params"),"url"),NULL),"https://x/\"?a=1&b=2")); mulib_json_destroy(j);
    for (i = 0; i < sizeof bad / sizeof *bad; ++i) assert(browser_receive(c, bad[i], &m) < 0 && !m.json && mulib_browsertools_pending(c) == 1);
    assert(browser_receive(c, "{\"id\":1,\"result\":{},\"sessionId\":\"other\"}", &m) == MULIB_BROWSERTOOLS_UNMATCHED);
    mulib_browsertools_message_free(&m); assert(mulib_browsertools_pending(c) == 1);
    assert(browser_receive(c, "{\"method\":\"Page.loadEventFired\",\"params\":{\"timestamp\":1}}", &m) == MULIB_BROWSERTOOLS_EVENT);
    mulib_browsertools_message_free(&m);
    assert(browser_receive(c, "{\"id\":1,\"result\":{},\"sessionId\":\"s1\"}", &m) == MULIB_BROWSERTOOLS_RESPONSE && !m.remote_error);
    mulib_browsertools_message_free(&m); assert(!mulib_browsertools_pending(c));
    assert(!mulib_browsertools_evaluate(c, "document.title", NULL, &id) && id == 2);
    assert(browser_receive(c, "{\"id\":2,\"error\":{\"code\":-1,\"message\":\"denied\"}}", &m) == MULIB_BROWSERTOOLS_RESPONSE && m.remote_error);
    mulib_browsertools_message_free(&m);
    sink.fail = 1; assert(mulib_browsertools_request(c, "DOM.enable", NULL, NULL, &id) == MULIB_BROWSERTOOLS_TRANSPORT && !mulib_browsertools_pending(c) && id == 2);
    sink.fail = 0; assert(!mulib_browsertools_request(c, "DOM.enable", NULL, NULL, &id) && id == 4);
    assert(mulib_browsertools_cancel(c, id) && !mulib_browsertools_cancel(c, id));
    mulib_browsertools_destroy(c); free(sink.wire); sink.wire = NULL;
    { mulib_browsertools_limits_t l = {1, 100};
      c = mulib_browsertools_create(browser_send, &sink, &l); assert(c);
      assert(!mulib_browsertools_request(c,"A.b",NULL,NULL,&id));
      assert(mulib_browsertools_request(c,"A.b",NULL,NULL,&id) == MULIB_BROWSERTOOLS_LIMIT);
      assert(mulib_browsertools_cancel(c,id));
      assert(mulib_browsertools_evaluate(c,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",NULL,&id) == MULIB_BROWSERTOOLS_LIMIT);
      assert(mulib_browsertools_request(c,"\xff",NULL,NULL,&id) == MULIB_BROWSERTOOLS_INVALID);
      { char oversized[101] = {0}; assert(mulib_browsertools_receive(c,oversized,sizeof oversized,&m) == MULIB_BROWSERTOOLS_LIMIT); }
      mulib_browsertools_destroy(c); free(sink.wire); sink.wire = NULL; }
    { mulib_http_message_t r = {0}; const char body[] = "[{\"id\":\"a\",\"type\":\"page\",\"url\":\"about:blank\"}]";
      r.is_response = 1; r.status = 200; r.body = (unsigned char *)(void *)body; r.body_length = sizeof body - 1;
      j = mulib_browsertools_targets(&r, NULL); assert(j && mulib_json_size(j) == 1); mulib_json_destroy(j);
      r.status = 500; assert(!mulib_browsertools_targets(&r, NULL));
      r.status = 200; r.body_length = 2; r.body = (unsigned char *)(void *)"[]";
      j = mulib_browsertools_targets(&r, NULL); assert(j && !mulib_json_size(j)); mulib_json_destroy(j); }
    { mulib_browsertools_limits_t l = {0,0};
      c = mulib_browsertools_create(browser_send,&sink,&l); assert(c);
      assert(mulib_browsertools_request(c,"A.b",NULL,NULL,&id) == MULIB_BROWSERTOOLS_LIMIT);
      mulib_browsertools_destroy(c); }
    { mulib_json_t *params = mulib_json_new(MULIB_JSON_OBJECT), *tail = params; uint32_t last = 0;
      c = mulib_browsertools_create(browser_send,&sink,NULL); assert(c && params);
      for (i = 0; i < 32; ++i) { j = mulib_json_new(MULIB_JSON_OBJECT); assert(j && mulib_json_set(tail,"x",j)); tail = j; }
      assert(!mulib_browsertools_request(c,"A.b",params,NULL,&id) && mulib_browsertools_cancel(c,id));
      j = mulib_json_new(MULIB_JSON_OBJECT); assert(j && mulib_json_set(tail,"x",j));
      assert(mulib_browsertools_request(c,"A.b",params,NULL,&id) == MULIB_BROWSERTOOLS_LIMIT);
      mulib_json_destroy(params);
      c->next_id = UINT32_MAX; /* Exercise the otherwise impractical ID exhaustion boundary. */
      assert(!mulib_browsertools_request(c,"A.b",NULL,NULL,&last) && last == UINT32_MAX);
      assert(mulib_browsertools_request(c,"A.b",NULL,NULL,&id) == MULIB_BROWSERTOOLS_LIMIT);
      assert(mulib_browsertools_cancel(c,last)); mulib_browsertools_destroy(c); free(sink.wire); sink.wire = NULL; }
}
static void test_allocations(void) {
    size_t i; int success = 0;
    for (i = 0; i < 128; ++i) {
        mulib_microjs_t *v; mulib_microjs_value_t r = {0}; language_budget = i;
        v = mulib_microjs_create(NULL);
        if (v) { success = js_eval(v,"let s='a'; s=s+'b'; s+'c'",&r) == MULIB_MICROJS_OK; mulib_microjs_value_free(&r); mulib_microjs_destroy(v); }
        if (success) break;
    }
    language_budget = SIZE_MAX; assert(success); success = 0;
    { mulib_microjs_t *v = mulib_microjs_create(NULL); mulib_microjs_value_t r = {0}; assert(v);
      assert(!js_eval(v,"let s='old'",&r)); mulib_microjs_value_free(&r);
      language_budget = 1; assert(js_eval(v,"s='new'",&r) == MULIB_MICROJS_MEMORY); language_budget = SIZE_MAX;
      assert(!js_eval(v,"s",&r) && !strcmp(r.string,"old")); mulib_microjs_value_free(&r); mulib_microjs_destroy(v); }
    for (i = 0; i < 128; ++i) {
        mulib_datalog_t *d; mulib_datalog_term_t t[] = {{"a",0}}, x[] = {{"X",1}};
        mulib_datalog_atom_t a = dl_atom("p",t,1), h = dl_atom("q",x,1), b = dl_atom("p",x,1);
        language_budget = i; d = mulib_datalog_create(NULL,NULL);
        if (d) { success = !mulib_datalog_fact(d,&a) && !mulib_datalog_rule(d,&h,&b,1) && !mulib_datalog_saturate(d); mulib_datalog_destroy(d); }
        if (success) break;
    }
    language_budget = SIZE_MAX; assert(success); success = 0;
    { mulib_datalog_limits_t l = mulib_datalog_default_limits(); mulib_datalog_t *d;
      mulib_datalog_term_t ta[] = {{"a",0}}, tb[] = {{"b",0}}, tc[] = {{"c",0}};
      mulib_datalog_atom_t a = dl_atom("p",ta,1), b = dl_atom("q",tb,1), c = dl_atom("r",tc,1);
      l.symbols = 4; d = mulib_datalog_create(&l,NULL); assert(d && !mulib_datalog_fact(d,&a));
      language_budget = 1; assert(mulib_datalog_fact(d,&b) == MULIB_DATALOG_MEMORY); language_budget = SIZE_MAX;
      assert(mulib_datalog_fact_count(d) == 1 && !mulib_datalog_fact(d,&c)); mulib_datalog_destroy(d); }
    for (i = 0; i < 128; ++i) {
        const char *s = "=head1 Hi\n\nB<bold> I<x>\n"; char *out; language_budget = i;
        out = mulib_perlpod_render(s,strlen(s),MULIB_PERLPOD_HTML,NULL,NULL,NULL);
        if (out) { free(out); success = 1; break; }
    }
    language_budget = SIZE_MAX; assert(success); success = 0;
    for (i = 0; i < 128; ++i) {
        mulib_microaudio_pcm_t a = {0}, b = {0}; unsigned char *wire = NULL; size_t n;
        language_budget = i;
        if (!mulib_microaudio_create(8,1,8000,NULL,&a) && !mulib_microaudio_wav_encode(&a,NULL,&wire,&n) &&
            !mulib_microaudio_wav_decode(wire,n,NULL,&b)) success = 1;
        free(wire); mulib_microaudio_free(&a); mulib_microaudio_free(&b); if (success) break;
    }
    language_budget = SIZE_MAX; assert(success); success = 0;
    for (i = 0; i < 256; ++i) {
        browser_sink sink = {0}; mulib_browsertools_t *c; language_budget = i;
        c = mulib_browsertools_create(browser_send,&sink,NULL);
        if (c) { success = !mulib_browsertools_evaluate(c,"1+2",NULL,NULL); mulib_browsertools_destroy(c); }
        free(sink.wire); if (success) break;
    }
    language_budget = SIZE_MAX; assert(success);
    { browser_sink sink = {0}; mulib_browsertools_t *c = mulib_browsertools_create(browser_send,&sink,NULL);
      const char *reply = "{\"id\":1,\"result\":{\"result\":{\"type\":\"string\",\"value\":\"ok\"}}}";
      assert(c && !mulib_browsertools_evaluate(c,"document.title",NULL,NULL)); success = 0;
      for (i = 0; i < 128; ++i) {
          mulib_browsertools_message_t m = {0}; mulib_browsertools_status_t rc;
          m.id = 77; language_budget = i; rc = browser_receive(c,reply,&m);
          if (rc == MULIB_BROWSERTOOLS_RESPONSE) { mulib_browsertools_message_free(&m); success = 1; break; }
          assert(rc < 0 && !m.json && m.id == 77 && mulib_browsertools_pending(c) == 1);
      }
      language_budget = SIZE_MAX; assert(success && !mulib_browsertools_pending(c)); mulib_browsertools_destroy(c); free(sink.wire); }
}
static void test_mutations(void) {
    uint32_t state = 123; char bytes[64]; size_t i, j;
    const char alphabet[] = "=<>/!&|;()+-*. '\"abc012BICE\n\\";
    mulib_microjs_t *v = mulib_microjs_create(NULL); browser_sink sink = {0};
    mulib_browsertools_t *c = mulib_browsertools_create(browser_send,&sink,NULL); assert(v && c);
    for (i = 0; i < 600; ++i) {
        size_t n = i % sizeof bytes; mulib_microjs_value_t r = {0}; mulib_browsertools_message_t m = {0};
        mulib_microaudio_pcm_t a = {0}; char *text;
        for (j = 0; j < n; ++j) { state = state * 1664525u + 1013904223u; bytes[j] = alphabet[state % (sizeof alphabet - 1)]; }
        (void)mulib_microjs_eval(v,bytes,n,&r,NULL); mulib_microjs_value_free(&r);
        text = mulib_perlpod_render(bytes,n,MULIB_PERLPOD_HTML,NULL,NULL,NULL); free(text);
        (void)mulib_microaudio_wav_decode(bytes,n,NULL,&a); mulib_microaudio_free(&a);
        (void)mulib_browsertools_receive(c,bytes,n,&m); mulib_browsertools_message_free(&m);
    }
    mulib_microjs_destroy(v); mulib_browsertools_destroy(c);
}
int main(void) {
    test_microjs(); test_datalog(); test_perlpod(); test_microaudio(); test_browsertools(); test_allocations(); test_mutations();
    return 0;
}
