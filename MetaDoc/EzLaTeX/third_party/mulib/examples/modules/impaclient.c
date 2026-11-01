/* "impaclient" is the preserved module spelling; this is an IMAP client. */
#define MULIB_IMPL_IMAPSERVER
#define MULIB_IMPL_IMPACLIENT
#include "mulib-impaclient.h"
#include <assert.h>
typedef struct queue { char bytes[1024]; size_t n; } queue;
static int enqueue(void *p, const void *data, size_t n) {
    queue *q = (queue *)p;
    if (n > sizeof(q->bytes) - q->n) return 0;
    memcpy(q->bytes + q->n, data, n); q->n += n; return 1;
}
static int login(void *p, const char *user, const char *password) {
    (void)p; return strcmp(user, "demo\"\\") == 0 && strcmp(password, "test") == 0;
}
static int select_mailbox(void *p, const char *name, size_t *exists) {
    (void)p; *exists = 3; return strcmp(name, "IN BOX") == 0;
}
static void exchange(mulib_imapserver *s, mulib_impaclient *c, queue *up, queue *down) {
    if (up->n) assert(mulib_imapserver_feed(s, up->bytes, up->n));
    up->n = 0;
    if (down->n) assert(mulib_impaclient_feed(c, down->bytes, down->n));
    down->n = 0;
}
int main(void) {
    queue up = {{0}, 0}, down = {{0}, 0}; char sl[256], cl[256];
    mulib_imapserver s; mulib_impaclient c;
    mulib_imapserver_io io = {enqueue, login, select_mailbox};
    assert(mulib_imapserver_init(&s, sl, sizeof(sl), &io, &down));
    assert(mulib_impaclient_init(&c, cl, sizeof(cl), enqueue, &up));
    exchange(&s, &c, &up, &down);
    assert(mulib_impaclient_request(&c, MULIB_IMAP_CAPABILITY, NULL, NULL)); exchange(&s, &c, &up, &down);
    assert(c.capability);
    assert(mulib_impaclient_request(&c, MULIB_IMAP_LOGIN, "demo\"\\", "test")); exchange(&s, &c, &up, &down);
    assert(c.ok && c.authorized);
    assert(mulib_impaclient_request(&c, MULIB_IMAP_SELECT, "IN BOX", NULL)); exchange(&s, &c, &up, &down);
    assert(c.ok && c.exists == 3);
    assert(mulib_impaclient_request(&c, MULIB_IMAP_LOGOUT, NULL, NULL)); exchange(&s, &c, &up, &down);
    assert(c.state == 3 && s.closed);
    return 0;
}
