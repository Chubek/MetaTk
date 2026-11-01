/**
 * @file mulib-kvdb.h
 * @brief Binary-safe sorted key/value snapshots with CRC and atomic POSIX commits.
 * Define MULIB_IMPL_KVDB, MULIB_IMPL_BUFFER, MULIB_IMPL_VARINT, MULIB_IMPL_CRC32,
 * and MULIB_IMPL_ENDIAN in exactly one translation unit. No external dependencies.
 */
#ifndef MULIB_KVDB_H
#define MULIB_KVDB_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Owned, non-thread-safe database; close discards uncommitted changes. */
typedef struct mulib_kvdb mulib_kvdb_t;
/** @brief Negative errors occur before replacement; UNCERTAIN is post-replacement. */
enum mulib_kvdb_result {
    MULIB_KVDB_OK = 0,
    /** Replacement succeeded, but directory fsync failed: visible, durability uncertain. */
    MULIB_KVDB_UNCERTAIN = 1,
    MULIB_KVDB_EINVAL = -1, MULIB_KVDB_ENOMEM = -2, MULIB_KVDB_EIO = -3,
    MULIB_KVDB_ECORRUPT = -4, MULIB_KVDB_ENOTFOUND = -5,
    MULIB_KVDB_ENOTSUP = -6, MULIB_KVDB_EOVERFLOW = -7
};
/** @brief Open and integrity-check an existing snapshot. create=1 permits ENOENT
 * and creates an empty dirty in-memory database, not a file until commit.
 * Output is unchanged on failure. Keys and values may be empty or contain NULs.
 */
int mulib_kvdb_open(const char *path, int create, mulib_kvdb_t **database);
/** @brief Free the database, accepting NULL. No implicit commit. */
void mulib_kvdb_close(mulib_kvdb_t *database);
/** @brief Number of stored pairs; NULL has size zero. */
size_t mulib_kvdb_size(const mulib_kvdb_t *database);
/** @brief Whether a commit is needed; NULL is clean. */
int mulib_kvdb_dirty(const mulib_kvdb_t *database);
/** @brief Borrow a value until its replacement/deletion or close. Missing is ENOTFOUND. */
int mulib_kvdb_get(const mulib_kvdb_t *database, const void *key, size_t key_size,
                   const void **value, size_t *value_size);
/** @brief Copy a binary key/value. Failure leaves the logical database unchanged. */
int mulib_kvdb_set(mulib_kvdb_t *database, const void *key, size_t key_size,
                   const void *value, size_t value_size);
/** @brief Remove a key, or return ENOTFOUND without changing the database. */
int mulib_kvdb_remove(mulib_kvdb_t *database, const void *key, size_t key_size);
/** @brief Borrow the index-th pair in unsigned-byte lexicographic key order.
 * Enumeration indexes change on insertion/removal. Outputs unchanged on failure.
 */
int mulib_kvdb_at(const mulib_kvdb_t *database, size_t index, const void **key,
                  size_t *key_size, const void **value, size_t *value_size);
/** @brief Serialize, fsync a mode-0600 exclusive sibling temp file, rename, fsync
 * parent directory. Negative errors occur before replacement; dirty is retained.
 * OK/UNCERTAIN clear dirty. No locking: concurrent writers are last-commit-wins.
 * Commit is explicitly unsupported off POSIX. Use a trusted, stable parent directory.
 */
int mulib_kvdb_commit(mulib_kvdb_t *database);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_KVDB_H */

