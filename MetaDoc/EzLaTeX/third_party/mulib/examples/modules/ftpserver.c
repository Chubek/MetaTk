#define MULIB_IMPL_FTPSERVER
#include "mulib-ftpserver.h"
#include <assert.h>
typedef struct store { unsigned char bytes[16]; size_t length; int closed; } store;
static int send_control(void *p, const void *bytes, size_t n) {
    (void)p; (void)bytes; (void)n; return 1; /* Replace with an all-or-fail queue. */
}
static int login(void *p, const char *u, const char *pw) {
    (void)p; return strcmp(u, "demo") == 0 && strcmp(pw, "test") == 0;
}
static int command(void *p, const char *verb, const char *path, char *result, size_t cap) {
    (void)p; (void)path; (void)result; (void)cap;
    return strcmp(verb, "STOR") == 0;
}
static int write_data(void *p, const void *bytes, size_t n) {
    store *s = (store *)p;
    if (n > sizeof(s->bytes) - s->length) return 0;
    memcpy(s->bytes + s->length, bytes, n); s->length += n; return 1;
}
static void close_data(void *p, int ok) { ((store *)p)->closed = ok ? 1 : -1; }
int main(void) {
    store storage = {{0}, 0, 0}; char line[256]; mulib_ftpserver server;
    mulib_ftpserver_io io = {send_control, login, command, NULL, write_data, close_data};
    const char *commands = "USER demo\r\nPASS test\r\nSTOR example\r\n";
    const unsigned char binary[] = {0, 255, '\n'};
    assert(mulib_ftpserver_init(&server, line, sizeof(line), &io, &storage));
    for (size_t i = 0; commands[i]; ++i) assert(mulib_ftpserver_feed(&server, commands + i, 1));
    assert(server.transfer == 2);
    assert(mulib_ftpserver_write(&server, binary, sizeof(binary)));
    assert(mulib_ftpserver_finish(&server, 1));
    assert(storage.closed == 1 && storage.length == sizeof(binary));
    assert(memcmp(storage.bytes, binary, sizeof(binary)) == 0);
    return 0;
}
