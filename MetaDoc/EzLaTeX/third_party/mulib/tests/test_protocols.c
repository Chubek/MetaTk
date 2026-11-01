#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_JSON
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_RPC
#define MULIB_IMPL_DBUS
#include "mulib-uri.h"
#include "mulib-http.h"
#include "mulib-rpc.h"
#include "mulib-dbus.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void test_uri(void) {
    static const char *const pairs[][2] = {
        {"g:h", "g:h"}, {"g", "http://a/b/c/g"}, {"./g", "http://a/b/c/g"},
        {"g/", "http://a/b/c/g/"}, {"/g", "http://a/g"}, {"//g", "http://g"},
        {"?y", "http://a/b/c/d;p?y"}, {"g?y", "http://a/b/c/g?y"},
        {"#s", "http://a/b/c/d;p?q#s"}, {"g#s", "http://a/b/c/g#s"},
        {"g?y#s", "http://a/b/c/g?y#s"}, {";x", "http://a/b/c/;x"},
        {"g;x", "http://a/b/c/g;x"}, {"g;x?y#s", "http://a/b/c/g;x?y#s"},
        {"", "http://a/b/c/d;p?q"}, {".", "http://a/b/c/"}, {"./", "http://a/b/c/"},
        {"..", "http://a/b/"}, {"../", "http://a/b/"}, {"../g", "http://a/b/g"},
        {"../..", "http://a/"}, {"../../", "http://a/"}, {"../../g", "http://a/g"},
        {"../../../g", "http://a/g"}, {"../../../../g", "http://a/g"},
        {"/./g", "http://a/g"}, {"/../g", "http://a/g"}, {"g.", "http://a/b/c/g."},
        {".g", "http://a/b/c/.g"}, {"g..", "http://a/b/c/g.."}, {"..g", "http://a/b/c/..g"},
        {"./../g", "http://a/b/g"}, {"./g/.", "http://a/b/c/g/"},
        {"g/./h", "http://a/b/c/g/h"}, {"g/../h", "http://a/b/c/h"},
        {"g;x=1/./y", "http://a/b/c/g;x=1/y"}, {"g;x=1/../y", "http://a/b/c/y"},
        {"g?y/./x", "http://a/b/c/g?y/./x"}, {"g#s/../x", "http://a/b/c/g#s/../x"},
        {"http:g", "http:g"}, {"?", "http://a/b/c/d;p?"}, {"#", "http://a/b/c/d;p?q#"},
        {"%2e/%2E%2e/g", "http://a/b/c/%2e/%2E%2e/g"}
    };
    static const char *const good[] = {
        "", "file:///a", "//", "mailto:a@example.org", "urn:x:y", "/a//b", "a/b:c",
        "http://user:pass@[2001:db8::1]:443/a?b#c", "//[::]", "//[::ffff:192.0.2.1]",
        "//[1:2:3:4:5:6:7:8]", "//[vF.a:b!c]", "//host:", "//host:999999", "x%00y?%23#%ff"
    };
    static const char *const bad[] = {
        "x%", "x%0", "x%gg", "a b", "a\tb", "a\\b", "\xc3\xa9", "1:a", ":x", "a#b#c",
        "http://a:b:c", "http://a:xyz", "http://a@@b", "http://[::1", "http://[::1]x",
        "//[1:2:3:4:5:6:7]", "//[1:2:3:4:5:6:7:8:9]", "//[1::2::3]", "//[:::]",
        "//[::ffff:192.0.2.256]", "//[::ffff:192.00.2.1]", "//[v.a]", "//[v1.]", "//[fe80::1%25eth0]"
    };
    mulib_uri_t base, ref, u;
    assert(mulib_uri_parse("http://a/b/c/d;p?q", strlen("http://a/b/c/d;p?q"), &base));
    for (size_t i = 0; i < sizeof(pairs)/sizeof(pairs[0]); ++i) {
        char *resolved;
        assert(mulib_uri_parse(pairs[i][0], strlen(pairs[i][0]), &ref));
        resolved = mulib_uri_resolve(&base, &ref);
        assert(resolved && !strcmp(resolved, pairs[i][1])); free(resolved);
    }
    for (size_t i = 0; i < sizeof(good)/sizeof(good[0]); ++i) {
        char *composed;
        assert(mulib_uri_parse(good[i], strlen(good[i]), &u));
        composed = mulib_uri_compose(&u); assert(composed && !strcmp(composed, good[i])); free(composed);
    }
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) assert(!mulib_uri_parse(bad[i], strlen(bad[i]), &u));
    assert(!mulib_uri_parse("x\0y", 3, &u));
    assert(mulib_uri_parse("a/b", 3, &u));
    u.path.data = "a?b"; assert(!mulib_uri_compose(&u));
    assert(mulib_uri_parse("http://a", 8, &base) && mulib_uri_parse("b", 1, &ref));
    { char *s = mulib_uri_resolve(&base, &ref); assert(s && !strcmp(s, "http://a/b")); free(s); }
}

