/* Carbine tooling: fetching files over HTTP/1.1 (plain text only).
 *
 * https:// is refused with CB_UNSUPPORTED: no TLS library is linked, and a
 * silent fallback to plain HTTP would be worse than an error. */
#define _GNU_SOURCE

#include "internal.h"

#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

cb_status cb_url_parse(const char *text, cb_url *u) {
    const char *p, *sep, *at, *colon;
    size_t n;

    memset(u, 0, sizeof *u);
    sep = strstr(text, "://");
    if (!sep) { cb_set_error("URL has no scheme: %s", text); return CB_INVALID_ARGUMENT; }
    n = (size_t)(sep - text);
    if (n == 0 || n >= sizeof u->scheme) { cb_set_error("bad scheme in %s", text); return CB_INVALID_ARGUMENT; }
    memcpy(u->scheme, text, n);
    for (size_t i = 0; i < n; ++i) u->scheme[i] = (char)((u->scheme[i] >= 'A' && u->scheme[i] <= 'Z') ? u->scheme[i] + 32 : u->scheme[i]);
    p = sep + 3;

    /* Authority ends at the first '/'. */
    {
        const char *slash = strchr(p, '/');
        size_t alen = slash ? (size_t)(slash - p) : strlen(p);
        char *auth = malloc(alen + 1);
        char *host;
        if (!auth) return CB_ERROR;
        memcpy(auth, p, alen);
        auth[alen] = '\0';
        host = auth;
        at = strrchr(auth, '@');
        if (at) {
            char *userinfo = auth;
            size_t ulen = (size_t)(at - auth);
            char *pw = memchr(userinfo, ':', ulen);
            if (pw) {
                size_t pl = ulen - (size_t)(pw - userinfo) - 1;
                if (pl >= sizeof u->pass) { free(auth); return CB_INVALID_ARGUMENT; }
                memcpy(u->pass, pw + 1, pl);
                ulen = (size_t)(pw - userinfo);
            }
            if (ulen >= sizeof u->user) { free(auth); return CB_INVALID_ARGUMENT; }
            memcpy(u->user, userinfo, ulen);
            host = (char *)at + 1;
        }
        colon = strrchr(host, ':');
        if (colon) {
            size_t hl = (size_t)(colon - host), pl = strlen(colon + 1);
            if (hl >= sizeof u->host || pl >= sizeof u->port) { free(auth); return CB_INVALID_ARGUMENT; }
            memcpy(u->host, host, hl);
            memcpy(u->port, colon + 1, pl);
        } else {
            if (strlen(host) >= sizeof u->host) { free(auth); return CB_INVALID_ARGUMENT; }
            strcpy(u->host, host);
        }
        free(auth);
        if (u->host[0] == '\0') { cb_set_error("URL has no host: %s", text); return CB_INVALID_ARGUMENT; }
        p = slash ? slash : p + alen;
    }
    if (*p == '/') {
        if (strlen(p) >= sizeof u->path) return CB_INVALID_ARGUMENT;
        strcpy(u->path, p);
    } else {
        strcpy(u->path, "/");
    }
    return CB_OK;
}

/* Connects to host:port (port as text) and returns the socket, or -1. */
int cb_tcp_connect(const char *host, const char *port) {
    struct addrinfo hints, *res, *ai;
    int fd = -1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        cb_set_error("cannot resolve %s", host);
        return -1;
    }
    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) cb_set_error("cannot connect to %s:%s: %s", host, port, strerror(errno));
    return fd;
}

