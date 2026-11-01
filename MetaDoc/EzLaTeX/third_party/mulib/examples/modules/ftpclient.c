#define MULIB_IMPL_FTPSERVER
#define MULIB_IMPL_FTPCLIENT
#include "mulib-ftpserver.h"
#include "mulib-ftpclient.h"
#include <assert.h>
typedef struct queue { char data[512]; size_t n; } queue;
static int enqueue(void *p, const void *bytes, size_t n) {
    queue *q = (queue *)p;
    if (n > sizeof(q->data) - q->n) return 0;
    memcpy(q->data + q->n, bytes, n); q->n += n; return 1;
}
static void exchange(mulib_ftpserver *s, mulib_ftpclient *c, queue *up, queue *down) {
    if (up->n) assert(mulib_ftpserver_feed(s, up->data, up->n));
    up->n = 0;
    if (down->n) assert(mulib_ftpclient_feed(c, down->data, down->n));
    down->n = 0;
}
int main(void) {
    queue up = {{0}, 0}, down = {{0}, 0}; char sl[128], cl[128];
    mulib_ftpserver s; mulib_ftpclient c;
    mulib_ftpserver_io io = {enqueue, NULL, NULL, NULL, NULL, NULL};
    assert(mulib_ftpserver_init(&s, sl, sizeof(sl), &io, &down));
    assert(mulib_ftpclient_init(&c, cl, sizeof(cl), enqueue, &up));
    exchange(&s, &c, &up, &down);
    assert(c.state == 1 && c.code == 220);
    assert(!mulib_ftpclient_request(&c, MULIB_FTP_USER, "bad\r\nQUIT"));
    assert(mulib_ftpclient_request(&c, MULIB_FTP_NOOP, NULL));
    exchange(&s, &c, &up, &down); assert(c.code == 200);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_QUIT, NULL));
    exchange(&s, &c, &up, &down); assert(c.state == 4 && s.closed);
    return 0;
}