static void test_uri_no_authority(void) {
    static const struct { const char *base, *ref, *expected; int authority; } cases[] = {
        {"file:/a/b", "..//c", "file:/.//c", 0},
        {"file:/a/b", "..///c", "file:/.///c", 0},
        {"file:/a/b", "..//", "file:/.//", 0},
        {"file:/a/b", "../..//c", "file:/.//c", 0},
        {"file:/a/b", "/.//c", "file:/.//c", 0},
        {"file:/a/b", "/..//c", "file:/.//c", 0},
        {"file:/a/b", "/a/..//c", "file:/.//c", 0},
        {"file:/a/b", "file:/a/..//c", "file:/.//c", 0},
        {"file:/a/b", "file:/.//c", "file:/.//c", 0},
        {"file:/a/b", "..//c?q#f", "file:/.//c?q#f", 0},
        {"foo:/a/b", "..//evil", "foo:/.//evil", 0},
        {"foo:a/b", "..//c", "foo:/.//c", 0},
        {"file:/a/b", "/%2e//c", "file:/%2e//c", 0},
        {"file:/a/b", "../x//c", "file:/x//c", 0},
        {"file:/a/b", "//c", "file://c", 1},
        {"file:///a/b", "..//c", "file:////c", 1},
        {"http://h/a/b", "..//c", "http://h//c", 1},
        {"file:/a/b", "//h/a/..//c", "file://h//c", 1},
        {"file:/.//c", "", "file:/.//c", 0},
        {"file:/.//c", "?q", "file:/.//c?q", 0}
    };
    static const struct { const char *path, *expected; } paths[] = {
        {"//c", "/.//c"}, {"///c", "/.///c"}, {"//", "/.//"},
        {"//host:80/path", "/.//host:80/path"}, {"/.//c", "/.//c"}
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        mulib_uri_t base, ref, parsed; char *resolved, *composed, *again;
        assert(mulib_uri_parse(cases[i].base, strlen(cases[i].base), &base));
        assert(mulib_uri_parse(cases[i].ref, strlen(cases[i].ref), &ref));
        resolved = mulib_uri_resolve(&base, &ref);
        assert(resolved && !strcmp(resolved, cases[i].expected));
        assert(mulib_uri_parse(resolved, strlen(resolved), &parsed) && parsed.has_authority == cases[i].authority);
        composed = mulib_uri_compose(&parsed);
        assert(composed && !strcmp(composed, resolved));
        again = mulib_uri_resolve(&parsed, &parsed);
        assert(again && !strcmp(again, resolved));
        free(again); free(composed); free(resolved);
    }
    for (size_t i = 0; i < sizeof(paths)/sizeof(paths[0]); ++i) {
        mulib_uri_t u = {0}, parsed; char *composed; mulib_strbuf_t expected;
        u.path.data = paths[i].path; u.path.length = strlen(paths[i].path);
        composed = mulib_uri_compose(&u);
        assert(composed && !strcmp(composed, paths[i].expected));
        assert(mulib_uri_parse(composed, strlen(composed), &parsed) && !parsed.has_authority && !parsed.scheme.length);
        free(composed);
        u.scheme.data = "file"; u.scheme.length = 4;
        mulib_strbuf_init(&expected);
        assert(mulib_strbuf_append(&expected, "file:") && mulib_strbuf_append(&expected, paths[i].expected));
        composed = mulib_uri_compose(&u);
        assert(composed && !strcmp(composed, expected.data));
        assert(mulib_uri_parse(composed, strlen(composed), &parsed) && !parsed.has_authority && parsed.scheme.length == 4);
        free(composed); mulib_strbuf_free(&expected);
    }
    {
        mulib_uri_t u = {0}; char *composed;
        u.scheme.data = "file"; u.scheme.length = 4; u.has_authority = 1;
        u.path.data = "//c"; u.path.length = 3;
        composed = mulib_uri_compose(&u);
        assert(composed && !strcmp(composed, "file:////c")); free(composed);
        u.has_authority = 0; u.path.data = "//c?x"; u.path.length = 5;
        assert(!mulib_uri_compose(&u));
    }
}

