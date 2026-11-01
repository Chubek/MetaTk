#define MULIB_IMPL_SMTPSERVER
#include "mulib-smtpserver.h"
#include <assert.h>
static int send_control(void *p, const void *bytes, size_t n) {
    (void)p; (void)bytes; (void)n; return 1;
}
static int accept_path(void *p, int recipient, const char *path) {
    (void)p; (void)recipient; return strcmp(path, "blocked") != 0;
}
static int accept_message(void *p, const mulib_smtpserver *s, const unsigned char *data, size_t n) {
    int *delivered = (int *)p;
    assert(strcmp(s->sender, "sender@example") == 0 && s->recipient_count == 1);
    assert(n == 6 && memcmp(data, ".one\r\n", n) == 0);
    ++*delivered; return 1;
}
int main(void) {
    char line[512]; unsigned char message[64]; int delivered = 0;
    mulib_smtpserver s;
    mulib_smtpserver_io io = {send_control, accept_path, accept_message};
    const char *input = "HELO example\r\nMAIL FROM:<sender@example>\r\nRCPT TO:<r@example>\r\nDATA\r\n..one\r\n.\r\n";
    assert(mulib_smtpserver_init(&s, line, sizeof(line), message, sizeof(message), &io, &delivered));
    for (size_t i = 0; input[i]; ++i) assert(mulib_smtpserver_feed(&s, input + i, 1));
    assert(delivered == 1 && !s.data && !s.mail);
    return 0;
}
