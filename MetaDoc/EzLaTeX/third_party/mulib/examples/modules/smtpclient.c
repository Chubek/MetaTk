#define MULIB_IMPL_SMTPSERVER
#define MULIB_IMPL_SMTPCLIENT
#include "mulib-smtpclient.h"
#include <assert.h>
typedef struct queue { char bytes[1024]; size_t n; int delivered; } queue;
static int enqueue(void *p, const void *data, size_t n) {
    queue *q = (queue *)p;
    if (n > sizeof(q->bytes) - q->n) return 0;
    memcpy(q->bytes + q->n, data, n); q->n += n; return 1;
}
static int accept_path(void *p, int recipient, const char *path) {
    (void)p; (void)recipient; (void)path; return 1;
}
static int accept_message(void *p, const mulib_smtpserver *s, const unsigned char *data, size_t n) {
    queue *q = (queue *)p; (void)s;
    assert(n == 3 && memcmp(data, ".\r\n", 3) == 0); ++q->delivered; return 1;
}
static void exchange(mulib_smtpserver *s, mulib_smtpclient *c, queue *up, queue *down) {
    if (up->n) assert(mulib_smtpserver_feed(s, up->bytes, up->n));
    up->n = 0;
    if (down->n) assert(mulib_smtpclient_feed(c, down->bytes, down->n));
    down->n = 0;
}
int main(void) {
    queue up = {{0}, 0, 0}, down = {{0}, 0, 0};
    char sl[512], cl[512]; unsigned char message[64];
    mulib_smtpserver s; mulib_smtpclient c;
    mulib_smtpserver_io io = {enqueue, accept_path, accept_message};
    assert(mulib_smtpserver_init(&s, sl, sizeof(sl), message, sizeof(message), &io, &down));
    assert(mulib_smtpclient_init(&c, cl, sizeof(cl), 64, 100, enqueue, &up));
    exchange(&s, &c, &up, &down);
    assert(mulib_smtpclient_request(&c, MULIB_SMTP_HELO, "example")); exchange(&s, &c, &up, &down);
    assert(mulib_smtpclient_request(&c, MULIB_SMTP_MAIL, "sender@example")); exchange(&s, &c, &up, &down);
    assert(mulib_smtpclient_request(&c, MULIB_SMTP_RCPT, "r@example")); exchange(&s, &c, &up, &down);
    assert(mulib_smtpclient_request(&c, MULIB_SMTP_DATA, NULL)); exchange(&s, &c, &up, &down);
    assert(c.state == 3 && mulib_smtpclient_message(&c, ".\r\n", 3));
    exchange(&s, &c, &up, &down); assert(c.code == 250 && down.delivered == 1);
    return 0;
}
