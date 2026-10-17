/* Carbine tooling: POSIX ustar archives (`carbine dist`).
 *
 * Writes regular files only. Names longer than 100 bytes are split at a '/'
 * into the ustar prefix (up to 155 bytes); anything longer is rejected. */
#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define BLOCK 512

static void put_octal(char *field, size_t width, unsigned long long value) {
    /* width includes the terminating NUL. */
    size_t i;
    field[width - 1] = '\0';
    for (i = width - 1; i-- > 0;) {
        field[i] = (char)('0' + (value & 7));
        value >>= 3;
    }
}

/* Splits name into prefix/name fields; returns 0 on success. */
static int split_name(const char *name, char *prefix, char *field) {
    size_t n = strlen(name);
    if (n <= 100) {
        memcpy(field, name, n);
        prefix[0] = '\0';
        return 0;
    }
    for (size_t cut = n; cut-- > 0;) {
        if (name[cut] != '/') continue;
        if (cut <= 155 && n - cut - 1 <= 100 && n - cut - 1 > 0) {
            memcpy(prefix, name, cut);
            prefix[cut] = '\0';
            memcpy(field, name + cut + 1, n - cut - 1);
            field[n - cut - 1] = '\0';
            return 0;
        }
    }
    return -1;
}

static cb_status write_header(FILE *out, const char *name, const struct stat *st) {
    unsigned char h[BLOCK];
    char prefix[156], field[101];
    unsigned int sum = 0;
    size_t i;

    if (split_name(name, prefix, field) != 0) {
        cb_set_error("archive name too long: %s", name);
        return CB_INVALID_ARGUMENT;
    }
    memset(h, 0, sizeof h);
    memcpy(h, field, strlen(field));
    put_octal((char *)h + 100, 8, (unsigned long long)(st->st_mode & 07777));
    put_octal((char *)h + 108, 8, 0);
    put_octal((char *)h + 116, 8, 0);
    put_octal((char *)h + 124, 12, (unsigned long long)st->st_size);
    put_octal((char *)h + 136, 12, (unsigned long long)st->st_mtime);
    memset(h + 148, ' ', 8);
    h[156] = '0';                                  /* regular file */
    memcpy(h + 257, "ustar", 6);                   /* magic includes NUL */
    memcpy(h + 263, "00", 2);
    memcpy(h + 345, prefix, strlen(prefix));
    for (i = 0; i < BLOCK; ++i) sum += h[i];
    put_octal((char *)h + 148, 7, sum);
    h[155] = ' ';
    if (fwrite(h, 1, BLOCK, out) != BLOCK) return CB_IO_ERROR;
    return CB_OK;
}

static cb_status copy_file(FILE *out, const char *path, off_t size) {
    FILE *in = fopen(path, "rb");
    unsigned char buf[8192];
    off_t left = size;
    size_t pad;
    unsigned char zeros[BLOCK];

    if (!in) {
        cb_set_error("cannot read %s: %s", path, strerror(errno));
        return CB_IO_ERROR;
    }
    while (left > 0) {
        size_t want = left > (off_t)sizeof buf ? sizeof buf : (size_t)left;
        size_t got = fread(buf, 1, want, in);
        if (got == 0) {
            fclose(in);
            cb_set_error("short read on %s", path);
            return CB_IO_ERROR;
        }
        if (fwrite(buf, 1, got, out) != got) { fclose(in); return CB_IO_ERROR; }
        left -= (off_t)got;
    }
    fclose(in);
    pad = (size_t)((BLOCK - (size % BLOCK)) % BLOCK);
    memset(zeros, 0, sizeof zeros);
    if (pad && fwrite(zeros, 1, pad, out) != pad) return CB_IO_ERROR;
    return CB_OK;
}

cb_status cb_archive_tar(const char *out_path, const char *root, const cb_list *files) {
    FILE *out = fopen(out_path, "wb");
    unsigned char zeros[BLOCK];
    size_t i;
    cb_status st = CB_OK;

    if (!out) {
        cb_set_error("cannot create %s: %s", out_path, strerror(errno));
        return CB_IO_ERROR;
    }
    for (i = 0; i < files->count && st == CB_OK; ++i) {
        const char *rel = files->items[i];
        char *full;
        struct stat sb;
        size_t n = strlen(root) + 1 + strlen(rel) + 1;

        full = malloc(n);
        if (!full) { st = CB_ERROR; break; }
        snprintf(full, n, "%s/%s", root, rel);
        if (stat(full, &sb) != 0 || !S_ISREG(sb.st_mode)) {
            cb_set_error("not a regular file: %s", full);
            st = CB_NOT_FOUND;
        } else {
            st = write_header(out, rel, &sb);
            if (st == CB_OK) st = copy_file(out, full, sb.st_size);
        }
        free(full);
    }
    if (st == CB_OK) {
        memset(zeros, 0, sizeof zeros);
        if (fwrite(zeros, 1, BLOCK, out) != BLOCK || fwrite(zeros, 1, BLOCK, out) != BLOCK)
            st = CB_IO_ERROR;
    }
    if (fclose(out) != 0 && st == CB_OK) st = CB_IO_ERROR;
    if (st != CB_OK) remove(out_path);
    return st;
}
