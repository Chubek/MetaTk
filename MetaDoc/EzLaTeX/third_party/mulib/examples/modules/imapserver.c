#define MULIB_IMPL_IMAPSERVER
#include "mulib-imapserver.h"
#include <assert.h>
static int send_control(void *p, const void *bytes, size_t n) {
    (void)p; (void)bytes; (void)n; return 1;
}
static int login(void *p, const char *user, const char *password) {
    (void)p; return strcmp(user, "demo") == 0 && strcmp(password, "test") == 0;
}
static int select_mailbox(void *p, const char *name, size_t *exists) {
    (void)p; *exists = 3; return strcmp(name, "INBOX") == 0;
}
int main(void) {
    char line[256]; mulib_imapserver s;
    mulib_imapserver_io io = {send_control, login, select_mailbox};
    const char *input = "a CAPABILITY\r\nb LOGIN \"demo\" \"test\"\r\nc SELECT INBOX\r\n";
    assert(mulib_imapserver_init(&s, line, sizeof(line), &io, NULL));
    for (size_t i = 0; input[i]; ++i) assert(mulib_imapserver_feed(&s, input + i, 1));
    assert(s.authorized && s.selected && s.exists == 3);
    assert(mulib_imapserver_feed(&s, "d LOGOUT\r\n", 10) && s.closed);
    return 0;
}
