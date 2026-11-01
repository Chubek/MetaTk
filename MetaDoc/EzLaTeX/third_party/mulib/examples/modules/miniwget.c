#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_MINIWGET
#include "mulib-miniwget.h"
#include <assert.h>
#include <string.h>
static int exchange(void *user, const mulib_uri_t *endpoint, const void *bytes,
    size_t length, mulib_webclient_receive_fn receive, void *receiver) {
    const char reply[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\na\0b";
    (void)user; (void)endpoint; (void)bytes; (void)length;
    return receive(receiver, reply, sizeof(reply)-1);
}
int main(void) {
    unsigned char destination[3] = {9, 9, 9}; size_t length = 99;
    assert(mulib_miniwget_download("http://example.test/file", NULL, exchange, NULL,
        destination, 2, &length) == MULIB_MINIWGET_LIMIT);
    assert(destination[0] == 9 && length == 99);
    assert(mulib_miniwget_download("http://example.test/file", NULL, exchange, NULL,
        destination, sizeof(destination), &length) == MULIB_MINIWGET_OK);
    assert(length == 3 && !memcmp(destination, "a\0b", 3)); return 0;
}