static void http_bad(const char *wire) {
    mulib_http_message_t m = {0}; size_t used = 99;
    mulib_http_options_t o = {0, 0, 0, MULIB_HTTP_EOF};
    assert(mulib_http_parse(wire, strlen(wire), 0, &o, &m, &used) < 0 && used == 0);
    assert(!m.headers && !m.body && !m.method);
}
static void http_prefixes(const char *wire, size_t length, int response) {
    for (size_t i = 0; i < length; ++i) {
        mulib_http_message_t m = {0}; size_t used = 99;
        assert(mulib_http_parse(wire, i, response, NULL, &m, &used) == MULIB_HTTP_MORE);
        assert(!used && !m.body && !m.headers);
    }
}
static void test_http(void) {
    const char fixed[] = "POST /a%20b?q HTTP/1.1\r\nHost: example.org\r\nContent-Length: 3\r\n\r\na\0b";
    const char chunked[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\na\0\r\n1\r\nb\r\n0\r\n\r\n";
    static const char *const bad[] = {
        "GET / HTTP/1.1\r\n\r\n", "GET / HTTP/1.1\r\nHost: a\r\nHost: a\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 0, 0\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nContent-Length: +0\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 184467440737095516160\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: gzip, chunked\r\n\r\n0\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n1;x=y\r\na\r\n0\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nX: y\r\n\r\n",
        "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n1\r\naX\n0\r\n\r\n",
        "GET / HTTP/1.1\nHost: a\n\n", "GET / HTTP/1.1\rXHost: a\r\n\r\n",
        "GET / HTTP/1.1\r\nHost : a\r\n\r\n", "GET / HTTP/1.1\r\nHost: a\r\n x: y\r\n\r\n",
        "GET / HTTP/1.0\r\nHost: a\r\n\r\n", "GET  / HTTP/1.1\r\nHost: a\r\n\r\n",
        "GET /#x HTTP/1.1\r\nHost: a\r\n\r\n", "GET //a HTTP/1.1\r\nHost: a\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: u@a\r\n\r\n", "GET / HTTP/1.1\r\nHost: a:65536\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: a:\r\n\r\n", "GET / HTTP/1.1\r\nHost: a/b\r\n\r\n",
        "GET https://a/ HTTP/1.1\r\nHost: b\r\n\r\n", "GET ftp://a/ HTTP/1.1\r\nHost: a\r\n\r\n",
        "GET https://u@a/ HTTP/1.1\r\nHost: a\r\n\r\n", "GET * HTTP/1.1\r\nHost: a\r\n\r\n",
        "CONNECT a HTTP/1.1\r\nHost: a\r\n\r\n", "CONNECT a:443 HTTP/1.1\r\nHost: a:80\r\n\r\n"
    };
    static const char *const good[] = {
        "GET / HTTP/1.1\r\nHost: [::1]:80\r\n\r\n", "GET http://A:80/a HTTP/1.1\r\nHost: a\r\n\r\n",
        "GET https://a/a HTTP/1.1\r\nHost: A:0443\r\n\r\n", "OPTIONS * HTTP/1.1\r\nHost: a\r\n\r\n",
        "CONNECT a:443 HTTP/1.1\r\nHost: A:443\r\n\r\n", "POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 000\r\n\r\n"
    };
    mulib_http_message_t m = {0}, again = {0}; size_t used, encoded_length; unsigned char *encoded = NULL;
    mulib_http_options_t o = {0, 0, 0, MULIB_HTTP_EOF};
    http_prefixes(fixed, sizeof(fixed)-1, 0); http_prefixes(chunked, sizeof(chunked)-1, 1);
    assert(mulib_http_parse(fixed, sizeof(fixed)-1, 0, NULL, &m, &used) == MULIB_HTTP_OK);
    assert(used == sizeof(fixed)-1 && m.body_length == 3 && m.body[1] == 0);
    assert(mulib_http_serialize(&m, NULL, &encoded, &encoded_length) == MULIB_HTTP_OK);
    assert(encoded_length == sizeof(fixed)-1 && !memcmp(encoded, fixed, encoded_length));
    free(encoded); encoded = NULL;
    m.body_length = 2;
    assert(mulib_http_serialize(&m, NULL, &encoded, &encoded_length) == MULIB_HTTP_INVALID && !encoded);
    m.body_length = 3; mulib_http_message_free(&m);
    assert(mulib_http_parse(chunked, sizeof(chunked)-1, 1, NULL, &m, &used) == MULIB_HTTP_OK);
    assert(m.chunked && m.body_length == 3 && !memcmp(m.body, "a\0b", 3));
    assert(mulib_http_serialize(&m, NULL, &encoded, &encoded_length) == MULIB_HTTP_OK);
    assert(mulib_http_parse(encoded, encoded_length, 1, NULL, &again, &used) == MULIB_HTTP_OK && used == encoded_length);
    assert(again.body_length == 3 && !memcmp(again.body, m.body, 3));
    free(encoded); mulib_http_message_free(&m); mulib_http_message_free(&again);
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) http_bad(bad[i]);
    for (size_t i = 0; i < sizeof(good)/sizeof(good[0]); ++i) {
        assert(mulib_http_parse(good[i], strlen(good[i]), 0, &o, &m, &used) == MULIB_HTTP_OK);
        assert(used == strlen(good[i])); mulib_http_message_free(&m);
    }
    {
        const char pipeline[] = "GET / HTTP/1.1\r\nHost: a\r\n\r\nGET /x HTTP/1.1\r\nHost: a\r\n\r\n";
        size_t first;
        assert(mulib_http_parse(pipeline, sizeof(pipeline)-1, 0, NULL, &m, &first) == MULIB_HTTP_OK);
        assert(first < sizeof(pipeline)-1); mulib_http_message_free(&m);
        assert(mulib_http_parse(pipeline+first, sizeof(pipeline)-1-first, 0, NULL, &m, &used) == MULIB_HTTP_OK);
        assert(!strcmp(m.target, "/x") && first+used == sizeof(pipeline)-1); mulib_http_message_free(&m);
    }
    {
        const char close[] = "HTTP/1.1 200 OK\r\n\r\nbody";
        assert(mulib_http_parse(close, sizeof(close)-1, 1, NULL, &m, &used) == MULIB_HTTP_MORE);
        assert(mulib_http_parse(close, sizeof(close)-1, 1, &o, &m, &used) == MULIB_HTTP_OK);
        assert(m.close_delimited && m.body_length == 4); mulib_http_message_free(&m);
    }
    {
        const char head[] = "HTTP/1.1 200 OK\r\nContent-Length: 999\r\n\r\nnext";
        o.flags = MULIB_HTTP_HEAD_RESPONSE;
        assert(mulib_http_parse(head, sizeof(head)-1, 1, &o, &m, &used) == MULIB_HTTP_OK);
        assert(used == sizeof(head)-5 && !m.body_length);
        assert(mulib_http_serialize(&m, &o, &encoded, &encoded_length) == MULIB_HTTP_OK);
        free(encoded); mulib_http_message_free(&m);
        o.flags = MULIB_HTTP_EOF;
        {
            const char invalid[] = "HTTP/1.1 204 No\r\nContent-Length: 0\r\n\r\n";
            assert(mulib_http_parse(invalid, sizeof(invalid)-1, 1, &o, &m, &used) == MULIB_HTTP_INVALID);
        }
    }
    {
        const char tunnel[] = "HTTP/1.1 200 Connected\r\n\r\ntunnel bytes";
        const char not_modified[] = "HTTP/1.1 304 Same\r\nContent-Length: 42\r\n\r\nnext";
        const char informational[] = "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 204 Empty\r\n\r\n";
        size_t first;
        o.flags = MULIB_HTTP_CONNECT_RESPONSE;
        assert(mulib_http_parse(tunnel, sizeof(tunnel)-1, 1, &o, &m, &used) == MULIB_HTTP_OK);
        assert(!m.body_length && used < sizeof(tunnel)-1); mulib_http_message_free(&m);
        o.flags = 0;
        assert(mulib_http_parse(not_modified, sizeof(not_modified)-1, 1, &o, &m, &used) == MULIB_HTTP_OK);
        assert(m.status == 304 && !m.body_length && used < sizeof(not_modified)-1); mulib_http_message_free(&m);
        assert(mulib_http_parse(informational, sizeof(informational)-1, 1, &o, &m, &first) == MULIB_HTTP_OK);
        assert(m.status == 100); mulib_http_message_free(&m);
        assert(mulib_http_parse(informational+first, sizeof(informational)-1-first, 1, &o, &m, &used) == MULIB_HTTP_OK);
        assert(m.status == 204 && first+used == sizeof(informational)-1); mulib_http_message_free(&m);
        o.flags = MULIB_HTTP_EOF;
        assert(mulib_http_parse(fixed, sizeof(fixed)-2, 0, &o, &m, &used) == MULIB_HTTP_INVALID);
        assert(mulib_http_parse(chunked, sizeof(chunked)-2, 1, &o, &m, &used) == MULIB_HTTP_INVALID);
    }
    {
        mulib_http_header_t h = {"Host", "a\r\nContent-Length: 9"};
        m.method = "GET"; m.target = "/"; m.headers = &h; m.header_count = 1;
        encoded = NULL;
        assert(mulib_http_serialize(&m, NULL, &encoded, &encoded_length) == MULIB_HTTP_INVALID && !encoded);
        memset(&m, 0, sizeof(m));
    }
    o.max_body_bytes = 2;
    assert(mulib_http_parse(fixed, sizeof(fixed)-1, 0, &o, &m, &used) == MULIB_HTTP_LIMIT);
    assert(mulib_http_parse(chunked, sizeof(chunked)-1, 1, &o, &m, &used) == MULIB_HTTP_LIMIT);
    o.max_body_bytes = 0; o.max_headers = 1;
    assert(mulib_http_parse(fixed, sizeof(fixed)-1, 0, &o, &m, &used) == MULIB_HTTP_LIMIT);
    o.max_headers = 0; o.max_header_bytes = 10;
    assert(mulib_http_parse(fixed, sizeof(fixed)-1, 0, &o, &m, &used) == MULIB_HTTP_LIMIT);
}

