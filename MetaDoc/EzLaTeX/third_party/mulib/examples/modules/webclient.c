#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#include "mulib-webclient.h"
#include <assert.h>
#include <string.h>
static int exchange(void *user, const mulib_uri_t *endpoint, const void *bytes,
    size_t length, mulib_webclient_receive_fn receive, void *receiver) {
    const char reply[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\na\0b";
    mulib_http_message_t request = {0}; size_t used = 0; (void)user;
    assert(endpoint->host.length == strlen("example.test"));
    assert(mulib_http_parse(bytes, length, 0, NULL, &request, &used) == MULIB_HTTP_OK && used == length);
    assert(!strcmp(request.target, "/bytes?q=1")); mulib_http_message_free(&request);
    return receive(receiver, reply, 12) && receive(receiver, reply + 12, sizeof(reply)-1-12);
}
int main(void) {
    mulib_http_message_t response = {0};
    assert(mulib_webclient_request("http://example.test/bytes?q=1", "GET", NULL, 0,
        NULL, 0, NULL, exchange, NULL, &response) == MULIB_WEBCLIENT_OK);
    assert(response.status == 200 && response.body_length == 3 && response.body[1] == 0);
    mulib_http_message_free(&response); return 0;
}
