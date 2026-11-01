#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBFRAMEWORK
#define MULIB_IMPL_FILESERVER
#include "mulib-fileserver.h"
#include <assert.h>
#include <string.h>
/* An immutable virtual filesystem has no symlinks or filesystem races. */
static mulib_fileserver_result load(void *user, const char *path, size_t maximum,
    int metadata_only, unsigned char **bytes, size_t *length) {
    (void)user;
    if (strcmp(path, "index.html")) return MULIB_FILESERVER_NOT_FOUND;
    if (maximum < 3) return MULIB_FILESERVER_LIMIT;
    *length = 3;
    if (!metadata_only) {
        *bytes = (unsigned char *)malloc(3); if (!*bytes) return MULIB_FILESERVER_NOMEM;
        memcpy(*bytes, "a\0b", 3);
    }
    return MULIB_FILESERVER_FOUND;
}
int main(void) {
    const char wire[] = "GET / HTTP/1.1\r\nHost: example.test\r\n\r\n";
    mulib_http_message_t request = {0}, response = {0}; size_t used;
    assert(mulib_http_parse(wire, sizeof(wire)-1, 0, NULL, &request, &used) == MULIB_HTTP_OK);
    assert(mulib_fileserver_serve(&request, NULL, load, NULL, &response) == MULIB_HTTP_OK);
    assert(response.status == 200 && response.body_length == 3 && response.body[1] == 0);
    mulib_http_message_free(&request); mulib_http_message_free(&response); return 0;
}
