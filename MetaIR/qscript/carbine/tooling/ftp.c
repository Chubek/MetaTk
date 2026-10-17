/* Carbine tooling: fetching files over FTP (RFC 959, passive mode, binary).
 *
 * Anonymous login is used when the URL has no user. The control connection
 * is plain text; FTPS is not supported. */
#define _GNU_SOURCE

#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Reads one (possibly multi-line) reply. Returns its 3-digit code or -1. */
static int read_reply(FILE *ctl, cb_buf *text) {
    char line[512];
    int code = -1;
    for (;;) {
        size_t n;
        if (!fgets(line, sizeof line, ctl)) return -1;
        n = strlen(line);
        if (text) cb_buf_add(text, line, n);
        if (n >= 4 && line[3] == ' ') {
            code = atoi(line);
            return code;
        }
        if (n >= 3 && line[0] >= '0' && line[0] <= '9' && line[3] == '-') {
            code = -2;   /* multi-line: keep reading until "NNN " */
            continue;
        }
        if (code == -2 && n >= 4) continue;
    }
}

static int command(FILE *ctl, cb_buf *reply, const char *fmt, const char *arg) {
    char buf[1400];
    if (arg) snprintf(buf, sizeof buf, fmt, arg);
    else snprintf(buf, sizeof buf, "%s", fmt);
    if (fputs(buf, ctl) == EOF || fputs("\r\n", ctl) == EOF || fflush(ctl) == EOF) return -1;
    return read_reply(ctl, reply);
}

/* Parses "(h1,h2,h3,h4,p1,p2)" from a 227 reply into a host and port. */
static int parse_pasv(const char *text, char *host, size_t hcap, char *port, size_t pcap) {
    const char *p = strchr(text, '(');
    int v[6], i;
    if (!p) return -1;
    for (i = 0; i < 6; ++i) {
        char *end;
        v[i] = (int)strtol(p + 1, &end, 10);
        if (end == p + 1 || v[i] < 0 || v[i] > 255) return -1;
        if (i < 5 && *end != ',') return -1;
        if (i == 5 && *end != ')') return -1;
        p = end;
    }
    snprintf(host, hcap, "%d.%d.%d.%d", v[0], v[1], v[2], v[3]);
    snprintf(port, pcap, "%d", v[4] * 256 + v[5]);
    return 0;
}

cb_status cb_ftp_get(const char *url, const char *dest) {
    cb_url u;
    char port[8], dhost[64], dport[16];
    char user[160], pass[160];
    cb_buf reply = {0};
    FILE *ctl = NULL, *out = NULL;
    int cfd = -1, dfd = -1, code;
    cb_status st = CB_IO_ERROR;
    char *chunk;

    if (cb_url_parse(url, &u) != CB_OK) return CB_INVALID_ARGUMENT;
    if (strcmp(u.scheme, "ftp") != 0) { cb_set_error("not an ftp URL: %s", url); return CB_UNSUPPORTED; }
    snprintf(port, sizeof port, "%s", u.port[0] ? u.port : "21");
    cfd = cb_tcp_connect(u.host, port);
    if (cfd < 0) return CB_IO_ERROR;
    ctl = fdopen(cfd, "r+");
    if (!ctl) { close(cfd); cb_set_error("fdopen failed"); return CB_ERROR; }

    if (read_reply(ctl, &reply) != 220) { cb_set_error("FTP greeting failed"); goto done; }
    snprintf(user, sizeof user, "%s", u.user[0] ? u.user : "anonymous");
    snprintf(pass, sizeof pass, "%s", u.pass[0] ? u.pass : "carbine@localhost");
    cb_buf_free(&reply);
    code = command(ctl, &reply, "USER %s", user);
    if (code == 331) { cb_buf_free(&reply); code = command(ctl, &reply, "PASS %s", pass); }
    if (code != 230 && code != 202) { cb_set_error("FTP login refused by %s", u.host); goto done; }
    cb_buf_free(&reply);
    if (command(ctl, &reply, "TYPE %s", "I") != 200) { cb_set_error("FTP TYPE I failed"); goto done; }
    cb_buf_free(&reply);
    if (command(ctl, &reply, "PASV", NULL) != 227) { cb_set_error("FTP PASV failed"); goto done; }
    if (parse_pasv(reply.data ? reply.data : "", dhost, sizeof dhost, dport, sizeof dport) != 0) {
        cb_set_error("bad FTP PASV reply");
        goto done;
    }
    cb_buf_free(&reply);

    dfd = cb_tcp_connect(dhost, dport);
    if (dfd < 0) goto done;

    code = command(ctl, &reply, "RETR %s", u.path);
    if (code != 150 && code != 125) {
        cb_set_error("FTP RETR %s failed: %s", u.path, reply.data ? reply.data : "");
        st = CB_NOT_FOUND;
        goto done;
    }
    out = fopen(dest, "wb");
    if (!out) { cb_set_error("cannot write %s: %s", dest, strerror(errno)); goto done; }
    chunk = malloc(8192);
    if (!chunk) goto done;
    for (;;) {
        ssize_t r = recv(dfd, chunk, 8192, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r < 0) { free(chunk); cb_set_error("FTP data transfer failed"); goto done; }
        if (r == 0) break;
        if (fwrite(chunk, 1, (size_t)r, out) != (size_t)r) { free(chunk); goto done; }
    }
    free(chunk);
    close(dfd);
    dfd = -1;
    cb_buf_free(&reply);
    if (read_reply(ctl, &reply) != 226) { cb_set_error("FTP transfer not confirmed"); goto done; }
    st = CB_OK;

done:
    if (dfd >= 0) close(dfd);
    if (out && fclose(out) != 0 && st == CB_OK) st = CB_IO_ERROR;
    if (st != CB_OK && out) remove(dest);
    if (ctl) {
        cb_buf_free(&reply);
        command(ctl, NULL, "QUIT", NULL);
        fclose(ctl);
    }
    cb_buf_free(&reply);
    return st;
}
