/**
 * @file mulib-archive.h
 * @brief Checked memory-only POSIX USTAR reader/writer for regular files/directories.
 * No extraction, links, special files, PAX, GNU extensions, or compression.
 * Define MULIB_IMPL_ARCHIVE and MULIB_IMPL_BUFFER in one translation unit.
 */
#ifndef MULIB_ARCHIVE_H
#define MULIB_ARCHIVE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief next returns ENTRY/OK; all failures are negative, all writer successes zero. */
enum mulib_archive_result {
    MULIB_ARCHIVE_OK = 0, MULIB_ARCHIVE_ENTRY = 1,
    MULIB_ARCHIVE_EINVAL = -1, MULIB_ARCHIVE_EFORMAT = -2,
    MULIB_ARCHIVE_EUNSAFE = -3, MULIB_ARCHIVE_ENOMEM = -4,
    MULIB_ARCHIVE_EOVERFLOW = -5, MULIB_ARCHIVE_ESTATE = -6
};
/** @brief Borrowed input and cursor; initialize with mulib_archive_reader_init. */
typedef struct mulib_archive_reader {
    const unsigned char *data; size_t size, offset; int ended;
} mulib_archive_reader_t;
/** @brief One checked entry. data is borrowed; type is '0' (file) or '5' (directory). */
typedef struct mulib_archive_entry {
    char path[257]; const unsigned char *data; size_t size;
    uint64_t mtime; uint32_t mode, uid, gid; char type;
} mulib_archive_entry_t;
/** @brief Owning byte buffer. Do not copy; initialize, add, finish, then free. */
typedef struct mulib_archive_writer { mulib_buffer_t bytes; int finished; } mulib_archive_writer_t;
/** @brief Check a portable relative path: no absolute/drive paths, backslashes,
 * empty, dot or dot-dot components; only directory paths may have a final slash.
 */
int mulib_archive_safe_path(const char *path, int directory);
/** @brief Initialize a reader over borrowed bytes, requiring a 512-byte multiple. */
int mulib_archive_reader_init(mulib_archive_reader_t *reader, const void *data, size_t size);
/** @brief Return ENTRY, OK (validated two-zero-block end), or a negative error.
 * Reject unsafe paths/types and bad checksums. Errors are sticky until reinit.
 * End must be followed only by zero bytes. Output changes only for an entry.
 */
int mulib_archive_next(mulib_archive_reader_t *reader, mulib_archive_entry_t *entry);
/** @brief Initialize a writer; does not release an already live writer. */
void mulib_archive_writer_init(mulib_archive_writer_t *writer);
/** @brief Append a file, with uid/gid zero. Mode <=07777; mtime fits 11 octal digits.
 * Input must not alias writer storage. Failure preserves logical archive bytes.
 */
int mulib_archive_add(mulib_archive_writer_t *writer, const char *path, const void *data,
                      size_t size, uint32_t mode, uint64_t mtime);
/** @brief Append a directory (no payload), with the same metadata limits. */
int mulib_archive_add_directory(mulib_archive_writer_t *writer, const char *path,
                                uint32_t mode, uint64_t mtime);
/** @brief Append the two end blocks, idempotently. No further entries may be added. */
int mulib_archive_finish(mulib_archive_writer_t *writer);
/** @brief Release all bytes and reset the writer. */
void mulib_archive_writer_free(mulib_archive_writer_t *writer);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ARCHIVE_H */