static void http_serialize_rejected(const mulib_http_message_t *message) {
    unsigned char sentinel[] = "unchanged", *bytes = sentinel;
    size_t length = 123;
    assert(mulib_http_serialize(message, NULL, &bytes, &length) == MULIB_HTTP_INVALID);
    assert(bytes == sentinel && length == 123 && !strcmp((const char *)sentinel, "unchanged"));
}
static void test_http_serialize_source_validation(void) {
    static const char *const bad_targets[] = {
        "/ HTTP/1.1\r\nX-Injected: yes\r\nX-Filler: ",
        "/ HTTP/1.0\r\nX-Injected: yes\r\nX-Filler: ",
        "/ HTTP/1.1\r\nContent-Length: 0\r\nX-Filler: ",
        "/ HTTP/1.1\r\nHost: evil.example\r\nX-Filler: ",
        "/ HTTP/1.1\r\n\r\nGET /next", "/ a", "/\ta", "/\ra", "/\na", "/\x7f", "/\x80",
        "", "relative", "//example.org/path", "http://evil.example/", "ftp://example.org/",
        "http://user@example.org/", "http://example.org/#fragment", "/#fragment", "/%zz", "/%", "*"
    };
    static const char *const bad_methods[] = {"", "GET POST", "GET\t", "GET\r\nX-Injected: yes", "GET / HTTP/1.0"};
    static const struct { const char *method, *target, *host; } good[] = {
        {"GET", "/", "example.org"},
        {"POST", "/path?q=%0D%0A", " \texample.org\t "},
        {"GET", "http://EXAMPLE.org:80/a", "example.org"},
        {"GET", "https://example.org/a", "example.org:443"},
        {"CONNECT", "example.org:443", "example.org:443"},
        {"OPTIONS", "*", "example.org"}, {"GET", "/.//x", "example.org"}
    };
    mulib_http_message_t m = {0}; mulib_http_header_t host = {"Host", "example.org"};
    m.method = "GET"; m.target = "/"; m.headers = &host; m.header_count = 1;
    for (size_t i = 0; i < sizeof(bad_targets)/sizeof(bad_targets[0]); ++i) {
        m.target = (char *)bad_targets[i]; http_serialize_rejected(&m);
    }
    m.target = "/";
    for (size_t i = 0; i < sizeof(bad_methods)/sizeof(bad_methods[0]); ++i) {
        m.method = (char *)bad_methods[i]; http_serialize_rejected(&m);
    }
    m.method = "GET";
    for (unsigned c = 1; c < 256; ++c) if (c <= 32 || c >= 127) {
        char target[] = {'/', (char)c, 0};
        m.target = target; http_serialize_rejected(&m);
    }
    for (size_t i = 0; i < sizeof(good)/sizeof(good[0]); ++i) {
        mulib_http_message_t parsed = {0}; unsigned char *wire = NULL; size_t length, used;
        m.method = (char *)good[i].method; m.target = (char *)good[i].target; host.value = (char *)good[i].host;
        assert(mulib_http_serialize(&m, NULL, &wire, &length) == MULIB_HTTP_OK);
        assert(mulib_http_parse(wire, length, 0, NULL, &parsed, &used) == MULIB_HTTP_OK && used == length);
        assert(!strcmp(parsed.method, m.method) && !strcmp(parsed.target, m.target));
        assert(parsed.header_count == 2 && !mulib_http_header(&parsed, "X-Injected"));
        mulib_http_message_free(&parsed); free(wire);
    }
    memset(&m, 0, sizeof(m)); m.is_response = 1; m.status = 200;
    m.reason = "OK\r\nX-Injected: yes"; http_serialize_rejected(&m);
}

