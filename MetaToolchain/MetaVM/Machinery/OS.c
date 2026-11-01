#include "Internal.h"
#include <stdio.h>
#include <errno.h>

mvm_status mvm_read_file(const char *path, size_t limit, mvm_buffer *out, mvm_error *e) {
    FILE *f;
    mvm_buffer b = {0};
    unsigned char block[4096];
    size_t n;
    mvm_status s = MVM_OK;
    mvm_error_clear(e);
    if (!path || !out) return mvm_fail(e, MVM_INVALID, "invalid file read arguments");
    if (!limit) limit = mvm_default_limits().max_module_bytes;
    f = fopen(path, "rb");
    if (!f) return mvm_fail(e, MVM_IO, "cannot open %s", path);
    while ((n = fread(block, 1, sizeof(block), f)) != 0) {
        if (n > limit - b.size) { s = MVM_LIMIT; break; }
        s = mvm_buffer_append(&b, block, n);
        if (s != MVM_OK) break;
        if (n < sizeof(block)) break; /* EOF or error: do not read again. */
    }
    if (ferror(f)) s = MVM_IO;
    if (fclose(f) != 0 && s == MVM_OK) s = MVM_IO;
    if (s == MVM_OK) s = mvm_buffer_append(out, b.data, b.size);
    mvm_buffer_dispose(&b);
    if (s != MVM_OK) return mvm_fail(e, s, "read %s: %s", path, mvm_status_string(s));
    return MVM_OK;
}
mvm_status mvm_write_file(const char *path, const void *data, size_t n, mvm_error *e) {
    FILE *f;
    mvm_status s = MVM_OK;
    mvm_error_clear(e);
    if (!path || (!data && n)) return mvm_fail(e, MVM_INVALID, "invalid file write arguments");
    f = fopen(path, "wb");
    if (!f) return mvm_fail(e, MVM_IO, "cannot open %s", path);
    if (n && fwrite(data, 1, n, f) != n) s = MVM_IO;
    if (fclose(f) != 0) s = MVM_IO;
    if (s != MVM_OK) return mvm_fail(e, s, "write failed: %s", path);
    return MVM_OK;
}
mvm_status mvm_path_join(const char *base, const char *leaf, char **out) {
    size_t a, b, sep;
    char *p;
    if (!out) return MVM_INVALID;
    *out = NULL;
    if (!base || !leaf) return MVM_INVALID;
    if (*leaf == '/') base = "";
    a = strlen(base); b = strlen(leaf);
    sep = a && b && base[a - 1] != '/';
    if (a == SIZE_MAX || b > SIZE_MAX - a - 1 || sep > SIZE_MAX - a - b - 1) return MVM_LIMIT;
    p = malloc(a + b + sep + 1);
    if (!p) return MVM_OOM;
    memcpy(p, base, a);
    if (sep) p[a] = '/';
    memcpy(p + a + sep, leaf, b + 1);
    *out = p;
    return MVM_OK;
}
mvm_status mvm_load_file(const char *path, size_t limit, mvm_module **out, mvm_error *e) {
    mvm_buffer b = {0};
    mvm_status s;
    if (!out) return mvm_fail(e, MVM_INVALID, "NULL module output");
    *out = NULL;
    s = mvm_read_file(path, limit, &b, e);
    if (s == MVM_OK) {
        if (b.size >= 4 && !memcmp(b.data, "MVM\0", 4)) s = mvm_deserialize(b.data, b.size, limit, out, e);
        else s = mvm_compile((const char *)b.data, b.size, limit, out, e);
    }
    mvm_buffer_dispose(&b);
    return s;
}
mvm_status mvm_save_file(const char *path, const mvm_module *m, mvm_error *e) {
    mvm_buffer b = {0};
    mvm_status s = mvm_serialize(m, &b, e);
    if (s == MVM_OK) s = mvm_write_file(path, b.data, b.size, e);
    mvm_buffer_dispose(&b);
    return s;
}

mvm_status mvm_write_file_atomic(const char *path, const void *data, size_t n, mvm_error *e) {
    char *temporary;
    size_t length;
    unsigned attempt;
    FILE *stream = NULL;
    mvm_status status = MVM_OK;
    mvm_error_clear(e);
    if (!path || !*path || (!data && n)) return mvm_fail(e, MVM_INVALID, "invalid atomic write arguments");
    length = strlen(path);
    if (length > SIZE_MAX - 40) return mvm_fail(e, MVM_LIMIT, "output path too long");
    temporary = malloc(length + 40);
    if (!temporary) return mvm_fail(e, MVM_OOM, "temporary path allocation failed");
    for (attempt = 0; attempt < 1024; ++attempt) {
        snprintf(temporary, length + 40, "%s.metavm-tmp-%u", path, attempt);
        stream = fopen(temporary, "wbx");
        if (stream) break;
#ifdef EEXIST
        if (errno != EEXIST) break;
#endif
    }
    if (!stream) { free(temporary); return mvm_fail(e, MVM_IO, "cannot stage output: %s", path); }
    if (n && fwrite(data, 1, n, stream) != n) status = MVM_IO;
    if (fclose(stream) != 0) status = MVM_IO;
    if (status == MVM_OK && rename(temporary, path) != 0) status = MVM_IO;
    if (status != MVM_OK) (void)remove(temporary);
    free(temporary);
    return status == MVM_OK ? status : mvm_fail(e, status, "atomic output failed: %s", path);
}
mvm_status mvm_save_file_atomic(const char *path, const mvm_module *m, mvm_error *e) {
    mvm_buffer buffer = {0};
    mvm_status status = mvm_serialize(m, &buffer, e);
    if (status == MVM_OK) status = mvm_write_file_atomic(path, buffer.data, buffer.size, e);
    mvm_buffer_dispose(&buffer);
    return status;
}