#if defined(MULIB_IMPL_ARCHIVE) && !defined(MULIB_ARCHIVE_IMPLEMENTED)
#define MULIB_ARCHIVE_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_archive_zero_(const unsigned char *p, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) if (p[i]) return 0;
    return 1;
}
int mulib_archive_safe_path(const char *path, int directory) {
    size_t i, start = 0, n;
    if (!path || (directory != 0 && directory != 1)) return 0;
    n = strlen(path);
    if (!n || n > 256 || path[0] == '/') return 0;
    for (i = 0; i <= n; ++i) {
        if (i < n && (path[i] == '\\' || path[i] == ':')) return 0;
        if (i == n || path[i] == '/') {
            size_t len = i - start;
            if (!len) return directory && i == n && i > 0 && path[i - 1] == '/';
            if ((len == 1 && path[start] == '.') ||
                (len == 2 && path[start] == '.' && path[start + 1] == '.')) return 0;
            start = i + 1;
        }
    }
    return 1;
}
static int mulib_archive_octal_(const unsigned char *p, size_t n, uint64_t *out) {
    size_t i = 0;
    uint64_t value = 0;
    int digits = 0;
    while (i < n && p[i] == ' ') ++i;
    while (i < n && p[i] >= '0' && p[i] <= '7') {
        if (value > (UINT64_MAX - (uint64_t)(p[i] - '0')) / 8) return 0;
        value = value * 8 + (uint64_t)(p[i++] - '0'); digits = 1;
    }
    while (i < n && (p[i] == 0 || p[i] == ' ')) ++i;
    if (i != n) return 0;
    *out = digits ? value : 0;
    return 1;
}
static size_t mulib_archive_fieldlen_(const unsigned char *p, size_t cap) {
    size_t n = 0;
    while (n < cap && p[n]) ++n;
    return n;
}
int mulib_archive_reader_init(mulib_archive_reader_t *r, const void *data, size_t size) {
    if (!r || (!data && size)) return MULIB_ARCHIVE_EINVAL;
    if (size < 1024 || size % 512) return MULIB_ARCHIVE_EFORMAT;
    r->data = (const unsigned char *)data; r->size = size; r->offset = 0; r->ended = 0;
    return MULIB_ARCHIVE_OK;
}
static int mulib_archive_read_(mulib_archive_reader_t *r, mulib_archive_entry_t *out) {
    const unsigned char *h;
    mulib_archive_entry_t e;
    uint64_t checksum, size, mode, uid, gid, mtime;
    uint64_t sum = 0;
    size_t i, name, prefix, padded;
    if (r->offset > r->size || r->size - r->offset < 512) return MULIB_ARCHIVE_EFORMAT;
    h = r->data + r->offset;
    if (mulib_archive_zero_(h, 512)) {
        if (r->size - r->offset < 1024 || !mulib_archive_zero_(h, r->size - r->offset))
            return MULIB_ARCHIVE_EFORMAT;
        r->offset = r->size; r->ended = 1; return MULIB_ARCHIVE_OK;
    }
    if (memcmp(h + 257, "ustar\0", 6) || memcmp(h + 263, "00", 2)) return MULIB_ARCHIVE_EFORMAT;
    if (!mulib_archive_octal_(h + 148, 8, &checksum)) return MULIB_ARCHIVE_EFORMAT;
    for (i = 0; i < 512; ++i) sum += (i >= 148 && i < 156) ? 32u : h[i];
    if (sum != checksum) return MULIB_ARCHIVE_EFORMAT;
    if (h[156] != 0 && h[156] != '0' && h[156] != '5') return MULIB_ARCHIVE_EUNSAFE;
    memset(&e, 0, sizeof(e)); e.type = h[156] == '5' ? '5' : '0';
    name = mulib_archive_fieldlen_(h, 100); prefix = mulib_archive_fieldlen_(h + 345, 155);
    if (!name) return MULIB_ARCHIVE_EFORMAT;
    if (prefix) { memcpy(e.path, h + 345, prefix); e.path[prefix++] = '/'; }
    memcpy(e.path + prefix, h, name); e.path[prefix + name] = '\0';
    if (!mulib_archive_safe_path(e.path, e.type == '5')) return MULIB_ARCHIVE_EUNSAFE;
    if (!mulib_archive_octal_(h + 100, 8, &mode) ||
        !mulib_archive_octal_(h + 108, 8, &uid) || !mulib_archive_octal_(h + 116, 8, &gid) ||
        !mulib_archive_octal_(h + 124, 12, &size) || !mulib_archive_octal_(h + 136, 12, &mtime))
        return MULIB_ARCHIVE_EFORMAT;
    if (mode > UINT32_MAX || uid > UINT32_MAX || gid > UINT32_MAX || size > SIZE_MAX - 511)
        return MULIB_ARCHIVE_EOVERFLOW;
    if (e.type == '5' && size) return MULIB_ARCHIVE_EFORMAT;
    padded = ((size_t)size + 511) / 512 * 512;
    if (padded > r->size - r->offset - 512) return MULIB_ARCHIVE_EFORMAT;
    e.mode = (uint32_t)mode; e.uid = (uint32_t)uid; e.gid = (uint32_t)gid;
    e.size = (size_t)size; e.mtime = mtime; e.data = h + 512;
    r->offset += 512 + padded; *out = e; return MULIB_ARCHIVE_ENTRY;
}
int mulib_archive_next(mulib_archive_reader_t *r, mulib_archive_entry_t *entry) {
    int result;
    if (!r || !entry || !r->data) return MULIB_ARCHIVE_EINVAL;
    if (r->ended < 0) return MULIB_ARCHIVE_ESTATE;
    if (r->ended) return MULIB_ARCHIVE_OK;
    result = mulib_archive_read_(r, entry);
    if (result < 0) r->ended = -1;
    return result;
}
void mulib_archive_writer_init(mulib_archive_writer_t *w) {
    if (w) { mulib_buffer_init(&w->bytes); w->finished = 0; }
}
static void mulib_archive_store_octal_(unsigned char *p, size_t n, uint64_t value) {
    size_t i;
    memset(p, '0', n); p[n - 1] = 0;
    for (i = n - 1; i; ) { --i; p[i] = (unsigned char)('0' + (value & 7)); value >>= 3; }
}
static int mulib_archive_write_(mulib_archive_writer_t *w, const char *path, const void *data,
                                size_t size, uint32_t mode, uint64_t mtime, int directory) {
    unsigned char h[512] = {0}, pad[512] = {0};
    size_t len, split = 0, i, old, padding, total;
    unsigned sum = 0;
    if (!w || !path || (!data && size)) return MULIB_ARCHIVE_EINVAL;
    if (w->finished) return MULIB_ARCHIVE_ESTATE;
    if (!mulib_archive_safe_path(path, directory)) return MULIB_ARCHIVE_EUNSAFE;
    if (mode > 07777 || mtime > UINT64_C(8589934591) || (uintmax_t)size > UINT64_C(8589934591))
        return MULIB_ARCHIVE_EOVERFLOW;
    len = strlen(path);
    if (len > 100) {
        for (i = 1; i < len; ++i)
            if (path[i] == '/' && i <= 155 && len - i - 1 <= 100 && i + 1 < len) split = i;
        if (!split) return MULIB_ARCHIVE_EOVERFLOW;
        memcpy(h + 345, path, split); memcpy(h, path + split + 1, len - split - 1);
    } else memcpy(h, path, len);
    mulib_archive_store_octal_(h + 100, 8, mode);
    mulib_archive_store_octal_(h + 108, 8, 0); mulib_archive_store_octal_(h + 116, 8, 0);
    mulib_archive_store_octal_(h + 124, 12, (uint64_t)size);
    mulib_archive_store_octal_(h + 136, 12, mtime);
    memset(h + 148, ' ', 8); h[156] = directory ? '5' : '0';
    memcpy(h + 257, "ustar\0" "00", 8);
    for (i = 0; i < sizeof(h); ++i) sum += h[i];
    mulib_archive_store_octal_(h + 148, 7, sum); h[155] = ' ';
    padding = (512 - size % 512) % 512;
    if (size > SIZE_MAX - 512 || padding > SIZE_MAX - 512 - size) return MULIB_ARCHIVE_EOVERFLOW;
    total = 512 + size + padding;
    if (w->bytes.length > SIZE_MAX - total) return MULIB_ARCHIVE_EOVERFLOW;
    old = w->bytes.length;
    if (!mulib_buffer_append(&w->bytes, h, 512) || !mulib_buffer_append(&w->bytes, data, size) ||
        !mulib_buffer_append(&w->bytes, pad, padding)) {
        w->bytes.length = old; return MULIB_ARCHIVE_ENOMEM;
    }
    return MULIB_ARCHIVE_OK;
}
int mulib_archive_add(mulib_archive_writer_t *w, const char *path, const void *data,
                      size_t size, uint32_t mode, uint64_t mtime) {
    return mulib_archive_write_(w, path, data, size, mode, mtime, 0);
}
int mulib_archive_add_directory(mulib_archive_writer_t *w, const char *path,
                                uint32_t mode, uint64_t mtime) {
    return mulib_archive_write_(w, path, NULL, 0, mode, mtime, 1);
}
int mulib_archive_finish(mulib_archive_writer_t *w) {
    unsigned char end[1024] = {0};
    if (!w) return MULIB_ARCHIVE_EINVAL;
    if (w->finished) return MULIB_ARCHIVE_OK;
    if (w->bytes.length > SIZE_MAX - sizeof(end)) return MULIB_ARCHIVE_EOVERFLOW;
    if (!mulib_buffer_append(&w->bytes, end, sizeof(end))) return MULIB_ARCHIVE_ENOMEM;
    w->finished = 1; return MULIB_ARCHIVE_OK;
}
void mulib_archive_writer_free(mulib_archive_writer_t *w) {
    if (w) { mulib_buffer_free(&w->bytes); w->finished = 0; }
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