static int rpc_calls;
static int rpc_sum(const mulib_json_t *params, mulib_json_t **result, void *user) {
    double sum = 0; (void)user; ++rpc_calls;
    if (mulib_json_typeof(params) != MULIB_JSON_ARRAY) return -32602;
    for (size_t i = 0; i < mulib_json_size(params); ++i) {
        if (mulib_json_typeof(mulib_json_at(params, i)) != MULIB_JSON_NUMBER) return -32602;
        sum += mulib_json_numeric(mulib_json_at(params, i));
    }
    *result = mulib_json_number(sum); return *result ? 0 : -32603;
}
static int rpc_custom(const mulib_json_t *params, mulib_json_t **result, void *user) {
    (void)params; (void)user; ++rpc_calls;
    *result = mulib_json_string("detail"); return -32042;
}
static const mulib_rpc_method_t rpc_methods[] = { {"sum", rpc_sum, NULL}, {"fail", rpc_custom, NULL} };
static mulib_json_t *rpc_response(const char *source, size_t max_batch) {
    char *text = NULL; mulib_json_t *res;
    assert(mulib_rpc_process(rpc_methods, 2, source, strlen(source), max_batch, &text));
    if (!text) return NULL;
    res = mulib_json_parse(text, strlen(text), NULL); assert(res); free(text); return res;
}
static void rpc_error(const char *source, int code) {
    mulib_json_t *res = rpc_response(source, 0);
    assert(res && mulib_json_typeof(res) == MULIB_JSON_OBJECT);
    assert(mulib_json_numeric(mulib_json_get(mulib_json_get(res, "error"), "code")) == code);
    assert(!mulib_json_get(res, "result")); mulib_json_destroy(res);
}
static void test_rpc(void) {
    mulib_json_t *res, *id; size_t n; const char *s;
    rpc_calls = 0;
    res = rpc_response("{\"jsonrpc\":\"2.0\",\"method\":\"sum\",\"params\":[1,2,3],\"id\":1.5}", 0);
    assert(res && mulib_json_numeric(mulib_json_get(res, "result")) == 6);
    assert(mulib_json_numeric(mulib_json_get(res, "id")) == 1.5); mulib_json_destroy(res);
    res = rpc_response("{\"jsonrpc\":\"2.0\",\"method\":\"sum\",\"params\":[],\"id\":\"a\\u0000b\"}", 0);
    id = mulib_json_get(res, "id"); s = mulib_json_text(id, &n);
    assert(s && n == 3 && !memcmp(s, "a\0b", 3)); mulib_json_destroy(res);
    res = rpc_response("{\"jsonrpc\":\"2.0\",\"method\":\"sum\",\"params\":[],\"id\":null}", 0);
    assert(res && mulib_json_get(res, "id") && mulib_json_typeof(mulib_json_get(res, "id")) == MULIB_JSON_NULL);
    mulib_json_destroy(res);
    assert(!rpc_response("{\"jsonrpc\":\"2.0\",\"method\":\"sum\",\"params\":[4]}", 0));
    assert(!rpc_response("{\"jsonrpc\":\"2.0\",\"method\":\"absent\"}", 0));
    assert(!rpc_response("{\"jsonrpc\":\"2.0\",\"method\":\"fail\"}", 0));
    rpc_error("{", -32700); rpc_error("{} garbage", -32700); rpc_error("{}{}", -32700);
    rpc_error("null", -32600); rpc_error("[]", -32600);
    rpc_error("{\"jsonrpc\":\"1.0\",\"method\":\"sum\"}", -32600);
    rpc_error("{\"jsonrpc\":\"2.0\",\"method\":\"sum\",\"id\":true}", -32600);
    rpc_error("{\"jsonrpc\":\"2.0\",\"method\":\"sum\",\"params\":7}", -32600);
    rpc_error("{\"jsonrpc\":\"2.0\",\"method\":\"sum\",\"id\":7}", -32602);
    rpc_error("{\"jsonrpc\":\"2.0\",\"method\":\"absent\",\"id\":7}", -32601);
    rpc_error("{\"jsonrpc\":\"2.0\",\"method\":\"sum\\u0000x\",\"id\":7}", -32601);
    res = rpc_response("{\"jsonrpc\":\"2.0\",\"method\":\"fail\",\"id\":\"x\"}", 0);
    assert(!strcmp(mulib_json_text(mulib_json_get(mulib_json_get(res, "error"), "data"), NULL), "detail")); mulib_json_destroy(res);
    {
        const char batch[] = "[{\"jsonrpc\":\"2.0\",\"method\":\"sum\",\"params\":[2],\"id\":1},{\"jsonrpc\":\"2.0\",\"method\":\"sum\",\"params\":[9]},false,[]]";
        int before = rpc_calls;
        res = rpc_response(batch, 0); assert(res && mulib_json_size(res) == 3 && rpc_calls == before+2);
        assert(mulib_json_numeric(mulib_json_get(mulib_json_at(res, 0), "result")) == 2);
        assert(mulib_json_numeric(mulib_json_get(mulib_json_get(mulib_json_at(res, 1), "error"), "code")) == -32600);
        mulib_json_destroy(res); before = rpc_calls;
        res = rpc_response(batch, 1);
        assert(res && mulib_json_numeric(mulib_json_get(mulib_json_get(res, "error"), "code")) == -32000 && rpc_calls == before);
        mulib_json_destroy(res);
    }
    assert(!rpc_response("[{\"jsonrpc\":\"2.0\",\"method\":\"absent\"},{\"jsonrpc\":\"2.0\",\"method\":\"fail\"}]", 0));
    {
        const char request[] = "{\"jsonrpc\":\"2.0\",\"method\":\"sum\",\"params\":[],\"id\":1}";
        int before = rpc_calls;
        for (size_t i = 0; i < sizeof(request)-1; ++i) {
            char *text = NULL;
            assert(mulib_rpc_process(rpc_methods, 2, request, i, 0, &text) && text);
            res = mulib_json_parse(text, strlen(text), NULL); assert(res);
            assert(mulib_json_numeric(mulib_json_get(mulib_json_get(res, "error"), "code")) == -32700);
            mulib_json_destroy(res); free(text);
        }
        assert(rpc_calls == before);
    }
    {
        mulib_rpc_method_t bad[2] = { {"sum", rpc_sum, NULL}, {"sum", rpc_sum, NULL} }; char *out = NULL;
        assert(!mulib_rpc_process(bad, 2, "{}", 2, 0, &out));
        bad[1].name = "rpc.foo"; assert(!mulib_rpc_process(bad, 2, "{}", 2, 0, &out));
    }
}

