#define MULIB_IMPL_FTPSERVER
#define MULIB_IMPL_FTPCLIENT
#define MULIB_IMPL_SMTPSERVER
#define MULIB_IMPL_SMTPCLIENT
#define MULIB_IMPL_IMAPSERVER
#define MULIB_IMPL_IMPACLIENT
#include "mulib-ftpserver.h"
#include "mulib-ftpclient.h"
#include "mulib-smtpclient.h"
#include "mulib-impaclient.h"
#include <assert.h>
#include <limits.h>

typedef struct wire {
    unsigned char bytes[8192];
    size_t n;
    int fail;
} wire;
typedef struct fixture {
    wire output;
    unsigned char stored[128];
    size_t stored_n, read_pos;
    int closed, success, delivered, deny;
} fixture;
static int send_bytes(void *context, const void *data, size_t n) {
    wire *w = (wire *)context;
    if (w->fail || n > sizeof(w->bytes) - w->n) return 0;
    memcpy(w->bytes + w->n, data, n); w->n += n;
    return 1;
}
static int has(wire *w, const char *text) {
    size_t n = strlen(text);
    for (size_t i = 0; i <= w->n && n <= w->n - i; ++i)
        if (memcmp(w->bytes + i, text, n) == 0) return 1;
    return 0;
}
static int authorize(void *context, const char *user, const char *password) {
    fixture *f = (fixture *)context;
    return !f->deny && strcmp(user, "u\"\\") == 0 && strcmp(password, "p") == 0;
}
static int ftp_command(void *context, const char *verb, const char *arg, char *result, size_t capacity) {
    fixture *f = (fixture *)context;
    (void)arg;
    if (f->deny) return 0;
    if (strcmp(verb, "PWD") == 0) {
        assert(capacity >= 2); strcpy(result, "/");
    }
    if (strcmp(verb, "STOR") == 0) f->stored_n = 0;
    if (strcmp(verb, "RETR") == 0) f->read_pos = 0;
    return 1;
}
static int ftp_read(void *context, void *out, size_t capacity, size_t *written) {
    fixture *f = (fixture *)context;
    size_t n = f->stored_n - f->read_pos;
    if (n > capacity) n = capacity;
    memcpy(out, f->stored + f->read_pos, n); f->read_pos += n; *written = n;
    return !f->deny;
}
static int ftp_write(void *context, const void *data, size_t n) {
    fixture *f = (fixture *)context;
    if (f->deny || n > sizeof(f->stored) - f->stored_n) return 0;
    memcpy(f->stored + f->stored_n, data, n); f->stored_n += n;
    return 1;
}
static void ftp_close(void *context, int success) {
    fixture *f = (fixture *)context;
    ++f->closed; f->success = success;
}
static int envelope(void *context, int recipient, const char *path) {
    fixture *f = (fixture *)context;
    (void)recipient;
    return !f->deny && strcmp(path, "denied") != 0;
}
static int message(void *context, const mulib_smtpserver *s, const unsigned char *data, size_t n) {
    fixture *f = (fixture *)context;
    assert(s->recipient_count > 0 && n <= sizeof(f->stored));
    if (n) memcpy(f->stored, data, n);
    f->stored_n = n; ++f->delivered;
    return !f->deny;
}
static int select_mailbox(void *context, const char *name, size_t *exists) {
    fixture *f = (fixture *)context;
    *exists = 7;
    return !f->deny && strcmp(name, "IN BOX") == 0;
}
static void ftp_pump(mulib_ftpserver *s, mulib_ftpclient *c, wire *up, wire *down) {
    for (size_t i = 0; i < up->n; ++i) assert(mulib_ftpserver_feed(s, up->bytes + i, 1));
    up->n = 0;
    for (size_t i = 0; i < down->n; ++i) assert(mulib_ftpclient_feed(c, down->bytes + i, 1));
    down->n = 0;
}
static void smtp_pump(mulib_smtpserver *s, mulib_smtpclient *c, wire *up, wire *down) {
    for (size_t i = 0; i < up->n; ++i) assert(mulib_smtpserver_feed(s, up->bytes + i, 1));
    up->n = 0;
    for (size_t i = 0; i < down->n; ++i) assert(mulib_smtpclient_feed(c, down->bytes + i, 1));
    down->n = 0;
}
static void imap_pump(mulib_imapserver *s, mulib_impaclient *c, wire *up, wire *down) {
    for (size_t i = 0; i < up->n; ++i) assert(mulib_imapserver_feed(s, up->bytes + i, 1));
    up->n = 0;
    for (size_t i = 0; i < down->n; ++i) assert(mulib_impaclient_feed(c, down->bytes + i, 1));
    down->n = 0;
}
static void test_ftp(void) {
    fixture f = {0}; wire up = {0};
    char sl[256], cl[256];
    mulib_ftpserver s; mulib_ftpclient c;
    mulib_ftpserver_io io = {send_bytes, authorize, ftp_command, ftp_read, ftp_write, ftp_close};
    const unsigned char binary[] = {0, 255, '\r', '\n', '.', 0};
    unsigned char got[16]; size_t n;
    assert(!mulib_ftpserver_init(NULL, sl, sizeof(sl), &io, &f));
    assert(mulib_ftpserver_init(&s, sl, sizeof(sl), &io, &f));
    assert(mulib_ftpclient_init(&c, cl, sizeof(cl), send_bytes, &up));
    assert(!mulib_ftpclient_request(&c, MULIB_FTP_NOOP, NULL));
    ftp_pump(&s, &c, &up, &f.output); assert(c.state == 1);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_RETR, "file"));
    ftp_pump(&s, &c, &up, &f.output); assert(c.code == 530);
    assert(!mulib_ftpclient_request(&c, MULIB_FTP_USER, "x\r\nQUIT")); assert(up.n == 0);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_USER, "u\"\\"));
    ftp_pump(&s, &c, &up, &f.output); assert(c.code == 331);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_PASS, "p"));
    ftp_pump(&s, &c, &up, &f.output); assert(s.authorized && c.code == 230);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_PWD, NULL));
    ftp_pump(&s, &c, &up, &f.output); assert(c.code == 257);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_TYPE, "I"));
    ftp_pump(&s, &c, &up, &f.output); assert(c.code == 200);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_CWD, "/")); ftp_pump(&s, &c, &up, &f.output);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_DELE, "old")); ftp_pump(&s, &c, &up, &f.output);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_STOR, "file"));
    assert(!mulib_ftpclient_request(&c, MULIB_FTP_NOOP, NULL));
    ftp_pump(&s, &c, &up, &f.output); assert(c.state == 3);
    assert(!mulib_ftpserver_read(&s, got, sizeof(got), &n));
    assert(mulib_ftpserver_write(&s, NULL, 0));
    assert(mulib_ftpserver_write(&s, binary, sizeof(binary)));
    assert(mulib_ftpserver_finish(&s, 1));
    ftp_pump(&s, &c, &up, &f.output); assert(c.code == 226 && f.closed == 1);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_RETR, "file"));
    ftp_pump(&s, &c, &up, &f.output);
    assert(mulib_ftpserver_read(&s, got, sizeof(got), &n));
    assert(n == sizeof(binary) && memcmp(got, binary, n) == 0);
    assert(mulib_ftpserver_read(&s, got, sizeof(got), &n) && n == 0);
    assert(mulib_ftpserver_finish(&s, 1)); ftp_pump(&s, &c, &up, &f.output);
    assert(f.closed == 2 && f.success);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_QUIT, NULL));
    ftp_pump(&s, &c, &up, &f.output); assert(c.state == 4 && s.closed);
    assert(!mulib_ftpserver_feed(&s, NULL, 0));

    f.output.n = 0;
    assert(mulib_ftpserver_init(&s, sl, sizeof(sl), &io, &f));
    assert(mulib_ftpserver_feed(&s, "PASS p\r\nPORT x\r\nNOOP extra\r\n", strlen("PASS p\r\nPORT x\r\nNOOP extra\r\n")));
    assert(has(&f.output, "503 ") && has(&f.output, "502 "));
    assert(!mulib_ftpserver_feed(&s, "NOOP\n", 5) && s.failed);
    assert(mulib_ftpserver_init(&s, sl, 8, &io, &f));
    assert(!mulib_ftpserver_feed(&s, "12345678", 8));
    assert(mulib_ftpclient_init(&c, cl, sizeof(cl), send_bytes, &up));
    assert(mulib_ftpclient_feed(&c, "220-first\r\nfree text\r\n220 ready\r\n", strlen("220-first\r\nfree text\r\n220 ready\r\n")));
    assert(c.state == 1);
    assert(!mulib_ftpclient_feed(&c, "200 unsolicited\r\n", 17));
    assert(mulib_ftpclient_init(&c, cl, 8, send_bytes, &up));
    assert(mulib_ftpclient_feed(&c, "220 OK\r\n", 8));
    assert(!mulib_ftpclient_request(&c, MULIB_FTP_USER, "long") && c.state == 1);
    assert(mulib_ftpclient_request(&c, MULIB_FTP_USER, "x"));
    assert(up.n == 8 && !memcmp(up.bytes, "USER x\r\n", 8)); up.n = 0;
    assert(mulib_ftpclient_feed(&c, "331 OK\r\n", 8));
    up.fail = 1; assert(!mulib_ftpclient_request(&c, MULIB_FTP_NOOP, NULL) && c.state == -1); up.fail = 0;
    f.output.fail = 1; assert(!mulib_ftpserver_init(&s, sl, sizeof(sl), &io, &f)); f.output.fail = 0;
}
static void smtp_envelope(mulib_smtpserver *s, mulib_smtpclient *c, wire *up, wire *down) {
    assert(mulib_smtpclient_request(c, MULIB_SMTP_MAIL, "")); smtp_pump(s, c, up, down);
    assert(mulib_smtpclient_request(c, MULIB_SMTP_RCPT, "r@example")); smtp_pump(s, c, up, down);
    assert(mulib_smtpclient_request(c, MULIB_SMTP_DATA, NULL)); smtp_pump(s, c, up, down);
    assert(c->state == 3);
}
static void test_smtp(void) {
    fixture f = {0}; wire up = {0}; char sl[512], cl[512]; unsigned char storage[128];
    mulib_smtpserver s; mulib_smtpclient c;
    mulib_smtpserver_io io = {send_bytes, envelope, message};
    const char body[] = "Subject: test\r\n\r\n.\r\n..hello\r\nend\r\n";
    const unsigned char binary[] = {0, '\r', '\n'};
    assert(mulib_smtpserver_init(&s, sl, sizeof(sl), storage, sizeof(storage), &io, &f));
    assert(mulib_smtpclient_init(&c, cl, sizeof(cl), 128, 100, send_bytes, &up));
    smtp_pump(&s, &c, &up, &f.output);
    assert(!mulib_smtpclient_request(&c, MULIB_SMTP_MAIL, "x"));
    assert(mulib_smtpclient_request(&c, MULIB_SMTP_HELO, "example")); smtp_pump(&s, &c, &up, &f.output);
    assert(!mulib_smtpclient_request(&c, MULIB_SMTP_MAIL, "x>\r\nQUIT"));
    smtp_envelope(&s, &c, &up, &f.output);
    assert(!mulib_smtpclient_message(&c, binary, sizeof(binary)) && up.n == 0);
    assert(!mulib_smtpclient_message(&c, "bare\n", 5) && c.state == 3);
    assert(mulib_smtpclient_message(&c, body, sizeof(body)-1));
    assert(has(&up, "\r\n..\r\n...hello\r\n")); smtp_pump(&s, &c, &up, &f.output);
    assert(c.code == 250 && f.delivered == 1 && f.stored_n == sizeof(body)-1);
    assert(memcmp(f.stored, body, sizeof(body)-1) == 0 && !s.mail && !c.mail);
    smtp_envelope(&s, &c, &up, &f.output);
    assert(mulib_smtpclient_message(&c, NULL, 0)); smtp_pump(&s, &c, &up, &f.output);
    assert(f.delivered == 2 && f.stored_n == 0);
    assert(mulib_smtpclient_request(&c, MULIB_SMTP_RSET, NULL)); smtp_pump(&s, &c, &up, &f.output);
    assert(mulib_smtpclient_request(&c, MULIB_SMTP_NOOP, NULL)); smtp_pump(&s, &c, &up, &f.output);
    assert(mulib_smtpclient_request(&c, MULIB_SMTP_QUIT, NULL)); smtp_pump(&s, &c, &up, &f.output);
    assert(c.state == 4 && s.closed);

    f.output.n = 0;
    assert(mulib_smtpserver_init(&s, sl, sizeof(sl), storage, 3, &io, &f));
    {
        const char *commands = "DATA\r\nSTARTTLS\r\nHELO host\r\nMAIL FROM:<a> SIZE=4\r\nMAIL FROM:<>\r\nRCPT TO:<r>\r\nDATA\r\n1234\r\n.\r\nNOOP\r\n";
        assert(mulib_smtpserver_feed(&s, commands, strlen(commands)));
        assert(has(&f.output, "503 ") && has(&f.output, "502 ") && has(&f.output, "501 ") && has(&f.output, "552 "));
        assert(!s.data && !s.mail && f.delivered == 2);
    }
    {
        const char *commands = "MAIL FROM:<>\r\nRCPT TO:<r>\r\nDATA\r\n";
        assert(mulib_smtpserver_feed(&s, commands, strlen(commands)));
        assert(mulib_smtpserver_feed(&s, binary, sizeof(binary)));
        assert(mulib_smtpserver_feed(&s, ".\r\n", 3)); assert(has(&f.output, "554 invalid DATA"));
    }
    assert(!mulib_smtpserver_feed(&s, "NOOP\rX", 6) && s.failed);
    assert(mulib_smtpserver_init(&s, sl, 8, storage, sizeof(storage), &io, &f));
    assert(!mulib_smtpserver_feed(&s, "12345678", 8));
    assert(mulib_smtpclient_init(&c, cl, sizeof(cl), 4, 2, send_bytes, &up));
    assert(mulib_smtpclient_feed(&c, "220 hi\r\n", 8));
    assert(mulib_smtpclient_request(&c, MULIB_SMTP_HELO, "h")); up.n = 0;
    assert(mulib_smtpclient_feed(&c, "250-first\r\n250 final\r\n", 22));
    assert(c.greeted);
    assert(mulib_smtpclient_request(&c, MULIB_SMTP_NOOP, NULL)); up.n = 0;
    assert(!mulib_smtpclient_feed(&c, "250-first\r\n251 wrong\r\n", 22));
    assert(mulib_smtp_message_valid("ab\r\n", 4, 4, 2));
    assert(!mulib_smtp_message_valid("abc\r\n", 5, 5, 2));
    assert(!mulib_smtp_message_valid(NULL, 1, 4, 4));
}
static void test_imap(void) {
    fixture f = {0}; wire up = {0}; char sl[256], cl[256], quoted[32]; size_t n = 999;
    mulib_imapserver s; mulib_impaclient c;
    mulib_imapserver_io io = {send_bytes, authorize, select_mailbox};
    assert(mulib_imap_quote("u\"\\", quoted, sizeof(quoted), &n));
    assert(strcmp(quoted, "\"u\\\"\\\\\"") == 0 && n == 7);
    strcpy(quoted, "unchanged"); n = 999;
    assert(!mulib_imap_quote("\n", quoted, sizeof(quoted), &n));
    assert(strcmp(quoted, "unchanged") == 0 && n == 999);
    assert(!mulib_imap_quote("abc", quoted, 5, &n));
    assert(mulib_imapserver_init(&s, sl, sizeof(sl), &io, &f));
    assert(mulib_impaclient_init(&c, cl, sizeof(cl), send_bytes, &up));
    imap_pump(&s, &c, &up, &f.output);
    assert(!mulib_impaclient_request(&c, MULIB_IMAP_SELECT, "IN BOX", NULL));
    assert(mulib_impaclient_request(&c, MULIB_IMAP_CAPABILITY, NULL, NULL));
    imap_pump(&s, &c, &up, &f.output); assert(c.ok && c.capability);
    assert(!mulib_impaclient_request(&c, MULIB_IMAP_LOGIN, "x\r\nT9 LOGOUT", "p")); assert(up.n == 0);
    assert(mulib_impaclient_request(&c, MULIB_IMAP_LOGIN, "u\"\\", "bad"));
    imap_pump(&s, &c, &up, &f.output); assert(!c.ok && !s.authorized);
    assert(mulib_impaclient_request(&c, MULIB_IMAP_LOGIN, "u\"\\", "p"));
    imap_pump(&s, &c, &up, &f.output); assert(c.ok && s.authorized && c.authorized);
    assert(mulib_impaclient_request(&c, MULIB_IMAP_SELECT, "IN BOX", NULL));
    imap_pump(&s, &c, &up, &f.output); assert(c.ok && c.exists == 7 && c.selected);
    assert(mulib_impaclient_request(&c, MULIB_IMAP_SELECT, "missing", NULL));
    imap_pump(&s, &c, &up, &f.output); assert(!c.ok && !c.selected && !s.selected);
    assert(mulib_impaclient_request(&c, MULIB_IMAP_NOOP, NULL, NULL)); imap_pump(&s, &c, &up, &f.output);
    assert(mulib_impaclient_request(&c, MULIB_IMAP_LOGOUT, NULL, NULL)); imap_pump(&s, &c, &up, &f.output);
    assert(c.state == 3 && s.closed);

    f.output.n = 0;
    assert(mulib_imapserver_init(&s, sl, sizeof(sl), &io, &f));
    {
        const char *commands = "a SELECT INBOX\r\nb LOGIN {3}\r\nc LOGIN \"unterminated\r\nd FETCH 1\r\ne NOOP\r\nf LOGIN \"a\\q\" p\r\n";
        assert(mulib_imapserver_feed(&s, commands, strlen(commands)));
        assert(has(&f.output, "a NO") && has(&f.output, "b BAD") && has(&f.output, "c BAD") &&
               has(&f.output, "d BAD") && has(&f.output, "e OK") && has(&f.output, "f BAD"));
    }
    assert(!mulib_imapserver_feed(&s, "x NOOP\n", 7));
    assert(mulib_imapserver_init(&s, sl, 16, &io, &f));
    assert(!mulib_imapserver_feed(&s, "1234567890123456", 16));
    assert(mulib_impaclient_init(&c, cl, sizeof(cl), send_bytes, &up));
    assert(mulib_impaclient_feed(&c, "* OK hi\r\n", 9));
    assert(mulib_impaclient_request(&c, MULIB_IMAP_NOOP, NULL, NULL)); up.n = 0;
    assert(!mulib_impaclient_feed(&c, "WRONG OK hi\r\n", 13));
    assert(mulib_impaclient_init(&c, cl, 16, send_bytes, &up));
    assert(mulib_impaclient_feed(&c, "* OK hi\r\n", 9));
    assert(!mulib_impaclient_request(&c, MULIB_IMAP_LOGIN, "long name", "password") && c.state == 1);
    c.serial = ULONG_MAX; assert(!mulib_impaclient_request(&c, MULIB_IMAP_NOOP, NULL, NULL));
    c.serial = 0; up.fail = 1;
    assert(!mulib_impaclient_request(&c, MULIB_IMAP_NOOP, NULL, NULL) && c.state == -1);
}
static int bad_pwd(void *context, const char *verb, const char *arg, char *result, size_t capacity) {
    (void)context; (void)verb; (void)arg;
    assert(capacity > 16); strcpy(result, "/\r\n200 injected"); return 1;
}
static void test_limits_failures(void) {
    fixture f = {0}; wire up = {0}; char line[256], reply[256]; unsigned char body[8];
    mulib_ftpserver fs; mulib_ftpclient fc;
    mulib_smtpserver ss; mulib_smtpclient sc;
    mulib_imapserver is; mulib_impaclient ic;
    mulib_ftpserver_io fio = {send_bytes, authorize, ftp_command, ftp_read, ftp_write, ftp_close};
    mulib_smtpserver_io sio = {send_bytes, envelope, message};
    mulib_imapserver_io iio = {send_bytes, authorize, select_mailbox};
    const char *login = "USER u\"\\\r\nPASS p\r\nSTOR file\r\n";
    assert(mulib_ftpserver_init(&fs, line, sizeof(line), &fio, &f));
    assert(!mulib_ftpserver_feed(&fs, NULL, 1) && !fs.failed);
    assert(mulib_ftpserver_feed(&fs, NULL, 0));
    assert(mulib_ftpserver_feed(&fs, login, strlen(login)) && fs.transfer == 2);
    assert(!mulib_ftpserver_feed(&fs, "\n", 1));
    assert(f.closed == 1 && !f.success && fs.failed);
    assert(!mulib_ftpserver_feed(&fs, NULL, 0) && f.closed == 1);
    f.output.n = 0;
    assert(mulib_ftpserver_init(&fs, line, sizeof(line), &fio, &f));
    assert(mulib_ftpserver_feed(&fs, login, strlen(login)));
    f.deny = 1;
    assert(!mulib_ftpserver_write(&fs, "x", 1));
    assert(!fs.failed && !fs.transfer && f.closed == 2 && has(&f.output, "451 "));
    f.deny = 0; f.output.n = 0;
    fio.command = bad_pwd;
    assert(mulib_ftpserver_init(&fs, line, sizeof(line), &fio, &f));
    login = "USER u\"\\\r\nPASS p\r\nPWD\r\n";
    assert(mulib_ftpserver_feed(&fs, login, strlen(login)));
    assert(has(&f.output, "451 invalid storage result") && !has(&f.output, "200 injected"));
    assert(mulib_ftpclient_init(&fc, reply, sizeof(reply), send_bytes, &up));
    assert(!mulib_ftpclient_feed(&fc, NULL, 1) && fc.state == 0);
    assert(!mulib_ftpclient_feed(&fc, "220\rX", 5) && fc.state == -1);
    assert(mulib_ftpclient_init(&fc, reply, 8, send_bytes, &up));
    assert(!mulib_ftpclient_feed(&fc, "220 toolong\r\n", 13));

    f.output.n = 0;
    assert(!mulib_smtpserver_init(&ss, line, sizeof(line), NULL, 1, &sio, &f));
    assert(mulib_smtpserver_init(&ss, line, sizeof(line), NULL, 0, &sio, &f));
    assert(!mulib_smtpserver_feed(&ss, NULL, 1) && !ss.failed);
    login = "EHLO h\r\nMAIL FROM:<>\r\n";
    assert(mulib_smtpserver_feed(&ss, login, strlen(login)));
    for (int i = 0; i < 16; ++i) assert(mulib_smtpserver_feed(&ss, "RCPT TO:<r>\r\n", 13));
    assert(ss.recipient_count == 16);
    assert(mulib_smtpserver_feed(&ss, "RCPT TO:<r>\r\n", 13));
    assert(ss.recipient_count == 16 && has(&f.output, "452 "));
    assert(mulib_smtpserver_feed(&ss, "DATA\r\n.\r\n", 9));
    assert(f.delivered == 1 && f.stored_n == 0);
    assert(mulib_smtpserver_init(&ss, line, 32, body, sizeof(body), &sio, &f));
    login = "HELO h\r\nMAIL FROM:<>\r\nRCPT TO:<r>\r\nDATA\r\n";
    assert(mulib_smtpserver_feed(&ss, login, strlen(login)));
    login = "0123456789012345678901234567890123456789\r\n.\r\nNOOP\r\n";
    assert(mulib_smtpserver_feed(&ss, login, strlen(login)));
    assert(!ss.failed && !ss.data && f.delivered == 1 && has(&f.output, "552 message too large"));
    assert(mulib_smtpserver_init(&ss, line, 8, body, sizeof(body), &sio, &f));
    /* Commands can be fragmented but cannot exceed the configured line bound. */
    assert(mulib_smtpserver_feed(&ss, "HELO h\r\n", 8));
    assert(mulib_smtpserver_feed(&ss, "NOOP\r\n", 6));
    f.output.fail = 1;
    assert(!mulib_smtpserver_feed(&ss, "NOOP\r\n", 6) && ss.failed);
    assert(!mulib_smtpserver_init(&ss, line, sizeof(line), body, sizeof(body), &sio, &f));
    f.output.fail = 0;
    assert(mulib_smtpclient_init(&sc, reply, sizeof(reply), 8, 8, send_bytes, &up));
    assert(!mulib_smtpclient_feed(&sc, NULL, 1) && sc.state == 0);
    assert(mulib_smtpclient_feed(&sc, "220 hi\r\n", 8));
    up.fail = 1;
    assert(!mulib_smtpclient_request(&sc, MULIB_SMTP_NOOP, NULL) && sc.state == -1);
    up.fail = 0;
    assert(mulib_smtpclient_init(&sc, reply, 8, 8, 8, send_bytes, &up));
    assert(!mulib_smtpclient_feed(&sc, "220 toolong\r\n", 13));
    assert(mulib_smtpclient_init(&sc, reply, 8, 8, 8, send_bytes, &up));
    assert(mulib_smtpclient_feed(&sc, "220 hi\r\n", 8));
    assert(mulib_smtpclient_request(&sc, MULIB_SMTP_HELO, "h")); up.n = 0;
    assert(mulib_smtpclient_feed(&sc, "250 OK\r\n", 8));
    assert(!mulib_smtpclient_request(&sc, MULIB_SMTP_MAIL, "") && !up.n && sc.state == 1);

    f.output.n = 0;
    assert(mulib_imapserver_init(&is, line, sizeof(line), &iio, &f));
    assert(!mulib_imapserver_feed(&is, NULL, 1) && !is.failed);
    login = "a LOGIN \"x\\\r\nb \"NOOP\"\r\n+ NOOP\r\nc LOGIN a b c\r\n";
    assert(mulib_imapserver_feed(&is, login, strlen(login)));
    assert(has(&f.output, "a BAD") && has(&f.output, "b BAD") && has(&f.output, "* BAD") && has(&f.output, "c BAD"));
    f.output.fail = 1;
    assert(!mulib_imapserver_feed(&is, "d NOOP\r\n", 8) && is.failed);
    assert(!mulib_imapserver_init(&is, line, sizeof(line), &iio, &f));
    f.output.fail = 0;
    assert(mulib_impaclient_init(&ic, reply, sizeof(reply), send_bytes, &up));
    assert(!mulib_impaclient_feed(&ic, NULL, 1) && ic.state == 0);
    assert(mulib_impaclient_feed(&ic, "* OK hi\r\n", 9));
    assert(mulib_impaclient_request(&ic, MULIB_IMAP_LOGIN, "u", "p")); up.n = 0;
    assert(mulib_impaclient_feed(&ic, "T1 OK login\r\n", 13));
    assert(mulib_impaclient_request(&ic, MULIB_IMAP_SELECT, "box", NULL)); up.n = 0;
    login = "* 99999999999999999999999999999999999999999999999999999999999 EXISTS\r\n";
    assert(!mulib_impaclient_feed(&ic, login, strlen(login)) && ic.state == -1);
    assert(mulib_impaclient_init(&ic, reply, 16, send_bytes, &up));
    assert(!mulib_impaclient_feed(&ic, "* OK this greeting is too long\r\n", 32));
    assert(mulib_impaclient_init(&ic, reply, sizeof(reply), send_bytes, &up));
    assert(mulib_impaclient_feed(&ic, "* OK hi\r\n", 9));
    assert(mulib_impaclient_request(&ic, MULIB_IMAP_NOOP, NULL, NULL)); up.n = 0;
    assert(!mulib_impaclient_feed(&ic, "+ literal continuation\r\n", 24));
}
static void test_bounded_mutations(void) {
    static const char *const samples[] = {
        "USER x\r\nPASS y\r\nNOOP\r\n", "HELO h\r\nMAIL FROM:<>\r\nRCPT TO:<r>\r\nDATA\r\n..\r\n.\r\n",
        "a LOGIN \"u\\\"\\\\\" p\r\nb SELECT INBOX\r\nc NOOP\r\n"
    };
    static const unsigned char mutations[] = {0, 1, '\n', '\r', '"', '\\', '{', ' ', 127, 255};
    for (size_t protocol = 0; protocol < 3; ++protocol) {
        size_t n = strlen(samples[protocol]);
        for (size_t at = 0; at < n; ++at) for (size_t m = 0; m < sizeof(mutations); ++m) {
            fixture f = {0}; char input[128], line[32]; unsigned char body[16];
            mulib_ftpserver fs; mulib_smtpserver ss; mulib_imapserver is;
            mulib_ftpserver_io fio = {send_bytes, authorize, ftp_command, ftp_read, ftp_write, ftp_close};
            mulib_smtpserver_io sio = {send_bytes, envelope, message};
            mulib_imapserver_io iio = {send_bytes, authorize, select_mailbox};
            memcpy(input, samples[protocol], n); input[at] = (char)mutations[m];
            if (protocol == 0) {
                assert(mulib_ftpserver_init(&fs, line, sizeof(line), &fio, &f));
                for (size_t i = 0; i < n; ++i) if (!mulib_ftpserver_feed(&fs, input + i, 1)) break;
            } else if (protocol == 1) {
                assert(mulib_smtpserver_init(&ss, line, sizeof(line), body, sizeof(body), &sio, &f));
                for (size_t i = 0; i < n; ++i) if (!mulib_smtpserver_feed(&ss, input + i, 1)) break;
            } else {
                assert(mulib_imapserver_init(&is, line, sizeof(line), &iio, &f));
                for (size_t i = 0; i < n; ++i) if (!mulib_imapserver_feed(&is, input + i, 1)) break;
            }
        }
    }
}
static void test_smtp_early_reply(void) {
    char line[64]; wire up = {0}; mulib_smtpclient c;
    static const char *const partial[] = {"250 early", "\r"};
    for (size_t i = 0; i < sizeof(partial)/sizeof(partial[0]); ++i) {
        assert(mulib_smtpclient_init(&c, line, sizeof(line), 128, 100, send_bytes, &up));
        assert(mulib_smtpclient_feed(&c, "220 hi\r\n", 8));
        assert(mulib_smtpclient_request(&c, MULIB_SMTP_HELO, "h"));
        assert(mulib_smtpclient_feed(&c, "250 OK\r\n", 8));
        assert(mulib_smtpclient_request(&c, MULIB_SMTP_MAIL, ""));
        assert(mulib_smtpclient_feed(&c, "250 OK\r\n", 8));
        assert(mulib_smtpclient_request(&c, MULIB_SMTP_RCPT, "r"));
        assert(mulib_smtpclient_feed(&c, "250 OK\r\n", 8));
        assert(mulib_smtpclient_request(&c, MULIB_SMTP_DATA, NULL));
        assert(mulib_smtpclient_feed(&c, "354 go\r\n", 8)); up.n = 0;
        assert(mulib_smtpclient_feed(&c, partial[i], strlen(partial[i])));
        assert(!mulib_smtpclient_message(&c, "body\r\n", 6) && !up.n && c.state == 3);
        assert(!mulib_smtpclient_message(&c, NULL, 0) && !up.n);
    }
}
int main(void) {
    test_ftp(); test_smtp(); test_imap(); test_limits_failures(); test_bounded_mutations(); test_smtp_early_reply();
    return 0;
}
