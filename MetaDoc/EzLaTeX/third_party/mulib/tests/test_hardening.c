/* Always-moving realloc and injectable allocation failures turn stale aliases
 * and incomplete failure cleanup into deterministic regression failures. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
typedef struct allocation { void *pointer; size_t size; } allocation;
static allocation allocations[8192];
static size_t live, calls, fail_at;
static allocation *record(void *p) {
    for (size_t i = 0; i < 8192; ++i) if (allocations[i].pointer == p) return &allocations[i];
    assert(!"unknown allocation"); return NULL;
}
static void *hard_malloc(size_t n) {
    void *p; allocation *r;
    if (++calls == fail_at) return NULL;
    p = malloc(n ? n : 1); if (!p) return NULL;
    r = record(NULL); r->pointer = p; r->size = n; ++live; return p;
}
static void hard_free(void *p) {
    if (p) { allocation *r = record(p); r->pointer = NULL; --live; free(p); }
}
static void *hard_calloc(size_t n, size_t stride) {
    void *p;
    if (stride && n > SIZE_MAX/stride) return NULL;
    p = hard_malloc(n*stride); if (p) memset(p,0,n*stride); return p;
}
static void *hard_realloc(void *p, size_t n) {
    allocation *r; void *next;
    if (!p) return hard_malloc(n);
    if (!n) { hard_free(p); return NULL; }
    if (++calls == fail_at) return NULL;
    r = record(p); next = malloc(n); if (!next) return NULL;
    memcpy(next,p,n < r->size ? n : r->size); free(p); r->pointer = next; r->size = n; return next;
}
#define malloc hard_malloc
#define calloc hard_calloc
#define realloc hard_realloc
#define free hard_free
#include "implementation.c"
#undef malloc
#undef calloc
#undef realloc
#undef free

static void aliases(void) {
    mulib_vec_t v; mulib_stack_t *stack; mulib_queue_t q; mulib_deque_t d; int value;
    mulib_vec_init(&v,sizeof(int));
    for (int i = 0; i < 8; ++i) assert(mulib_vec_push(&v,&i));
    assert(mulib_vec_push(&v,mulib_vec_at(&v,0)) && *(int *)mulib_vec_at(&v,8) == 0);
    fail_at = calls+1; assert(!mulib_vec_reserve(&v,100) && v.length == 9); fail_at = 0;
    assert(!mulib_vec_push(&v,v.data+v.length*v.stride)); mulib_vec_free(&v);
    stack = mulib_stack_create(); assert(stack && mulib_stack_set_stride(stack,sizeof(int)));
    for (int i = 0; i < 8; ++i) assert(mulib_stack_push(stack,&i));
    assert(mulib_stack_push(stack,mulib_stack_peek(stack)) && mulib_stack_pop(stack,&value) && value == 7);
    mulib_stack_destroy(stack);
    mulib_queue_init(&q,sizeof(int));
    for (int i = 0; i < 8; ++i) assert(mulib_queue_push(&q,&i));
    for (int i = 0; i < 3; ++i) assert(mulib_queue_pop(&q,&value) && value == i);
    for (int i = 8; i < 11; ++i) assert(mulib_queue_push(&q,&i));
    fail_at = calls+1; assert(!mulib_queue_push(&q,q.data+q.head*q.stride) && q.length == 8); fail_at = 0;
    assert(mulib_queue_push(&q,q.data+q.head*q.stride));
    for (int i = 3; i < 11; ++i) assert(mulib_queue_pop(&q,&value) && value == i);
    assert(mulib_queue_pop(&q,&value) && value == 3); mulib_queue_free(&q);
    for (unsigned front = 0; front < 2; ++front) {
        mulib_deque_init(&d,sizeof(int));
        for (int i = 0; i < 8; ++i) assert(mulib_deque_push_back(&d,&i));
        for (int i = 0; i < 3; ++i) assert(mulib_deque_pop_front(&d,&value) && value == i);
        for (int i = 8; i < 11; ++i) assert(mulib_deque_push_back(&d,&i));
        assert(front ? mulib_deque_push_front(&d,d.data+d.head*d.stride) : mulib_deque_push_back(&d,d.data+d.head*d.stride));
        if (front) assert(mulib_deque_pop_front(&d,&value) && value == 3);
        for (int i = 3; i < 11; ++i) assert(mulib_deque_pop_front(&d,&value) && value == i);
        if (!front) assert(mulib_deque_pop_front(&d,&value) && value == 3);
        mulib_deque_free(&d);
    }
    { mulib_archive_writer_t w; mulib_archive_reader_t r; mulib_archive_entry_t e;
      const char payload[] = "payload"; mulib_archive_writer_init(&w);
      assert(mulib_archive_add(&w,"first",payload,sizeof(payload),0644,0) == MULIB_ARCHIVE_OK);
      assert(mulib_archive_add(&w,"second",w.bytes.data+512,sizeof(payload),0644,0) == MULIB_ARCHIVE_OK);
      assert(mulib_archive_finish(&w) == MULIB_ARCHIVE_OK && mulib_archive_reader_init(&r,w.bytes.data,w.bytes.length) == MULIB_ARCHIVE_OK);
      assert(mulib_archive_next(&r,&e) == MULIB_ARCHIVE_ENTRY && !memcmp(e.data,payload,sizeof(payload)));
      assert(mulib_archive_next(&r,&e) == MULIB_ARCHIVE_ENTRY && !memcmp(e.data,payload,sizeof(payload)));
      mulib_archive_writer_free(&w); }
    assert(!live);
}
#define FAIL_INIT(type, init, release) do { \
    type value = {0}, before; assert(init); before = value; \
    fail_at = calls+1; assert(!(init)); fail_at = 0; assert(!memcmp(&value,&before,sizeof(value))); \
    release; assert(!live); \
} while (0)
static void initialization(void) {
    FAIL_INIT(mulib_bitset_t,mulib_bitset_init(&value,8),mulib_bitset_free(&value));
    FAIL_INIT(mulib_bitmap_t,mulib_bitmap_init(&value,8,8),mulib_bitmap_free(&value));
    FAIL_INIT(mulib_bloom_t,mulib_bloom_init(&value,64,2),mulib_bloom_free(&value));
    FAIL_INIT(mulib_grid_t,mulib_grid_init(&value,2,2,4),mulib_grid_free(&value));
    FAIL_INIT(mulib_graph_t,mulib_graph_init(&value,4),mulib_graph_free(&value));
    FAIL_INIT(mulib_matrix_t,mulib_matrix_init(&value,2,2),mulib_matrix_free(&value));
    FAIL_INIT(mulib_movingavg_t,mulib_movingavg_init(&value,8),mulib_movingavg_free(&value));
    FAIL_INIT(mulib_histogram_t,mulib_histogram_init(&value,8,0,1),mulib_histogram_free(&value));
    FAIL_INIT(mulib_ring_t,mulib_ring_init(&value,8,4),mulib_ring_free(&value));
    for (unsigned fail = 1; fail <= 2; ++fail) {
        mulib_dsu_t d, before; assert(mulib_dsu_init(&d,8)); before = d;
        fail_at = calls+fail; assert(!mulib_dsu_init(&d,16)); fail_at = 0;
        assert(!memcmp(&d,&before,sizeof(d))); mulib_dsu_free(&d); assert(!live);
    }
    { mulib_grid_t grid; mulib_matrix_t matrix;
      assert(mulib_grid_init(&grid,4,0,1) && !mulib_grid_at(&grid,0,0)); mulib_grid_free(&grid);
      assert(mulib_matrix_init(&matrix,4,0)); mulib_matrix_free(&matrix); }
}
#undef FAIL_INIT
static void codecs(void) {
    unsigned char bytes[32], before[32]; size_t written = 99;
    memset(before,0xa5,sizeof(before)); memcpy(bytes,before,sizeof(bytes));
    assert(!mulib_base64_decode("YWJjAB==",8,bytes,sizeof(bytes),&written));
    assert(written == 99 && !memcmp(bytes,before,sizeof(bytes)));
    assert(!mulib_base64_decode("YWJjZA==",8,bytes,3,&written) && !memcmp(bytes,before,sizeof(bytes)));
    assert(!mulib_hex_decode("6162zz",6,bytes,sizeof(bytes)) && !memcmp(bytes,before,sizeof(bytes)));
    assert(!mulib_url_decode("abc%GG",6,bytes,sizeof(bytes),&written) && !memcmp(bytes,before,sizeof(bytes)));
    { unsigned char bad[] = {2,'a',0,'b'}; assert(!mulib_rle_decode(bad,sizeof(bad),bytes,sizeof(bytes),&written) && !memcmp(bytes,before,sizeof(bytes))); }
    assert(mulib_base64_decode(NULL,0,NULL,0,&written) && !written);
    assert(mulib_hex_decode(NULL,0,NULL,0) && mulib_url_decode(NULL,0,NULL,0,&written) && !written);
    memcpy(bytes,"YWJjZA==",8); assert(mulib_base64_decode((char *)bytes,8,bytes,sizeof(bytes),&written) && written == 4 && !memcmp(bytes,"abcd",4));
    memcpy(bytes,"a%20b",5); assert(mulib_url_decode((char *)bytes,5,bytes,sizeof(bytes),&written) && written == 3 && !memcmp(bytes,"a b",3));
}
static int csv_count(void *user, const char *const *fields, size_t n) {
    size_t *count = (size_t *)user; assert(fields && n); ++*count; return 1;
}
static void text_formats(void) {
    size_t count = 0; mulib_semver_t v = {1,2,3}, before = v;
    static const char *const invalid[] = {"-1.2.3","+1.2.3"," 1.2.3","1.2.3 ","1.02.3","1.2.3x","42949672960.2.3","1..3"};
    for (size_t i = 0; i < sizeof(invalid)/sizeof(*invalid); ++i) assert(!mulib_semver_parse(invalid[i],&v) && !memcmp(&v,&before,sizeof(v)));
    assert(!mulib_csv_parse("\"a\"junk,b",csv_count,&count) && !count);
    assert(!mulib_csv_parse("a\"b,c",csv_count,&count) && !count);
    assert(mulib_csv_parse("\"a\"\"b\",c\r\nx,",csv_count,&count) && count == 2);
    assert(!mulib_sexpr_parse("\"",NULL) && !mulib_sexpr_parse("\"abc\\\"",NULL));
    for (unsigned length = 0; length < 300; ++length) {
        char text[512]; mulib_sexpr_t *s, *again; char *rendered;
        memset(text,'(',100); text[100] = '"'; memset(text+101,'x',length); text[101+length] = '"';
        memset(text+102+length,')',100); text[202+length] = 0;
        s = mulib_sexpr_parse(text,NULL); assert(s); rendered = mulib_sexpr_render(s); assert(rendered);
        again = mulib_sexpr_parse(rendered,NULL); assert(again); mulib_sexpr_destroy(again); hard_free(rendered); mulib_sexpr_destroy(s);
    }
    { mulib_sexpr_t *root = mulib_sexpr_cont(), *child = mulib_sexpr_cont(); assert(root && child);
      assert(!mulib_sexpr_push(root,root) && mulib_sexpr_push(root,child) && !mulib_sexpr_push(root,child) && !mulib_sexpr_push(child,root)); mulib_sexpr_destroy(root); }
    { mulib_manpage_t *m = mulib_manpage_linux(); char title[4097], *rendered;
      memset(title,'X',4096); title[4096] = 0; assert(m && mulib_manpage_set(m,title,"1","synopsis","description-at-end"));
      rendered = mulib_manpage_render(m); assert(rendered && strstr(rendered,"description-at-end")); hard_free(rendered);
      assert(mulib_manpage_set(m,".so secret","1",".sy command","'danger")); rendered = mulib_manpage_render(m);
      assert(rendered && strstr(rendered,"\n\\&.so secret") && strstr(rendered,"\n\\&.sy command") && strstr(rendered,"\n\\&'danger"));
      hard_free(rendered); mulib_manpage_destroy(m); }
    assert(!live);
}
static void tcl(void) {
    mulib_microtcl_t *t = mulib_microtcl_loadall(); char long_value[101], script[256];
    assert(t); memset(long_value,'a',100); long_value[100] = 0;
    snprintf(script,sizeof(script),"set x %s",long_value); assert(mulib_microtcl_eval(t,script));
    assert(mulib_microtcl_eval(t,"set y $x.abcdefghijklmnopqrstuvwxyz"));
    assert(strlen(mulib_microtcl_get(t,"y")) == 127);
    assert(mulib_microtcl_eval(t,"set z \\$x") && !strcmp(mulib_microtcl_get(t,"z"),"$x"));
    assert(mulib_microtcl_eval(t,"set\vx\f42") && !strcmp(mulib_microtcl_get(t,"x"),"42"));
    assert(mulib_microtcl_eval(t,"set x {}") && !mulib_microtcl_eval(t,"incr x"));
    memset(script,' ',sizeof(script)); for (unsigned i = 0; i < 65; ++i) script[2*i] = 'x'; script[130] = 0;
    { size_t saved = live; assert(!mulib_microtcl_eval(t,script) && live == saved); }
    t->errors = INT_MAX; assert(!mulib_microtcl_eval(t,"unknown") && t->errors == INT_MAX);
    mulib_microtcl_destroy(t); assert(!live);
    for (size_t failure = 1; failure <= 60; ++failure) {
        fail_at = calls+failure; t = mulib_microtcl_loadall();
        if (t) { (void)mulib_microtcl_eval(t,"set x abc; set y $x.abcdefgh; append x def; incr counter"); mulib_microtcl_destroy(t); }
        fail_at = 0; assert(!live);
    }
}
static int trial_prime(unsigned n) {
    if (n < 2) return 0;
    for (unsigned i = 2; i <= n/i; ++i) if (n%i == 0) return 0;
    return 1;
}
static void numerical(void) {
    mulib_histogram_t h; mulib_color_rgb_t rgb;
    assert(!mulib_histogram_init(&h,4,-INFINITY,1) && !mulib_histogram_init(&h,4,0,INFINITY));
    assert(mulib_histogram_init(&h,4,-DBL_MAX,DBL_MAX));
    assert(mulib_histogram_add(&h,-DBL_MAX) && mulib_histogram_add(&h,0) && mulib_histogram_add(&h,DBL_MAX));
    assert(h.bins[0] == 1 && h.bins[2] == 1 && h.bins[3] == 1 && !mulib_histogram_add(&h,NAN)); mulib_histogram_free(&h);
    assert(mulib_interpolate_linear(-DBL_MAX,DBL_MAX,0) == -DBL_MAX && mulib_interpolate_linear(-DBL_MAX,DBL_MAX,1) == DBL_MAX);
    assert(mulib_interpolate_linear(-DBL_MAX,DBL_MAX,0.5) == 0 && mulib_interpolate_clamped(-DBL_MAX,DBL_MAX,2) == DBL_MAX);
    rgb = mulib_color_from_hsv((mulib_color_hsv_t){-420,1,1}); assert(rgb.r == 1 && rgb.g == 0 && rgb.b == 1);
    for (unsigned i = 0; i < 5000; ++i) assert(mulib_prime_is_prime(i) == trial_prime(i));
    assert(!mulib_prime_is_prime(UINT64_C(341550071728321)) && !mulib_prime_is_prime(UINT64_C(3825123056546413051)));
    assert(mulib_prime_is_prime(UINT64_C(18446744073709551557)) && !mulib_prime_is_prime(UINT64_MAX));
    assert(mulib_prime_next(UINT64_C(18446744073709551556)) == UINT64_C(18446744073709551557) && !mulib_prime_next(UINT64_MAX));
    assert(!live);
}
static void lazy_depth(void) {
    mulib_lazystream_t *s = mulib_lazystream_array(NULL,0), *next; void *value;
    assert(s);
    for (unsigned i = 1; i < 256; ++i) { next = mulib_lazystream_take(s,1); assert(next); s = next; }
    assert(!mulib_lazystream_take(s,1) && mulib_lazystream_next(s,&value) == 0);
    mulib_lazystream_destroy(s); assert(!live);
}
static void allocating_operation(unsigned kind) {
    static const char json[] = "{\"array\":[1,2,true,null,\"\\u20ac\"],\"nested\":{\"x\":\"hello\"}}";
    if (kind == 0) { mulib_json_t *v = mulib_json_parse(json,sizeof(json)-1,NULL); mulib_json_destroy(v); }
    else if (kind == 1) { const char s[] = "a:\n  - one\n  - two\nb: {x: 42}\n"; mulib_json_t *v = mulib_yaml_parse(s,sizeof(s)-1,NULL); mulib_json_destroy(v); }
    else if (kind == 2) { const char s[] = "a = [1, 2, 3]\n[table]\nx = 'hello'\n"; mulib_json_t *v = mulib_toml_parse(s,sizeof(s)-1,NULL); mulib_json_destroy(v); }
    else if (kind == 3) { const char s[] = "<root x=\"one&amp;two\"><a>text</a><!--comment--></root>"; mulib_xml_node_t *v = mulib_xml_parse(s,sizeof(s)-1,NULL); mulib_xml_destroy(v); }
    else if (kind == 4) { const char s[] = "(ab|c[0-9]){2,4}"; mulib_regex_t *r = mulib_regex_compile(s,sizeof(s)-1,NULL); mulib_regex_destroy(r); }
    else if (kind == 5) { const char s[] = "root <- ('a' / [b-d])+ !."; mulib_peg_t *g = mulib_peg_compile(s,sizeof(s)-1,NULL); mulib_peg_destroy(g); }
    else if (kind == 6) { const char s[] = "a = 42; section 'title' { x = {1, 2, 3}; }"; mulib_confuse_t *v = mulib_confuse_parse(s,sizeof(s)-1,NULL); mulib_confuse_destroy(v); }
    else if (kind == 7) { size_t rows = 0; (void)mulib_csv_parse("a,b,c\n\"x\"\"y\",z,\n",csv_count,&rows); }
    else if (kind == 8) { mulib_sexpr_t *s = mulib_sexpr_parse("(one (two three) \"four\\nfive\")",NULL); if (s) { char *text = mulib_sexpr_render(s); hard_free(text); } mulib_sexpr_destroy(s); }
    else if (kind == 9) { const char s[] = "# title\n\n* one\n* **two**\n\n[text](https://example.org)"; char *text = mulib_markdown_to_html(s,sizeof(s)-1,NULL,NULL); hard_free(text); }
    else if (kind == 10) { const char s[] = "a { color: red; --x: 'escaped\\\"'; }"; mulib_css_sheet_t *v = mulib_css_parse(s,sizeof(s)-1,NULL); mulib_css_destroy(v); }
    else {
        mulib_json_t *v = mulib_json_parse(json,sizeof(json)-1,NULL);
        if (v) { const char s[] = "{{#each array}}{{this}}{{/each}}"; char *text = mulib_template_render(s,sizeof(s)-1,v,NULL,NULL); hard_free(text); }
        mulib_json_destroy(v);
    }
}
static void parser_failures(void) {
    for (unsigned kind = 0; kind < 12; ++kind) {
        size_t begin = calls, count;
        allocating_operation(kind); assert(!live); count = calls-begin;
        for (size_t failure = 1; failure <= count; ++failure) {
            fail_at = calls+failure; allocating_operation(kind); fail_at = 0; assert(!live);
        }
    }
}
int main(void) { aliases(); initialization(); codecs(); text_formats(); tcl(); numerical(); lazy_depth(); parser_failures(); assert(!live); return 0; }