static void dbus_complex(char endian) {
    mulib_dbus_value_t v[19], ints[2], tuple[2], dict, pair[2], dict_value;
    mulib_dbus_message_t m = {0}, d = {0}; unsigned char *wire = NULL, *again = NULL;
    size_t length, used, again_length;
    memset(v, 0, sizeof(v)); memset(ints, 0, sizeof(ints)); memset(tuple, 0, sizeof(tuple));
    memset(&dict, 0, sizeof(dict)); memset(pair, 0, sizeof(pair)); memset(&dict_value, 0, sizeof(dict_value));
    v[0].type = 'y'; v[0].u64 = 255; v[1].type = 'b'; v[1].u64 = 1;
    v[2].type = 'n'; v[2].i64 = INT16_MIN; v[3].type = 'q'; v[3].u64 = UINT16_MAX;
    v[4].type = 'i'; v[4].i64 = INT32_MIN; v[5].type = 'u'; v[5].u64 = UINT32_MAX;
    v[6].type = 'x'; v[6].i64 = INT64_MIN; v[7].type = 't'; v[7].u64 = UINT64_MAX;
    v[8].type = 'd'; v[8].real = -2.25; v[9].type = 's'; v[9].text = "h\xc3\xa9"; v[9].length = 3;
    v[10].type = 'o'; v[10].text = "/x_2"; v[10].length = 4;
    v[11].type = 'g'; v[11].text = "a{sv}"; v[11].length = 5;
    ints[0].type = ints[1].type = 'i'; ints[0].i64 = 1; ints[1].i64 = -2;
    v[12].type = 'a'; v[12].signature = "i"; v[12].children = ints; v[12].count = 2;
    tuple[0].type = 'u'; tuple[0].u64 = 7; tuple[1].type = 's'; tuple[1].text = "x"; tuple[1].length = 1;
    v[13].type = '('; v[13].children = tuple; v[13].count = 2;
    pair[0].type = 's'; pair[0].text = "key"; pair[0].length = 3;
    dict_value.type = 'u'; dict_value.u64 = 19;
    pair[1].type = 'v'; pair[1].signature = "u"; pair[1].children = &dict_value; pair[1].count = 1;
    dict.type = '{'; dict.children = pair; dict.count = 2;
    v[14].type = 'a'; v[14].signature = "{sv}"; v[14].children = &dict; v[14].count = 1;
    v[15].type = 'v'; v[15].signature = "(us)"; v[15].children = &v[13]; v[15].count = 1;
    v[16].type = 'a'; v[16].signature = "t";
    v[17].type = 'a'; v[17].signature = "(us)"; v[17].children = &v[13]; v[17].count = 1;
    v[18].type = 'a'; v[18].signature = "{sv}";
    m.endian = endian; m.type = 4; m.flags = 1; m.serial = UINT32_MAX;
    m.path = "/a"; m.interface = "x.y"; m.member = "Sig"; m.sender = ":1.23"; m.destination = "org.example.Test";
    m.body = v; m.count = 19;
    assert(mulib_dbus_encode(&m, NULL, &wire, &length) == MULIB_DBUS_OK);
    assert(mulib_dbus_decode(wire, length, NULL, &d, &used) == MULIB_DBUS_OK && used == length && d.count == 19);
    assert(!strcmp(d.signature, "ybnqiuxtdsogai(us)a{sv}vata(us)a{sv}"));
    assert(d.body[2].i64 == INT16_MIN && d.body[4].i64 == INT32_MIN && d.body[6].i64 == INT64_MIN);
    assert(d.body[7].u64 == UINT64_MAX && d.body[8].real == -2.25 && d.body[12].children[1].i64 == -2);
    assert(d.body[14].children[0].children[1].children[0].u64 == 19 && d.body[16].count == 0);
    assert(d.body[17].children[0].children[0].u64 == 7 && d.body[18].count == 0);
    assert(mulib_dbus_encode(&d, NULL, &again, &again_length) == MULIB_DBUS_OK);
    assert(again_length == length && !memcmp(again, wire, length));
    free(again); free(wire); mulib_dbus_message_free(&d);
}
static void test_dbus(void) {
    static const unsigned char golden[] = {
        0x6c,2,0,1, 4,0,0,0, 2,0,0,0, 15,0,0,0,
        5,1,'u',0, 1,0,0,0, 8,1,'g',0, 1,'u',0,0, 0x78,0x56,0x34,0x12
    };
    static const char *const good_sig[] = { "", "ybnqiuxtdsogv", "a{sv}", "a(is)", "aa{sa{sv}}", "(a(uv)s)" };
    static const char *const bad_sig[] = { "a", "()", "{sv}", "a{vv}", "a{sss}", "a{}", "(i", "i)", "h", "a{sh}", "z", "{s}" };
    mulib_dbus_value_t value = {0}; mulib_dbus_message_t m = {0}, d = {0};
    unsigned char *wire = NULL; size_t length, used;
    for (size_t i = 0; i < sizeof(good_sig)/sizeof(good_sig[0]); ++i) assert(mulib_dbus_signature_valid(good_sig[i], strlen(good_sig[i])));
    for (size_t i = 0; i < sizeof(bad_sig)/sizeof(bad_sig[0]); ++i) assert(!mulib_dbus_signature_valid(bad_sig[i], strlen(bad_sig[i])));
    value.type = 'u'; value.u64 = UINT32_C(0x12345678);
    m.endian = 'l'; m.type = 2; m.serial = 2; m.reply_serial = 1; m.body = &value; m.count = 1;
    assert(mulib_dbus_encode(&m, NULL, &wire, &length) == MULIB_DBUS_OK);
    assert(length == sizeof(golden) && !memcmp(wire, golden, length));
    for (size_t i = 0; i < length; ++i) {
        used = 99;
        assert(mulib_dbus_decode(wire, i, NULL, &d, &used) == MULIB_DBUS_MORE && !used && !d.body);
    }
    assert(mulib_dbus_decode(wire, length, NULL, &d, &used) == MULIB_DBUS_OK && d.body[0].u64 == value.u64);
    mulib_dbus_message_free(&d);
    {
        unsigned char pipeline[sizeof(golden)*2];
        memcpy(pipeline, golden, sizeof(golden)); memcpy(pipeline+sizeof(golden), golden, sizeof(golden));
        assert(mulib_dbus_decode(pipeline, sizeof(pipeline), NULL, &d, &used) == MULIB_DBUS_OK && used == sizeof(golden));
        mulib_dbus_message_free(&d);
    }
    {
        unsigned char bad[sizeof(golden)];
        static const struct { size_t pos; unsigned char byte; } changes[] = {
            {0,'x'}, {1,0}, {2,8}, {3,2}, {8,0}, {16,0}, {18,'y'}, {24,5}, {29,'h'}, {30,1}, {31,1}
        };
        for (size_t i = 0; i < sizeof(changes)/sizeof(changes[0]); ++i) {
            memcpy(bad, golden, sizeof(bad)); bad[changes[i].pos] = changes[i].byte;
            assert(mulib_dbus_decode(bad, sizeof(bad), NULL, &d, &used) == MULIB_DBUS_INVALID && !used && !d.body);
        }
        memcpy(bad, golden, sizeof(bad)); bad[4] = 3;
        assert(mulib_dbus_decode(bad, sizeof(bad)-1, NULL, &d, &used) == MULIB_DBUS_INVALID);
        memcpy(bad, golden, sizeof(bad)); bad[29] = 'b'; bad[32] = 2; bad[33] = bad[34] = bad[35] = 0;
        assert(mulib_dbus_decode(bad, sizeof(bad), NULL, &d, &used) == MULIB_DBUS_INVALID);
        memcpy(bad, golden, sizeof(bad)); bad[4] = 0; bad[24] = 42;
        assert(mulib_dbus_decode(bad, sizeof(bad), NULL, &d, &used) == MULIB_DBUS_OK && used == 32 && !d.count);
        mulib_dbus_message_free(&d);
    }
    {
        mulib_dbus_limits_t limit = {10, 0, 0}; unsigned char *none = NULL;
        assert(mulib_dbus_decode(wire, length, &limit, &d, &used) == MULIB_DBUS_LIMIT);
        assert(mulib_dbus_encode(&m, &limit, &none, &used) == MULIB_DBUS_LIMIT && !none);
        limit.max_bytes = 0; limit.max_values = 1;
        assert(mulib_dbus_decode(wire, length, &limit, &d, &used) == MULIB_DBUS_LIMIT);
        assert(mulib_dbus_encode(&m, &limit, &none, &used) == MULIB_DBUS_LIMIT);
    }
    free(wire); wire = NULL;
    {
        mulib_dbus_value_t array = {0}, items[2]; size_t body_start;
        memset(items, 0, sizeof(items)); items[0].type = items[1].type = 'u';
        array.type = 'a'; array.signature = "u"; array.children = items; array.count = 2;
        m.body = &array;
        assert(mulib_dbus_encode(&m, NULL, &wire, &length) == MULIB_DBUS_OK);
        body_start = (16+mulib_endian_load_le32(wire+12)+7)&~(size_t)7;
        wire[body_start] = 9;
        assert(mulib_dbus_decode(wire, length, NULL, &d, &used) == MULIB_DBUS_INVALID);
        wire[body_start] = 4;
        assert(mulib_dbus_decode(wire, length, NULL, &d, &used) == MULIB_DBUS_INVALID);
        free(wire); wire = NULL; m.body = &value;
    }
    m.signature = "s"; assert(mulib_dbus_encode(&m, NULL, &wire, &length) == MULIB_DBUS_INVALID); m.signature = NULL;
    value.u64 = UINT64_MAX; assert(mulib_dbus_encode(&m, NULL, &wire, &length) == MULIB_DBUS_INVALID);
    value.type = 'b'; value.u64 = 2; assert(mulib_dbus_encode(&m, NULL, &wire, &length) == MULIB_DBUS_INVALID);
    value.type = 's'; value.text = "x\0y"; value.length = 3;
    assert(mulib_dbus_encode(&m, NULL, &wire, &length) == MULIB_DBUS_INVALID);
    value.text = "\xc0\x80"; value.length = 2;
    assert(mulib_dbus_encode(&m, NULL, &wire, &length) == MULIB_DBUS_INVALID);
    value.type = 'v'; value.signature = "v"; value.children = &value; value.count = 1;
    assert(mulib_dbus_encode(&m, NULL, &wire, &length) == MULIB_DBUS_LIMIT);
    dbus_complex('l'); dbus_complex('B');
}