#if defined(MULIB_IMPL_KVDB) && !defined(MULIB_KVDB_IMPLEMENTED)
#define MULIB_KVDB_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "mulib-buffer.h"
#include "mulib-varint.h"
#include "mulib-crc32.h"
#include "mulib-endian.h"
#if defined(__unix__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define MULIB_KVDB_POSIX_ 1
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_kvdb_pair_ {
    unsigned char *key, *value; size_t key_size, value_size;
} mulib_kvdb_pair_;
struct mulib_kvdb {
    char *path; mulib_kvdb_pair_ *pairs; size_t size, capacity;
    int dirty; uint64_t sequence;
};
static int mulib_kvdb_compare_(const void *a, size_t an, const void *b, size_t bn) {
    size_t n = an < bn ? an : bn;
    int order = n ? memcmp(a, b, n) : 0;
    if (order) return order;
    return an < bn ? -1 : an > bn ? 1 : 0;
}
static size_t mulib_kvdb_find_(const mulib_kvdb_t *db, const void *key, size_t n, int *found) {
    size_t lo = 0, hi = db->size;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int order = mulib_kvdb_compare_(db->pairs[mid].key, db->pairs[mid].key_size, key, n);
        if (order < 0) lo = mid + 1; else hi = mid;
    }
    *found = lo < db->size && !mulib_kvdb_compare_(db->pairs[lo].key, db->pairs[lo].key_size, key, n);
    return lo;
}
static unsigned char *mulib_kvdb_copy_(const void *p, size_t n) {
    unsigned char *copy = (unsigned char *)malloc(n ? n : 1);
    if (copy && n) memcpy(copy, p, n);
    return copy;
}
void mulib_kvdb_close(mulib_kvdb_t *db) {
    size_t i;
    if (!db) return;
    for (i = 0; i < db->size; ++i) { free(db->pairs[i].key); free(db->pairs[i].value); }
    free(db->pairs); free(db->path); free(db);
}
size_t mulib_kvdb_size(const mulib_kvdb_t *db) { return db ? db->size : 0; }
int mulib_kvdb_dirty(const mulib_kvdb_t *db) { return db && db->dirty; }
int mulib_kvdb_get(const mulib_kvdb_t *db, const void *key, size_t n, const void **value, size_t *size) {
    int found;
    size_t i;
    if (!db || (!key && n) || !value || !size) return MULIB_KVDB_EINVAL;
    i = mulib_kvdb_find_(db, key, n, &found);
    if (!found) return MULIB_KVDB_ENOTFOUND;
    *value = db->pairs[i].value; *size = db->pairs[i].value_size; return MULIB_KVDB_OK;
}
int mulib_kvdb_at(const mulib_kvdb_t *db, size_t i, const void **key, size_t *kn,
                  const void **value, size_t *vn) {
    if (!db || !key || !kn || !value || !vn) return MULIB_KVDB_EINVAL;
    if (i >= db->size) return MULIB_KVDB_ENOTFOUND;
    *key = db->pairs[i].key; *kn = db->pairs[i].key_size;
    *value = db->pairs[i].value; *vn = db->pairs[i].value_size; return MULIB_KVDB_OK;
}
int mulib_kvdb_set(mulib_kvdb_t *db, const void *key, size_t kn, const void *value, size_t vn) {
    size_t i;
    int found;
    unsigned char *key_copy, *value_copy;
    if (!db || (!key && kn) || (!value && vn)) return MULIB_KVDB_EINVAL;
    if ((uintmax_t)kn > UINT64_MAX || (uintmax_t)vn > UINT64_MAX) return MULIB_KVDB_EOVERFLOW;
    i = mulib_kvdb_find_(db, key, kn, &found);
    value_copy = mulib_kvdb_copy_(value, vn);
    if (!value_copy) return MULIB_KVDB_ENOMEM;
    if (found) {
        free(db->pairs[i].value); db->pairs[i].value = value_copy; db->pairs[i].value_size = vn;
        db->dirty = 1; return MULIB_KVDB_OK;
    }
    key_copy = mulib_kvdb_copy_(key, kn);
    if (!key_copy) { free(value_copy); return MULIB_KVDB_ENOMEM; }
    if (db->size == db->capacity) {
        size_t cap = db->capacity ? db->capacity : 8;
        void *memory;
        if (cap > SIZE_MAX / 2 || cap * 2 > SIZE_MAX / sizeof(*db->pairs)) {
            free(key_copy); free(value_copy); return MULIB_KVDB_EOVERFLOW;
        }
        cap *= 2;
        memory = realloc(db->pairs, cap * sizeof(*db->pairs));
        if (!memory) { free(key_copy); free(value_copy); return MULIB_KVDB_ENOMEM; }
        db->pairs = (mulib_kvdb_pair_ *)memory; db->capacity = cap;
    }
    memmove(db->pairs + i + 1, db->pairs + i, (db->size - i) * sizeof(*db->pairs));
    db->pairs[i].key = key_copy; db->pairs[i].key_size = kn;
    db->pairs[i].value = value_copy; db->pairs[i].value_size = vn;
    ++db->size; db->dirty = 1; return MULIB_KVDB_OK;
}
int mulib_kvdb_remove(mulib_kvdb_t *db, const void *key, size_t kn) {
    size_t i;
    int found;
    if (!db || (!key && kn)) return MULIB_KVDB_EINVAL;
    i = mulib_kvdb_find_(db, key, kn, &found);
    if (!found) return MULIB_KVDB_ENOTFOUND;
    free(db->pairs[i].key); free(db->pairs[i].value);
    memmove(db->pairs + i, db->pairs + i + 1, (db->size - i - 1) * sizeof(*db->pairs));
    --db->size; db->dirty = 1; return MULIB_KVDB_OK;
}
static int mulib_kvdb_varint_(const unsigned char *p, size_t n, size_t *position, uint64_t *out) {
    size_t used;
    unsigned char encoded[10];
    if (*position > n || !mulib_varint_decode(p + *position, n - *position, out, &used)) return 0;
    if (mulib_varint_encode(*out, encoded) != used) return 0;
    *position += used; return 1;
}
static int mulib_kvdb_decode_(mulib_kvdb_t *db, const unsigned char *p, size_t n) {
    uint64_t count;
    size_t position = 8, i;
    if (n < 13 || memcmp(p, "MuKVDB\001\0", 8) ||
        mulib_endian_load_le32(p + n - 4) != mulib_crc32(p, n - 4)) return MULIB_KVDB_ECORRUPT;
    n -= 4;
    if (!mulib_kvdb_varint_(p, n, &position, &count) || count > SIZE_MAX / sizeof(*db->pairs) ||
        count > (n - position) / 2) return MULIB_KVDB_ECORRUPT;
    if (count) {
        db->pairs = (mulib_kvdb_pair_ *)calloc((size_t)count, sizeof(*db->pairs));
        if (!db->pairs) return MULIB_KVDB_ENOMEM;
        db->capacity = (size_t)count;
    }
    for (i = 0; i < (size_t)count; ++i) {
        uint64_t kn, vn;
        mulib_kvdb_pair_ *pair = db->pairs + i;
        if (!mulib_kvdb_varint_(p, n, &position, &kn) || !mulib_kvdb_varint_(p, n, &position, &vn) ||
            kn > n - position || vn > n - position - (size_t)kn) return MULIB_KVDB_ECORRUPT;
        if (i && mulib_kvdb_compare_(db->pairs[i - 1].key, db->pairs[i - 1].key_size,
                                     p + position, (size_t)kn) >= 0) return MULIB_KVDB_ECORRUPT;
        pair->key = mulib_kvdb_copy_(p + position, (size_t)kn);
        pair->value = mulib_kvdb_copy_(p + position + (size_t)kn, (size_t)vn);
        if (!pair->key || !pair->value) {
            free(pair->key); free(pair->value); pair->key = NULL; pair->value = NULL;
            return MULIB_KVDB_ENOMEM;
        }
        pair->key_size = (size_t)kn; pair->value_size = (size_t)vn;
        ++db->size; position += (size_t)kn + (size_t)vn;
    }
    return position == n ? MULIB_KVDB_OK : MULIB_KVDB_ECORRUPT;
}
int mulib_kvdb_open(const char *path, int create, mulib_kvdb_t **out) {
    mulib_kvdb_t *db;
    mulib_buffer_t bytes;
    FILE *file;
    unsigned char chunk[4096];
    size_t n, len;
    int rc = MULIB_KVDB_OK, saved;
    if (!path || !*path || !out || (create != 0 && create != 1)) return MULIB_KVDB_EINVAL;
    len = strlen(path);
    if (len == SIZE_MAX) return MULIB_KVDB_EOVERFLOW;
    db = (mulib_kvdb_t *)calloc(1, sizeof(*db));
    if (!db) return MULIB_KVDB_ENOMEM;
    db->path = (char *)malloc(len + 1);
    if (!db->path) { mulib_kvdb_close(db); return MULIB_KVDB_ENOMEM; }
    memcpy(db->path, path, len + 1);
    file = fopen(path, "rb");
    if (!file) {
        saved = errno;
        if (saved == ENOENT && create) { db->dirty = 1; *out = db; return MULIB_KVDB_OK; }
        mulib_kvdb_close(db); errno = saved;
        return saved == ENOENT ? MULIB_KVDB_ENOTFOUND : MULIB_KVDB_EIO;
    }
    mulib_buffer_init(&bytes);
    while ((n = fread(chunk, 1, sizeof(chunk), file)) != 0) {
        if (!mulib_buffer_append(&bytes, chunk, n)) { rc = MULIB_KVDB_ENOMEM; break; }
    }
    if (ferror(file)) rc = MULIB_KVDB_EIO;
    if (fclose(file) != 0) rc = MULIB_KVDB_EIO;
    if (!rc) rc = mulib_kvdb_decode_(db, bytes.data, bytes.length);
    mulib_buffer_free(&bytes);
    if (rc) { mulib_kvdb_close(db); return rc; }
    *out = db; return MULIB_KVDB_OK;
}
#ifdef MULIB_KVDB_POSIX_
static int mulib_kvdb_encode_(const mulib_kvdb_t *db, mulib_buffer_t *bytes) {
    unsigned char encoded[10], crc[4];
    size_t i, n;
    if (!mulib_buffer_append(bytes, "MuKVDB\001\0", 8)) return MULIB_KVDB_ENOMEM;
    if ((uintmax_t)db->size > UINT64_MAX) return MULIB_KVDB_EOVERFLOW;
    n = mulib_varint_encode((uint64_t)db->size, encoded);
    if (!mulib_buffer_append(bytes, encoded, n)) return MULIB_KVDB_ENOMEM;
    for (i = 0; i < db->size; ++i) {
        const mulib_kvdb_pair_ *pair = db->pairs + i;
        n = mulib_varint_encode((uint64_t)pair->key_size, encoded);
        if (!mulib_buffer_append(bytes, encoded, n)) return MULIB_KVDB_ENOMEM;
        n = mulib_varint_encode((uint64_t)pair->value_size, encoded);
        if (!mulib_buffer_append(bytes, encoded, n) ||
            !mulib_buffer_append(bytes, pair->key, pair->key_size) ||
            !mulib_buffer_append(bytes, pair->value, pair->value_size)) return MULIB_KVDB_ENOMEM;
    }
    mulib_endian_store_le32(crc, mulib_crc32(bytes->data, bytes->length));
    return mulib_buffer_append(bytes, crc, 4) ? MULIB_KVDB_OK : MULIB_KVDB_ENOMEM;
}
static int mulib_kvdb_sync_(int fd) {
    int rc;
    do { rc = fsync(fd); } while (rc < 0 && errno == EINTR);
    return rc;
}
static int mulib_kvdb_replace_(mulib_kvdb_t *db, const mulib_buffer_t *bytes) {
    size_t len = strlen(db->path), parent_len, done = 0;
    char *temporary, *parent;
    const char *slash = strrchr(db->path, '/');
    int fd = -1, dirfd, rc = MULIB_KVDB_EIO, saved = 0;
    unsigned attempt;
    struct stat st;
    if (len > SIZE_MAX - 96) return MULIB_KVDB_EOVERFLOW;
    temporary = (char *)malloc(len + 96);
    parent_len = slash ? (size_t)(slash - db->path) : 0;
    if (slash && !parent_len) parent_len = 1;
    parent = (char *)malloc(parent_len + 2);
    if (!temporary || !parent) { free(temporary); free(parent); return MULIB_KVDB_ENOMEM; }
    if (slash) { memcpy(parent, db->path, parent_len); parent[parent_len] = 0; }
    else { parent[0] = '.'; parent[1] = 0; }
    dirfd = open(parent, O_RDONLY);
    free(parent);
    if (dirfd < 0) { free(temporary); return MULIB_KVDB_EIO; }
    if (fcntl(dirfd, F_SETFD, FD_CLOEXEC) < 0 || fstat(dirfd, &st) < 0 || !S_ISDIR(st.st_mode))
        goto cleanup;
    for (attempt = 0; attempt < 128; ++attempt) {
        int used = snprintf(temporary, len + 96, "%s.mulib-%lu-%llu", db->path,
                            (unsigned long)getpid(), (unsigned long long)db->sequence++);
        if (used < 0 || (size_t)used >= len + 96) { rc = MULIB_KVDB_EOVERFLOW; goto cleanup; }
        fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd >= 0 || errno != EEXIST) break;
    }
    if (fd < 0) goto cleanup;
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) goto remove_temporary;
    while (done < bytes->length) {
        size_t chunk = bytes->length - done;
        ssize_t written;
        if (chunk > 1048576) chunk = 1048576;
        written = write(fd, bytes->data + done, chunk);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { if (!written) errno = EIO; goto remove_temporary; }
        done += (size_t)written;
    }
    if (mulib_kvdb_sync_(fd) < 0) goto remove_temporary;
    if (close(fd) < 0) { fd = -1; goto remove_temporary; }
    fd = -1;
    if (rename(temporary, db->path) < 0) goto remove_temporary;
    db->dirty = 0;
    rc = mulib_kvdb_sync_(dirfd) < 0 ? MULIB_KVDB_UNCERTAIN : MULIB_KVDB_OK;
    goto cleanup;
remove_temporary:
    saved = errno;
    if (fd >= 0) (void)close(fd);
    (void)unlink(temporary);
cleanup:
    if (!saved) saved = errno;
    (void)close(dirfd); free(temporary); errno = saved; return rc;
}
#endif
int mulib_kvdb_commit(mulib_kvdb_t *db) {
    if (!db) return MULIB_KVDB_EINVAL;
#ifdef MULIB_KVDB_POSIX_
    {
        mulib_buffer_t bytes;
        int rc;
        if (!db->dirty) return MULIB_KVDB_OK;
        mulib_buffer_init(&bytes); rc = mulib_kvdb_encode_(db, &bytes);
        if (!rc) rc = mulib_kvdb_replace_(db, &bytes);
        mulib_buffer_free(&bytes); return rc;
    }
#else
    return MULIB_KVDB_ENOTSUP;
#endif
}
#ifdef __cplusplus
}
#endif
#undef MULIB_KVDB_POSIX_
#endif /* implementation */