/* Writes all of data to fd. */
int cb_send_all(int fd, const char *data, size_t n) {
    while (n > 0) {
        ssize_t w = send(fd, data, n, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        data += w;
        n -= (size_t)w;
    }
    return 0;
}

/* Reads until EOF into b. */
int cb_recv_all(int fd, cb_buf *b) {
    char chunk[8192];
    for (;;) {
        ssize_t r = recv(fd, chunk, sizeof chunk, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;
        if (cb_buf_add(b, chunk, (size_t)r) != CB_OK) return -1;
    }
}

/* Decodes a chunked body in place into out. */
static cb_status dechunk(const char *body, size_t len, cb_buf *out) {
    const char *p = body, *end = body + len;
    for (;;) {
        char *after;
        unsigned long size = strtoul(p, &after, 16);
        if (after == p) { cb_set_error("bad chunk size"); return CB_IO_ERROR; }
        p = after;
        while (p < end && *p != '\n') ++p;
        if (p >= end) { cb_set_error("truncated chunk header"); return CB_IO_ERROR; }
        ++p;
        if (size == 0) return CB_OK;
        if ((size_t)(end - p) < size) { cb_set_error("truncated chunk"); return CB_IO_ERROR; }
        if (cb_buf_add(out, p, size) != CB_OK) return CB_ERROR;
        p += size;
        while (p < end && (*p == '\r' || *p == '\n')) ++p;
    }
}

cb_status cb_http_get(const char *url_text, const char *dest) {
    cb_url u;
    char port[8];
    char request[2048];
    cb_buf response = {0}, body = {0};
    int fd;
    char *head_end, *status, *hdr;
    cb_status st = CB_OK;
    FILE *out;

    if (cb_url_parse(url_text, &u) != CB_OK) return CB_INVALID_ARGUMENT;
    if (strcmp(u.scheme, "http") != 0) {
        cb_set_error("not an http URL: %s", url_text);
        return CB_UNSUPPORTED;
    }
    snprintf(port, sizeof port, "%s", u.port[0] ? u.port : "80");
    fd = cb_tcp_connect(u.host, port);
    if (fd < 0) return CB_IO_ERROR;
    if (snprintf(request, sizeof request,
                 "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: carbine/" "1.0.0\r\nConnection: close\r\n\r\n",
                 u.path, u.host) >= (int)sizeof request) {
        close(fd);
        cb_set_error("request too long");
        return CB_INVALID_ARGUMENT;
    }
    if (cb_send_all(fd, request, strlen(request)) != 0 || cb_recv_all(fd, &response) != 0) {
        close(fd);
        cb_set_error("HTTP transfer failed for %s", url_text);
        return CB_IO_ERROR;
    }
    close(fd);

    head_end = response.data ? strstr(response.data, "\r\n\r\n") : NULL;
    if (!head_end) { cb_set_error("malformed HTTP response from %s", u.host); cb_buf_free(&response); return CB_IO_ERROR; }
    status = response.data;
    if (strncmp(status, "HTTP/1.", 7) != 0 || status[8] != ' ' || strncmp(status + 9, "200", 3) != 0) {
        cb_set_error("HTTP error from %s: %.*s", u.host, (int)strcspn(status, "\r\n"), status);
        cb_buf_free(&response);
        return CB_NOT_FOUND;
    }
    hdr = strcasestr(response.data, "\r\nTransfer-Encoding: chunked");
    if (hdr && hdr < head_end) {
        st = dechunk(head_end + 4, (size_t)(response.data + response.len - (head_end + 4)), &body);
        if (st != CB_OK) { cb_buf_free(&response); cb_buf_free(&body); return st; }
    } else {
        cb_buf_add(&body, head_end + 4, (size_t)(response.data + response.len - (head_end + 4)));
    }
    cb_buf_free(&response);

    out = fopen(dest, "wb");
    if (!out) {
        cb_set_error("cannot write %s: %s", dest, strerror(errno));
        cb_buf_free(&body);
        return CB_IO_ERROR;
    }
    if (body.len && fwrite(body.data, 1, body.len, out) != body.len) st = CB_IO_ERROR;
    if (fclose(out) != 0) st = CB_IO_ERROR;
    cb_buf_free(&body);
    return st;
}

cb_status cb_ftp_get(const char *url, const char *dest);

cb_status cb_fetch(const char *url, const char *dest) {
    cb_url u;
    if (cb_url_parse(url, &u) != CB_OK) return CB_INVALID_ARGUMENT;
    if (strcmp(u.scheme, "http") == 0) return cb_http_get(url, dest);
    if (strcmp(u.scheme, "ftp") == 0) return cb_ftp_get(url, dest);
    cb_set_error("unsupported URL scheme '%s' (https needs a TLS build)", u.scheme);
    return CB_UNSUPPORTED;
}
