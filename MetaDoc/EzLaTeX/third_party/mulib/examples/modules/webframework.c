#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBFRAMEWORK
#include "mulib-webframework.h"
#include <assert.h>
#include <string.h>
static mulib_http_result hello(void *user, const mulib_http_message_t *request, mulib_http_message_t *response) {
    (void)user; (void)request;
    return mulib_webframework_reply(response, 200, "text/plain", "hello\n", 6);
}
int main(void) {
    const char wire[] = "HEAD /hello?lang=en HTTP/1.1\r\nHost: example.test\r\n\r\n";
    mulib_webframework_t router; mulib_http_message_t request = {0}, response = {0}; size_t used;
    mulib_webframework_init(&router, 8);
    assert(mulib_webframework_route(&router, "GET", "/hello", hello, NULL) == MULIB_HTTP_OK);
    assert(mulib_http_parse(wire, sizeof(wire)-1, 0, NULL, &request, &used) == MULIB_HTTP_OK);
    assert(mulib_webframework_dispatch(&router, &request, &response) == MULIB_HTTP_OK);
    assert(response.status == 200 && !response.body_length);
    assert(!strcmp(mulib_http_header(&response, "Content-Length"), "6"));
    mulib_http_message_free(&request); mulib_http_message_free(&response);
    mulib_webframework_free(&router); return 0;
}