/* Deterministic malformed-wire mutation smoke tests, also run under sanitizers. */
static void test_mutations(void) {
    const char request[] = "POST /x HTTP/1.1\r\nHost: a\r\nContent-Length: 1\r\n\r\nx";
    const unsigned char reply[] = {
        0x6c,2,0,1, 4,0,0,0, 2,0,0,0, 15,0,0,0,
        5,1,'u',0, 1,0,0,0, 8,1,'g',0, 1,'u',0,0, 0x78,0x56,0x34,0x12
    };
    unsigned char changed[sizeof(request)]; mulib_http_options_t options = {0, 0, 0, MULIB_HTTP_EOF};
    for (size_t i = 0; i < sizeof(request)-1; ++i) for (unsigned c = 0; c < 256; ++c) {
        mulib_http_message_t m = {0}; size_t used;
        memcpy(changed, request, sizeof(request)); changed[i] = (unsigned char)c;
        if (mulib_http_parse(changed, sizeof(request)-1, 0, &options, &m, &used) == MULIB_HTTP_OK) {
            unsigned char *wire = NULL; size_t length;
            assert(used <= sizeof(request)-1);
            assert(mulib_http_serialize(&m, NULL, &wire, &length) == MULIB_HTTP_OK);
            free(wire); mulib_http_message_free(&m);
        }
    }
    for (size_t i = 0; i < sizeof(reply); ++i) for (unsigned c = 0; c < 256; ++c) {
        mulib_dbus_message_t m = {0}; size_t used;
        memcpy(changed, reply, sizeof(reply)); changed[i] = (unsigned char)c;
        if (mulib_dbus_decode(changed, sizeof(reply), NULL, &m, &used) == MULIB_DBUS_OK) {
            unsigned char *wire = NULL; size_t length;
            assert(used <= sizeof(reply));
            assert(mulib_dbus_encode(&m, NULL, &wire, &length) == MULIB_DBUS_OK);
            free(wire); mulib_dbus_message_free(&m);
        }
    }
}

int main(void) {
    test_uri(); test_uri_no_authority(); test_http(); test_http_serialize_source_validation();
    test_rpc(); test_dbus(); test_mutations();
    puts("protocol tests passed");
    return 0;
}
