#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_MINICURL
#include "mulib-minicurl.h"
#include <assert.h>
#include <string.h>
static int exchange(void *user, const mulib_uri_t *endpoint, const void *bytes,
    size_t length, mulib_webclient_receive_fn receive, void *receiver) {
    const char reply[] = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
    mulib_http_message_t request = {0}; size_t used = 0; (void)user; (void)endpoint;
    assert(mulib_http_parse(bytes, length, 0, NULL, &request, &used) == MULIB_HTTP_OK && used == length);
    assert(!strcmp(request.method, "POST") && request.body_length == 3 && request.body[1] == 0);
    assert(!strcmp(mulib_http_header(&request, "Content-Type"), "application/octet-stream"));
    mulib_http_message_free(&request); return receive(receiver, reply, sizeof(reply)-1);
}
int main(void) {
    mulib_http_message_t response = {0};
    assert(mulib_minicurl_send("http://example.test/upload", "POST", "application/octet-stream",
        "a\0b", 3, NULL, exchange, NULL, &response) == MULIB_WEBCLIENT_OK);
    assert(response.status == 201); mulib_http_message_free(&response); return 0;
}
