#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_REVPROXY
#include "mulib-revproxy.h"
#include <assert.h>
#include <string.h>
static int exchange(void *user, const mulib_uri_t *endpoint, const void *bytes,
    size_t length, mulib_webclient_receive_fn receive, void *receiver) {
    const char reply[] = "HTTP/1.1 200 OK\r\nConnection: X-Secret\r\nX-Secret: remove\r\nContent-Length: 3\r\n\r\na\0b";
    mulib_http_message_t request = {0}; size_t used; (void)user; (void)endpoint;
    assert(mulib_http_parse(bytes, length, 0, NULL, &request, &used) == MULIB_HTTP_OK && used == length);
    assert(!strcmp(mulib_http_header(&request, "Host"), "internal.test"));
    assert(!mulib_http_header(&request, "X-Private")); mulib_http_message_free(&request);
    return receive(receiver, reply, sizeof(reply)-1);
}
int main(void) {
    const char wire[] = "GET /file HTTP/1.1\r\nHost: public.test\r\nConnection: X-Private\r\nX-Private: remove\r\n\r\n";
    mulib_http_message_t request = {0}, response = {0}; size_t used;
    assert(mulib_http_parse(wire, sizeof(wire)-1, 0, NULL, &request, &used) == MULIB_HTTP_OK);
    assert(mulib_revproxy_forward("http://internal.test", &request, NULL, exchange, NULL, &response) == MULIB_WEBCLIENT_OK);
    assert(!mulib_http_header(&response, "X-Secret") && response.body_length == 3 && response.body[1] == 0);
    mulib_http_message_free(&request); mulib_http_message_free(&response); return 0;
}
