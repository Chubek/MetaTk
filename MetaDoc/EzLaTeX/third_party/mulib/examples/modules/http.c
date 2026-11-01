#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#include "mulib-http.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char wire[] = "POST /rpc HTTP/1.1\r\nHost: example.org\r\nTransfer-Encoding: chunked\r\n\r\n3\r\na\0b\r\n0\r\n\r\n";
    mulib_http_message_t message = {0};
    unsigned char *encoded = NULL;
    size_t used, length;
    assert(mulib_http_parse(wire, 20, 0, NULL, &message, &used) == MULIB_HTTP_MORE);
    assert(mulib_http_parse(wire, sizeof(wire)-1, 0, NULL, &message, &used) == MULIB_HTTP_OK);
    assert(used == sizeof(wire)-1 && message.body_length == 3 && message.body[1] == 0);
    assert(!strcmp(mulib_http_header(&message, "host"), "example.org"));
    assert(mulib_http_serialize(&message, NULL, &encoded, &length) == MULIB_HTTP_OK);
    assert(length == sizeof(wire)-1 && !memcmp(encoded, wire, length));
    free(encoded);
    mulib_http_message_free(&message);
    return 0;
}
