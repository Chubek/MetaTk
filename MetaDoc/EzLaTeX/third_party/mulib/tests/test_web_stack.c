#define _POSIX_C_SOURCE 200809L
#if defined(__unix__) || defined(__APPLE__)
#define MULIB_FILESERVER_POSIX
#endif
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_MINICURL
#define MULIB_IMPL_MINIWGET
#define MULIB_IMPL_MININCAT
#define MULIB_IMPL_WEBFRAMEWORK
#define MULIB_IMPL_FILESERVER
#define MULIB_IMPL_REVPROXY
#include "mulib-revproxy.h"
#include "mulib-fileserver.h"
#include "mulib-minicurl.h"
#include "mulib-miniwget.h"
#include "mulib-minincat.h"
#include <assert.h>

typedef struct fixture {
    const void *wire;
    size_t length, calls;
    int fail;
    mulib_http_message_t sent;
} fixture;
static int transport(void *user, const mulib_uri_t *endpoint, const void *request,
    size_t length, mulib_webclient_receive_fn receive, void *receiver) {
    fixture *f = (fixture *)user; size_t used = 0;
    mulib_http_options_t o = {0, 0, 0, MULIB_HTTP_EOF};
    ++f->calls;
    assert(endpoint->host.length);
    mulib_http_message_free(&f->sent);
    assert(mulib_http_parse(request, length, 0, &o, &f->sent, &used) == MULIB_HTTP_OK && used == length);
    if (f->fail == 1) return 0;
    if (f->fail == 2) { (void)receive(receiver, NULL, 1); return 1; }
    for (size_t i = 0; i < f->length;) {
        size_t n = f->length - i; if (n > 3) n = 3;
        if (!receive(receiver, (const unsigned char *)f->wire + i, n)) return 0;
        i += n;
    }
    return 1;
}
static void set_wire(fixture *f, const void *s, size_t n) { f->wire = s; f->length = n; f->fail = 0; }
static void test_client(void) {
    const char binary[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\na\0b";
    fixture f = {0}; mulib_http_message_t response = {0};
    mulib_webclient_options_t o = {{0, 0, 0, 0}, 0};
    mulib_http_header_t header = {(char *)"X-Test", (char *)"safe"};
    set_wire(&f, binary, sizeof(binary)-1);
    assert(mulib_webclient_request("http://example.test:8080/a?q=1", "POST", &header, 1,
        "x\0y", 3, NULL, transport, &f, &response) == MULIB_WEBCLIENT_OK);
    assert(!strcmp(f.sent.target, "/a?q=1") && !strcmp(mulib_http_header(&f.sent, "Host"), "example.test:8080"));
    assert(f.sent.body_length == 3 && !memcmp(f.sent.body, "x\0y", 3));
    assert(response.body_length == 3 && !memcmp(response.body, "a\0b", 3));
    mulib_http_message_free(&f.sent); /* response is not borrowed from transport. */
    assert(response.body[2] == 'b'); mulib_http_message_free(&response);
    response.status = 777;
    assert(mulib_minicurl_get("https://example.test/", NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
    assert(mulib_minicurl_get("http://u@example.test/", NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
    assert(mulib_minicurl_get("http://example.test:65536/", NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
    assert(mulib_minicurl_get("http://example.test:0/", NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
    assert(mulib_minicurl_get("http://example.test/#x", NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
    assert(mulib_minicurl_get(NULL, NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
    assert(response.status == 777 && f.calls == 1);
    header.value = (char *)"safe\r\nInjected: yes";
    assert(mulib_webclient_request("http://example.test/", "GET", &header, 1, NULL, 0, NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
    header.name = (char *)"Host"; header.value = (char *)"evil.test";
    assert(mulib_webclient_request("http://example.test/", "GET", &header, 1, NULL, 0, NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
    assert(mulib_minicurl_send("http://example.test/", "POST", "x\r\ny", "", 0, NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
    o.max_response_bytes = 5;
    assert(mulib_minicurl_get("http://example.test/", &o, transport, &f, &response) == MULIB_WEBCLIENT_LIMIT);
    o.max_response_bytes = 0; o.http.max_body_bytes = 2;
    assert(mulib_minicurl_get("http://example.test/", &o, transport, &f, &response) == MULIB_WEBCLIENT_LIMIT);
    o.http.max_body_bytes = 0; o.http.flags = MULIB_HTTP_EOF;
    assert(mulib_minicurl_get("http://example.test/", &o, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
    f.fail = 1;
    assert(mulib_minicurl_get("http://example.test/", NULL, transport, &f, &response) == MULIB_WEBCLIENT_TRANSPORT);
    f.fail = 2;
    assert(mulib_minicurl_get("http://example.test/", NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
    {
        const char *bad[] = {"", "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nx",
            "HTTP/1.1 200 OK\r\nBad\r\n\r\n", "HTTP/1.1 101 Switching Protocols\r\n\r\n",
            "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\nextra"};
        for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
            set_wire(&f, bad[i], strlen(bad[i]));
            assert(mulib_minicurl_get("http://example.test/", NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
            assert(response.status == 777);
        }
    }
    {
        const char info[] = "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
        set_wire(&f, info, sizeof(info)-1);
        assert(mulib_minicurl_get("HTTP://example.test", NULL, transport, &f, &response) == MULIB_WEBCLIENT_OK);
        assert(!strcmp(f.sent.target, "/") && response.status == 200); mulib_http_message_free(&response);
    }
    {
        mulib_strbuf_t many; mulib_strbuf_init(&many);
        for (size_t i = 0; i < 9; ++i) assert(mulib_strbuf_append(&many, "HTTP/1.1 100 Continue\r\n\r\n"));
        assert(mulib_strbuf_append(&many, "HTTP/1.1 204 No Content\r\n\r\n"));
        set_wire(&f, many.data, many.length);
        assert(mulib_minicurl_get("http://example.test/", NULL, transport, &f, &response) == MULIB_WEBCLIENT_INVALID);
        mulib_strbuf_free(&many);
    }
    {
        const char head[] = "HTTP/1.1 200 OK\r\nContent-Length: 123\r\n\r\n";
        set_wire(&f, head, sizeof(head)-1);
        assert(mulib_minicurl_send("http://example.test/", "HEAD", NULL, NULL, 0, NULL, transport, &f, &response) == MULIB_WEBCLIENT_OK);
        assert(!response.body_length && !strcmp(mulib_http_header(&response, "Content-Length"), "123"));
        mulib_http_message_free(&response);
    }
    {
        const char closed[] = "HTTP/1.1 200 OK\r\n\r\nx\0y";
        set_wire(&f, closed, sizeof(closed)-1);
        assert(mulib_minicurl_get("http://[::1]/", NULL, transport, &f, &response) == MULIB_WEBCLIENT_OK);
        assert(response.close_delimited && response.body_length == 3); mulib_http_message_free(&response);
    }
    mulib_http_message_free(&f.sent);
}
static void test_download(void) {
    const char wire[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\na\0b";
    const char empty[] = "HTTP/1.1 204 No Content\r\n\r\n";
    const char missing[] = "HTTP/1.1 404 No\r\nContent-Length: 0\r\n\r\n";
    fixture f = {0}; unsigned char bytes[4] = {9, 9, 9, 9}; size_t n = 77;
    set_wire(&f, wire, sizeof(wire)-1);
    assert(mulib_miniwget_download("http://x/", NULL, transport, &f, bytes, 2, &n) == MULIB_MINIWGET_LIMIT);
    assert(n == 77 && bytes[0] == 9);
    assert(mulib_miniwget_download("http://x/", NULL, transport, &f, bytes, 3, &n) == MULIB_MINIWGET_OK);
    assert(n == 3 && !memcmp(bytes, "a\0b", 3) && bytes[3] == 9);
    set_wire(&f, missing, sizeof(missing)-1);
    assert(mulib_miniwget_download("http://x/", NULL, transport, &f, bytes, 4, &n) == MULIB_MINIWGET_STATUS && n == 3);
    set_wire(&f, empty, sizeof(empty)-1);
    assert(mulib_miniwget_download("http://x/", NULL, transport, &f, NULL, 0, &n) == MULIB_MINIWGET_OK && !n);
    assert(mulib_miniwget_download("http://x/", NULL, transport, &f, NULL, 1, &n) == MULIB_MINIWGET_INVALID);
    mulib_http_message_free(&f.sent);
}
typedef struct stream { unsigned char bytes[16]; size_t length, position; int bad; } stream;
static int stream_read(void *u, void *b, size_t cap, size_t *n) {
    stream *s = (stream *)u;
    if (s->bad) { *n = cap + 1; return 1; }
    *n = s->length - s->position; if (*n > cap) *n = cap;
    memcpy(b, s->bytes + s->position, *n); s->position += *n; return 1;
}
static int stream_write(void *u, const void *b, size_t n, size_t *written) {
    stream *s = (stream *)u;
    if (s->bad) { *written = 0; return 1; }
    assert(n && s->length < sizeof(s->bytes)); s->bytes[s->length++] = *(const unsigned char *)b;
    *written = 1; return 1;
}
static void test_relay(void) {
    stream in = {{'a', 0, 'b'}, 3, 0, 0}, out = {{0}, 0, 0, 0};
    unsigned char scratch[2]; size_t n;
    assert(mulib_minincat_relay(stream_read, &in, stream_write, &out, scratch, 2, 10, &n) == MULIB_MININCAT_OK);
    assert(n == 3 && !memcmp(out.bytes, in.bytes, 3));
    in.position = 0; out.length = 0;
    assert(mulib_minincat_relay(stream_read, &in, stream_write, &out, scratch, 2, 2, &n) == MULIB_MININCAT_LIMIT && n == 2 && in.position == 2);
    assert(mulib_minincat_relay(stream_read, &in, stream_write, &out, scratch, 2, 0, &n) == MULIB_MININCAT_LIMIT && !n);
    in.bad = 1;
    assert(mulib_minincat_relay(stream_read, &in, stream_write, &out, scratch, 2, 5, &n) == MULIB_MININCAT_IO && !n);
    in.bad = 0; out.bad = 1;
    assert(mulib_minincat_relay(stream_read, &in, stream_write, &out, scratch, 2, 5, &n) == MULIB_MININCAT_IO && !n);
    assert(mulib_minincat_relay(NULL, &in, stream_write, &out, scratch, 2, 5, &n) == MULIB_MININCAT_INVALID);
}
static mulib_http_message_t request(const char *method, const char *target) {
    static mulib_http_header_t host = {(char *)"Host", (char *)"local.test"};
    mulib_http_message_t m = {0}; m.method = (char *)method; m.target = (char *)target;
    m.headers = &host; m.header_count = 1; return m;
}
static mulib_http_result echo(void *user, const mulib_http_message_t *req, mulib_http_message_t *out) {
    (void)user; return mulib_webframework_reply(out, 200, "application/octet-stream", req->body, req->body_length);
}
static mulib_http_result broken(void *user, const mulib_http_message_t *req, mulib_http_message_t *out) {
    (void)user; (void)req; (void)out; return MULIB_HTTP_MORE;
}
static void test_routes(void) {
    mulib_webframework_t router; mulib_http_message_t req = request("POST", "/echo?q=1"), out = {0};
    char method[] = "POST", path[] = "/echo";
    mulib_webframework_init(&router, 2);
    assert(mulib_webframework_route(&router, method, path, echo, NULL) == MULIB_HTTP_OK);
    method[0] = 'X'; path[1] = 'X';
    assert(mulib_webframework_route(&router, "POST", "/echo", echo, NULL) == MULIB_HTTP_INVALID);
    assert(mulib_webframework_route(&router, "G\rET", "/", echo, NULL) == MULIB_HTTP_INVALID);
    assert(mulib_webframework_route(&router, "GET", "/?x", echo, NULL) == MULIB_HTTP_INVALID);
    assert(mulib_webframework_route(&router, "GET", "/echo", echo, NULL) == MULIB_HTTP_OK);
    assert(mulib_webframework_route(&router, "GET", "/other", echo, NULL) == MULIB_HTTP_LIMIT);
    req.body = (unsigned char *)"x\0z"; req.body_length = 3;
    assert(mulib_webframework_dispatch(&router, &req, &out) == MULIB_HTTP_OK);
    assert(out.status == 200 && out.body_length == 3 && !memcmp(out.body, req.body, 3));
    mulib_http_message_free(&out); req.method = (char *)"HEAD";
    assert(mulib_webframework_dispatch(&router, &req, &out) == MULIB_HTTP_OK);
    assert(!out.body_length && !strcmp(mulib_http_header(&out, "Content-Length"), "3")); mulib_http_message_free(&out);
    req.method = (char *)"DELETE";
    assert(mulib_webframework_dispatch(&router, &req, &out) == MULIB_HTTP_OK && out.status == 404); mulib_http_message_free(&out);
    req.target = (char *)"/bad\r\nInjected: yes"; out.status = 777;
    assert(mulib_webframework_dispatch(&router, &req, &out) == MULIB_HTTP_INVALID && out.status == 777);
    mulib_webframework_free(&router); mulib_webframework_init(&router, 0);
    assert(mulib_webframework_route(&router, "GET", "/", broken, NULL) == MULIB_HTTP_OK);
    req = request("GET", "/");
    assert(mulib_webframework_dispatch(&router, &req, &out) == MULIB_HTTP_INVALID && out.status == 777);
    assert(mulib_webframework_reply(&out, 200, "bad\r\nHeader: yes", "", 0) == MULIB_HTTP_INVALID);
    mulib_webframework_free(&router);
}
static mulib_fileserver_result memory_load(void *user, const char *path, size_t max,
    int head, unsigned char **data, size_t *length) {
    size_t *calls = (size_t *)user; ++*calls;
    if (!strcmp(path, "missing")) return MULIB_FILESERVER_NOT_FOUND;
    if (!strcmp(path, "nomem")) return MULIB_FILESERVER_NOMEM;
    if (!strcmp(path, "forbidden")) return MULIB_FILESERVER_FORBIDDEN;
    if (!strcmp(path, "io")) return MULIB_FILESERVER_IO;
    if (max < 3) return MULIB_FILESERVER_LIMIT;
    assert(!strcmp(path, "blob") || !strcmp(path, "index.html")); *length = 3;
    if (!head) { *data = (unsigned char *)malloc(3); assert(*data); memcpy(*data, "a\0b", 3); }
    return MULIB_FILESERVER_FOUND;
}
static void test_files(void) {
    char *path = NULL; mulib_http_message_t req = request("GET", "/blob"), out = {0}; size_t calls = 0;
    mulib_fileserver_options_t limits = {2, NULL, NULL};
    const char *bad[] = {"/../secret", "/a/../secret", "/%2e%2e/secret", "/.hidden", "/a//b", "/a/%2fb",
        "/a%5cb", "/%00", "/%252e%252e/secret", "/bad%", "/a#fragment", "//host/a", "/a:b", "/a/./b"};
    assert(mulib_fileserver_path("/a/%62.txt?q=ignored", NULL, &path) == MULIB_HTTP_OK);
    assert(!strcmp(path, "a/b.txt")); free(path); path = NULL;
    assert(mulib_fileserver_path("/docs/", "home.htm", &path) == MULIB_HTTP_OK);
    assert(!strcmp(path, "docs/home.htm")); free(path); path = NULL;
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i)
        assert(mulib_fileserver_path(bad[i], NULL, &path) == MULIB_HTTP_INVALID && !path);
    assert(mulib_fileserver_path("/", "../x", &path) == MULIB_HTTP_INVALID);
    assert(mulib_fileserver_serve(&req, NULL, memory_load, &calls, &out) == MULIB_HTTP_OK);
    assert(out.body_length == 3 && !memcmp(out.body, "a\0b", 3)); mulib_http_message_free(&out);
    req.method = (char *)"HEAD";
    assert(mulib_fileserver_serve(&req, NULL, memory_load, &calls, &out) == MULIB_HTTP_OK);
    assert(!out.body_length && !strcmp(mulib_http_header(&out, "Content-Length"), "3")); mulib_http_message_free(&out);
    assert(mulib_fileserver_serve(&req, &limits, memory_load, &calls, &out) == MULIB_HTTP_OK && out.status == 413); mulib_http_message_free(&out);
    req = request("GET", "/../secret"); calls = 0;
    assert(mulib_fileserver_serve(&req, NULL, memory_load, &calls, &out) == MULIB_HTTP_OK && out.status == 403 && !calls); mulib_http_message_free(&out);
    req = request("POST", "/blob");
    assert(mulib_fileserver_serve(&req, NULL, memory_load, &calls, &out) == MULIB_HTTP_OK && out.status == 405 && !calls);
    assert(!strcmp(mulib_http_header(&out, "Allow"), "GET, HEAD")); mulib_http_message_free(&out);
    {
        const char *names[] = {"/missing", "/forbidden", "/io"}; unsigned status[] = {404, 403, 500};
        for (size_t i = 0; i < 3; ++i) {
            req = request("GET", names[i]);
            assert(mulib_fileserver_serve(&req, NULL, memory_load, &calls, &out) == MULIB_HTTP_OK && out.status == status[i]);
            mulib_http_message_free(&out);
        }
    }
    req = request("GET", "/nomem"); out.status = 777;
    assert(mulib_fileserver_serve(&req, NULL, memory_load, &calls, &out) == MULIB_HTTP_NOMEM && out.status == 777);
}
#if defined(MULIB_FILESERVER_POSIX)
static void test_posix_files(void) {
    char tmp[256], name[512];
    int root; FILE *file; mulib_http_message_t req, out = {0};
    const char *base = getenv("TMPDIR");
    if (!base || !*base) base = access("/tmp/opencode", W_OK) == 0 ? "/tmp/opencode" : "/tmp";
    assert(snprintf(tmp, sizeof(tmp), "%s/mulib-web-XXXXXX", base) > 0);
    assert(mkdtemp(tmp)); root = open(tmp, O_RDONLY | O_DIRECTORY); assert(root >= 0);
    assert(snprintf(name, sizeof(name), "%s/blob", tmp) > 0); file = fopen(name, "wb"); assert(file);
    assert(fwrite("a\0b", 1, 3, file) == 3 && !fclose(file));
    req = request("GET", "/blob");
    assert(mulib_fileserver_serve(&req, NULL, mulib_fileserver_posix_load, &root, &out) == MULIB_HTTP_OK);
    assert(out.body_length == 3 && out.body[1] == 0); mulib_http_message_free(&out);
    req.method = (char *)"HEAD";
    assert(mulib_fileserver_serve(&req, NULL, mulib_fileserver_posix_load, &root, &out) == MULIB_HTTP_OK);
    assert(!out.body_length && !strcmp(mulib_http_header(&out, "Content-Length"), "3")); mulib_http_message_free(&out);
    {
        mulib_fileserver_options_t limits = {2, NULL, NULL};
        assert(mulib_fileserver_serve(&req, &limits, mulib_fileserver_posix_load, &root, &out) == MULIB_HTTP_OK && out.status == 413);
        mulib_http_message_free(&out);
    }
    assert(symlinkat("blob", root, "link") == 0); req = request("GET", "/link");
    assert(mulib_fileserver_serve(&req, NULL, mulib_fileserver_posix_load, &root, &out) == MULIB_HTTP_OK && out.status == 403); mulib_http_message_free(&out);
    assert(mkdirat(root, "dir", 0700) == 0 && symlinkat("dir", root, "linkdir") == 0);
    req = request("GET", "/linkdir/file");
    assert(mulib_fileserver_serve(&req, NULL, mulib_fileserver_posix_load, &root, &out) == MULIB_HTTP_OK && out.status == 403); mulib_http_message_free(&out);
    assert(mkfifoat(root, "pipe", 0600) == 0); req = request("GET", "/pipe");
    assert(mulib_fileserver_serve(&req, NULL, mulib_fileserver_posix_load, &root, &out) == MULIB_HTTP_OK && out.status == 403); mulib_http_message_free(&out);
    assert(unlinkat(root, "pipe", 0) == 0 && unlinkat(root, "linkdir", 0) == 0 && unlinkat(root, "link", 0) == 0);
    assert(unlinkat(root, "dir", AT_REMOVEDIR) == 0 && unlinkat(root, "blob", 0) == 0);
    assert(close(root) == 0 && rmdir(tmp) == 0);
}
#endif
static void test_proxy(void) {
    const char raw[] = "POST /blob?q=1 HTTP/1.1\r\nHost: public.test\r\nConnection: X-Private, Keep-Alive\r\nX-Private: secret\r\nKeep-Alive: timeout=3\r\nTE: trailers\r\nTransfer-Encoding: chunked\r\nContent-Type: application/octet-stream\r\n\r\n3\r\na\0b\r\n0\r\n\r\n";
    const char wire[] = "HTTP/1.1 200 OK\r\nConnection: X-Secret\r\nX-Secret: hidden\r\nProxy-Authenticate: hidden\r\nTransfer-Encoding: chunked\r\nX-End: yes\r\n\r\n3\r\nx\0y\r\n0\r\n\r\n";
    mulib_http_message_t req = {0}, out = {0}; size_t used = 0; fixture f = {0};
    assert(mulib_http_parse(raw, sizeof(raw)-1, 0, NULL, &req, &used) == MULIB_HTTP_OK);
    set_wire(&f, wire, sizeof(wire)-1);
    assert(mulib_revproxy_forward("http://internal.test:8080/", &req, NULL, transport, &f, &out) == MULIB_WEBCLIENT_OK);
    assert(!strcmp(f.sent.target, "/blob?q=1") && !strcmp(mulib_http_header(&f.sent, "Host"), "internal.test:8080"));
    assert(!mulib_http_header(&f.sent, "X-Private") && !mulib_http_header(&f.sent, "TE") && !f.sent.chunked);
    assert(f.sent.body_length == 3 && !memcmp(f.sent.body, "a\0b", 3));
    assert(!mulib_http_header(&out, "X-Secret") && !mulib_http_header(&out, "Connection") && !mulib_http_header(&out, "Proxy-Authenticate"));
    assert(!out.chunked && !strcmp(mulib_http_header(&out, "Content-Length"), "3") && out.body_length == 3 && out.body[1] == 0);
    assert(!strcmp(mulib_http_header(&out, "X-End"), "yes")); mulib_http_message_free(&out);
    out.status = 777;
    assert(mulib_revproxy_forward("http://internal.test/prefix", &req, NULL, transport, &f, &out) == MULIB_WEBCLIENT_INVALID && out.status == 777);
    {
        const char malformed[] = "HTTP/1.1 200 OK\r\nConnection: X-End,\r\nContent-Length: 0\r\n\r\n";
        set_wire(&f, malformed, sizeof(malformed)-1);
        assert(mulib_revproxy_forward("http://internal.test", &req, NULL, transport, &f, &out) == MULIB_WEBCLIENT_INVALID && out.status == 777);
    }
    free(req.headers[1].value); req.headers[1].value = (char *)malloc(5); assert(req.headers[1].value); memcpy(req.headers[1].value, "bad,", 5);
    assert(mulib_revproxy_forward("http://internal.test", &req, NULL, transport, &f, &out) == MULIB_WEBCLIENT_INVALID);
    mulib_http_message_free(&req); mulib_http_message_free(&f.sent);
    {
        const char upgrade[] = "GET / HTTP/1.1\r\nHost: local.test\r\nUpgrade: websocket\r\n\r\n";
        assert(mulib_http_parse(upgrade, sizeof(upgrade)-1, 0, NULL, &req, &used) == MULIB_HTTP_OK);
        assert(mulib_revproxy_forward("http://internal.test", &req, NULL, transport, &f, &out) == MULIB_WEBCLIENT_INVALID);
        mulib_http_message_free(&req);
    }
}
typedef struct application { mulib_webframework_t router; size_t loads; } application;
static mulib_http_result file_handler(void *user, const mulib_http_message_t *req, mulib_http_message_t *out) {
    application *app = (application *)user;
    return mulib_fileserver_serve(req, NULL, memory_load, &app->loads, out);
}
static int application_transport(void *user, const mulib_uri_t *endpoint, const void *bytes,
    size_t length, mulib_webclient_receive_fn receive, void *receiver) {
    application *app = (application *)user; mulib_http_message_t req = {0}, out = {0};
    unsigned char *wire = NULL; size_t n = 0, used = 0; int ok;
    mulib_http_options_t options = {0, 0, 0, MULIB_HTTP_EOF}; (void)endpoint;
    assert(mulib_http_parse(bytes, length, 0, &options, &req, &used) == MULIB_HTTP_OK && used == length);
    assert(mulib_webframework_dispatch(&app->router, &req, &out) == MULIB_HTTP_OK);
    options.flags = !strcmp(req.method, "HEAD") ? MULIB_HTTP_HEAD_RESPONSE : 0;
    assert(mulib_http_serialize(&out, &options, &wire, &n) == MULIB_HTTP_OK);
    ok = receive(receiver, wire, n); free(wire); mulib_http_message_free(&req); mulib_http_message_free(&out); return ok;
}
static void test_composed(void) {
    application app; unsigned char data[3]; size_t n = 0;
    mulib_http_message_t req = request("GET", "/blob"), out = {0};
    mulib_webframework_init(&app.router, 0); app.loads = 0;
    assert(mulib_webframework_route(&app.router, "GET", "/blob", file_handler, &app) == MULIB_HTTP_OK);
    assert(mulib_miniwget_download("http://virtual.test/blob", NULL, application_transport, &app, data, 3, &n) == MULIB_MINIWGET_OK);
    assert(n == 3 && !memcmp(data, "a\0b", 3));
    assert(mulib_revproxy_forward("http://virtual.test", &req, NULL, application_transport, &app, &out) == MULIB_WEBCLIENT_OK);
    assert(out.status == 200 && out.body_length == 3); mulib_http_message_free(&out);
    req.method = (char *)"HEAD";
    assert(mulib_revproxy_forward("http://virtual.test", &req, NULL, application_transport, &app, &out) == MULIB_WEBCLIENT_OK);
    assert(!out.body_length && !strcmp(mulib_http_header(&out, "Content-Length"), "3")); mulib_http_message_free(&out);
    assert(app.loads == 3); mulib_webframework_free(&app.router);
}
int main(void) {
    test_client(); test_download(); test_relay(); test_routes(); test_files(); test_proxy(); test_composed();
#if defined(MULIB_FILESERVER_POSIX)
    test_posix_files();
#endif
    return 0;
}
