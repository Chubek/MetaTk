/* All dependency switches intentionally precede every include. */
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_VARINT
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_ARCHIVE
#define MULIB_IMPL_MMFILE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_KVDB
#define MULIB_IMPL_ELF
#define MULIB_IMPL_DRM
#include "mulib-archive.h"
#include "mulib-mmfile.h"
#include "mulib-dynload.h"
#include "mulib-kvdb.h"
#include "mulib-elf.h"
#include "mulib-drm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <dirent.h>
#endif

static char storage_root[128];
static void storage_path(char out[256], const char *name) {
    int n = snprintf(out, 256, "%s/%s", storage_root, name);
    assert(n > 0 && n < 256);
}
static void storage_write(const char *path, const void *p, size_t n) {
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(!n || fwrite(p, 1, n, f) == n);
    assert(fclose(f) == 0);
}
#if defined(__unix__) || defined(__APPLE__)
static unsigned char *storage_read(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    mulib_buffer_t b;
    unsigned char chunk[1024];
    size_t got;
    assert(f);
    mulib_buffer_init(&b);
    while ((got = fread(chunk, 1, sizeof(chunk), f)) != 0) assert(mulib_buffer_append(&b, chunk, got));
    assert(!ferror(f) && fclose(f) == 0);
    *n = b.length;
    return b.data;
}
#endif
static void storage_tar_checksum(unsigned char *p) {
    unsigned sum = 0;
    size_t i;
    memset(p + 148, ' ', 8);
    for (i = 0; i < 512; ++i) sum += p[i];
    assert(snprintf((char *)p + 148, 7, "%06o", sum) == 6);
    p[155] = ' ';
}
static void storage_bad_tar(unsigned char *p, size_t n, int expected) {
    mulib_archive_reader_t r;
    mulib_archive_entry_t e;
    assert(mulib_archive_reader_init(&r, p, n) == 0);
    assert(mulib_archive_next(&r, &e) == expected);
    assert(mulib_archive_next(&r, &e) == MULIB_ARCHIVE_ESTATE);
}
static void test_archive(void) {
    mulib_archive_writer_t w;
    mulib_archive_reader_t r;
    mulib_archive_entry_t e;
    const unsigned char binary[] = { 1, 0, 2, 0, 255 };
    char path[257];
    unsigned char *bad;
    size_t saved, i;
    const char *unsafe[] = { "", "/a", "../a", "a/../b", "a/./b", "a//b", "a\\b", "C:/a", "a/" };
    for (i = 0; i < sizeof(unsafe) / sizeof(*unsafe); ++i) assert(!mulib_archive_safe_path(unsafe[i], 0));
    assert(mulib_archive_safe_path("a/", 1));
    assert(!mulib_archive_safe_path("a//", 1));
    assert(!mulib_archive_safe_path("./", 1));
    mulib_archive_writer_init(&w);
    assert(mulib_archive_add(&w, "file", binary, sizeof(binary), 0640, 123) == 0);
    saved = w.bytes.length;
    assert(mulib_archive_add(&w, "../x", NULL, 0, 0644, 0) == MULIB_ARCHIVE_EUNSAFE);
    assert(mulib_archive_add(&w, "x", NULL, 0, 010000, 0) == MULIB_ARCHIVE_EOVERFLOW);
    assert(mulib_archive_add(&w, "x", NULL, 0, 0600, UINT64_MAX) == MULIB_ARCHIVE_EOVERFLOW);
    memset(path, 'x', 101); path[101] = 0;
    assert(mulib_archive_add(&w, path, NULL, 0, 0600, 0) == MULIB_ARCHIVE_EOVERFLOW);
    assert(w.bytes.length == saved);
    assert(mulib_archive_add_directory(&w, "dir/", 0750, 5) == 0);
    memset(path, 'p', 155); path[155] = '/'; memset(path + 156, 'n', 100); path[256] = 0;
    assert(mulib_archive_add(&w, path, NULL, 0, 0600, 0) == 0);
    assert(mulib_archive_finish(&w) == 0);
    saved = w.bytes.length;
    assert(mulib_archive_finish(&w) == 0 && w.bytes.length == saved);
    assert(mulib_archive_add_directory(&w, "late", 0755, 0) == MULIB_ARCHIVE_ESTATE);
    assert(mulib_archive_reader_init(&r, w.bytes.data, w.bytes.length) == 0);
    assert(mulib_archive_next(&r, &e) == 1 && !strcmp(e.path, "file"));
    assert(e.mode == 0640 && e.mtime == 123 && e.size == sizeof(binary) && e.uid == 0 && e.gid == 0);
    assert(!memcmp(e.data, binary, sizeof(binary)));
    assert(mulib_archive_next(&r, &e) == 1 && e.type == '5' && !strcmp(e.path, "dir/"));
    assert(mulib_archive_next(&r, &e) == 1 && !strcmp(e.path, path) && e.size == 0);
    assert(mulib_archive_next(&r, &e) == 0 && mulib_archive_next(&r, &e) == 0);
    assert(mulib_archive_reader_init(&r, w.bytes.data, 0) == MULIB_ARCHIVE_EFORMAT);
    assert(mulib_archive_reader_init(&r, w.bytes.data, w.bytes.length - 1) == MULIB_ARCHIVE_EFORMAT);
    bad = (unsigned char *)malloc(w.bytes.length);
    assert(bad);
    memcpy(bad, w.bytes.data, w.bytes.length); bad[0] ^= 1;
    storage_bad_tar(bad, w.bytes.length, MULIB_ARCHIVE_EFORMAT);
    for (i = 0; i < 8; ++i) {
        memcpy(bad, w.bytes.data, w.bytes.length); bad[156] = (unsigned char)"12346xgL"[i];
        storage_tar_checksum(bad); storage_bad_tar(bad, w.bytes.length, MULIB_ARCHIVE_EUNSAFE);
    }
    memcpy(bad, w.bytes.data, w.bytes.length); memset(bad, 0, 100); memcpy(bad, "../escape", 9);
    storage_tar_checksum(bad); storage_bad_tar(bad, w.bytes.length, MULIB_ARCHIVE_EUNSAFE);
    memcpy(bad, w.bytes.data, w.bytes.length); bad[124] = 0x80;
    storage_tar_checksum(bad); storage_bad_tar(bad, w.bytes.length, MULIB_ARCHIVE_EFORMAT);
    memcpy(bad, w.bytes.data, w.bytes.length); memset(bad + 124, '7', 11); bad[135] = 0;
    storage_tar_checksum(bad);
    storage_bad_tar(bad, w.bytes.length, (uintmax_t)SIZE_MAX > UINT64_C(8589934591) ?
                    MULIB_ARCHIVE_EFORMAT : MULIB_ARCHIVE_EOVERFLOW);
    memcpy(bad, w.bytes.data, w.bytes.length); bad[156] = '5';
    storage_tar_checksum(bad); storage_bad_tar(bad, w.bytes.length, MULIB_ARCHIVE_EFORMAT);
    memcpy(bad, w.bytes.data, w.bytes.length); bad[w.bytes.length - 1] = 1;
    assert(mulib_archive_reader_init(&r, bad, w.bytes.length) == 0);
    for (i = 0; i < 3; ++i) assert(mulib_archive_next(&r, &e) == 1);
    assert(mulib_archive_next(&r, &e) == MULIB_ARCHIVE_EFORMAT);
    assert(mulib_archive_reader_init(&r, w.bytes.data, w.bytes.length - 512) == 0);
    for (i = 0; i < 3; ++i) assert(mulib_archive_next(&r, &e) == 1);
    assert(mulib_archive_next(&r, &e) == MULIB_ARCHIVE_EFORMAT);
    free(bad); mulib_archive_writer_free(&w);
    mulib_archive_writer_init(&w);
    assert(mulib_archive_finish(&w) == 0 && w.bytes.length == 1024);
    assert(mulib_archive_reader_init(&r, w.bytes.data, w.bytes.length) == 0);
    assert(mulib_archive_next(&r, &e) == 0);
    mulib_archive_writer_free(&w);
}

static void test_mmfile(void) {
    mulib_mmfile_t m = MULIB_MMFILE_INIT;
    char path[256], missing[256];
    unsigned char *p;
    size_t n;
    storage_path(path, "mapped"); storage_path(missing, "missing");
    storage_write(path, "abcd", 4);
#if defined(__unix__) || defined(__APPLE__)
    assert(mulib_mmfile_open(&m, missing, 0) == MULIB_MMFILE_EIO && !m.data && !m.size);
    assert(mulib_mmfile_open(&m, storage_root, 0) == MULIB_MMFILE_EINVAL);
    assert(mulib_mmfile_open(&m, path, 2) == MULIB_MMFILE_EINVAL);
    assert(mulib_mmfile_open(&m, path, 0) == 0 && m.size == 4 && !m.writable);
    assert(!memcmp(m.data, "abcd", 4));
    assert(mulib_mmfile_open(&m, path, 1) == MULIB_MMFILE_ESTATE);
    assert(mulib_mmfile_flush(&m) == 0 && mulib_mmfile_close(&m) == 0);
    assert(mulib_mmfile_open(&m, path, 1) == 0);
    ((unsigned char *)m.data)[0] = 0; ((unsigned char *)m.data)[3] = 255;
    assert(mulib_mmfile_flush(&m) == 0 && mulib_mmfile_close(&m) == 0);
    p = storage_read(path, &n); assert(n == 4 && p[0] == 0 && p[3] == 255); free(p);
    storage_write(path, NULL, 0);
    assert(mulib_mmfile_open(&m, path, 0) == 0 && !m.data && !m.size && m.active);
    assert(mulib_mmfile_open(&m, path, 0) == MULIB_MMFILE_ESTATE);
    assert(mulib_mmfile_flush(&m) == 0 && mulib_mmfile_close(&m) == 0);
    assert(mulib_mmfile_open(&m, path, 1) == 0 && !m.data && !m.size && m.writable);
    assert(mulib_mmfile_close(&m) == 0 && mulib_mmfile_close(&m) == 0);
#else
    (void)p; (void)n;
    assert(mulib_mmfile_open(&m, path, 0) == MULIB_MMFILE_ENOTSUP);
#endif
    assert(remove(path) == 0);
}
static void test_dynload(void) {
    mulib_dynload_t d = MULIB_DYNLOAD_INIT;
    char error[128], tiny[1] = { 'x' };
    void *p = &d;
#if defined(__unix__) || defined(__APPLE__) || defined(_WIN32)
    assert(mulib_dynload_open(&d, "mulib-no-such-library-743918", error, sizeof(error)) == MULIB_DYNLOAD_EIO);
    assert(error[0] && !d.handle);
    assert(mulib_dynload_open(&d, "mulib-no-such-library-743918", tiny, sizeof(tiny)) == MULIB_DYNLOAD_EIO);
    assert(tiny[0] == 0);
#ifdef _WIN32
    assert(mulib_dynload_open(&d, "kernel32.dll", error, sizeof(error)) == 0);
    assert(mulib_dynload_symbol(&d, "GetCurrentProcessId", &p, error, sizeof(error)) == 0 && p);
#else
    assert(mulib_dynload_open(&d, NULL, error, sizeof(error)) == 0);
    assert(mulib_dynload_symbol(&d, "malloc", &p, error, sizeof(error)) == 0 && p);
#endif
    assert(mulib_dynload_open(&d, NULL, NULL, 0) == MULIB_DYNLOAD_ESTATE);
    p = &d;
    assert(mulib_dynload_symbol(&d, "mulib_no_such_symbol_743918", &p, error, sizeof(error)) == MULIB_DYNLOAD_EIO);
    assert(p == &d && error[0]);
    assert(mulib_dynload_close(&d, error, sizeof(error)) == 0 && error[0] == 0 && !d.handle);
    assert(mulib_dynload_close(&d, NULL, 0) == 0);
    assert(mulib_dynload_symbol(&d, "malloc", &p, NULL, 0) == MULIB_DYNLOAD_EINVAL);
#else
    (void)p; (void)tiny;
    assert(mulib_dynload_open(&d, NULL, error, sizeof(error)) == MULIB_DYNLOAD_ENOTSUP);
#endif
}

static void storage_u16(unsigned char *p, uint16_t n, int little) {
    p[little ? 0 : 1] = (unsigned char)n; p[little ? 1 : 0] = (unsigned char)(n >> 8);
}
static void storage_u32(unsigned char *p, uint32_t n, int little) {
    if (little) mulib_endian_store_le32(p, n); else mulib_endian_store_be32(p, n);
}
static void storage_u64(unsigned char *p, uint64_t n, int little) {
    storage_u32(p + (little ? 0 : 4), (uint32_t)n, little);
    storage_u32(p + (little ? 4 : 0), (uint32_t)(n >> 32), little);
}
static unsigned char *storage_sh(unsigned char *p, unsigned bits, size_t i) {
    return p + 128 + i * (bits == 32 ? 40 : 64);
}
static void storage_section(unsigned char *p, unsigned bits, int little, size_t index,
                            uint32_t name, uint32_t type, uint64_t offset, uint64_t size,
                            uint32_t link, uint32_t info, uint64_t stride) {
    unsigned char *s = storage_sh(p, bits, index);
    storage_u32(s, name, little); storage_u32(s + 4, type, little);
    if (bits == 32) {
        storage_u32(s + 16, (uint32_t)offset, little); storage_u32(s + 20, (uint32_t)size, little);
        storage_u32(s + 24, link, little); storage_u32(s + 28, info, little);
        storage_u32(s + 32, 1, little); storage_u32(s + 36, (uint32_t)stride, little);
    } else {
        storage_u64(s + 24, offset, little); storage_u64(s + 32, size, little);
        storage_u32(s + 40, link, little); storage_u32(s + 44, info, little);
        storage_u64(s + 48, 1, little); storage_u64(s + 56, stride, little);
    }
}
static void storage_make_elf(unsigned char *p, unsigned bits, int little, int extended) {
    static const char names[] = "\0.shstrtab\0.strtab\0.symtab\0.bss\0.shndx\0";
    unsigned char *ph = p + 64, *symbol;
    size_t stride = bits == 32 ? 16 : 24;
    memset(p, 0, 1024);
    memcpy(p, "\177ELF", 4); p[4] = bits == 32 ? 1 : 2; p[5] = little ? 1 : 2; p[6] = 1;
    storage_u16(p + 16, 2, little); storage_u16(p + 18, bits == 32 ? 3 : 62, little);
    storage_u32(p + 20, 1, little);
    if (bits == 32) {
        storage_u32(p + 24, 0x1234, little); storage_u32(p + 28, 64, little); storage_u32(p + 32, 128, little);
        storage_u16(p + 40, 52, little); storage_u16(p + 42, 32, little);
        storage_u16(p + 44, extended ? 0xffffu : 1, little); storage_u16(p + 46, 40, little);
        storage_u16(p + 48, extended ? 0 : 6, little); storage_u16(p + 50, extended ? 0xffffu : 1, little);
        storage_u32(ph + 4, 600, little); storage_u32(ph + 8, 0x400000 + 600, little);
        storage_u32(ph + 16, 32, little); storage_u32(ph + 20, 64, little);
        storage_u32(ph + 24, 5, little); storage_u32(ph + 28, 8, little);
    } else {
        storage_u64(p + 24, 0x1234, little); storage_u64(p + 32, 64, little); storage_u64(p + 40, 128, little);
        storage_u16(p + 52, 64, little); storage_u16(p + 54, 56, little);
        storage_u16(p + 56, extended ? 0xffffu : 1, little); storage_u16(p + 58, 64, little);
        storage_u16(p + 60, extended ? 0 : 6, little); storage_u16(p + 62, extended ? 0xffffu : 1, little);
        storage_u32(ph + 4, 5, little); storage_u64(ph + 8, 600, little);
        storage_u64(ph + 16, 0x400000 + 600, little); storage_u64(ph + 32, 32, little);
        storage_u64(ph + 40, 64, little); storage_u64(ph + 48, 8, little);
    }
    storage_u32(ph, 1, little);
    storage_section(p, bits, little, 0, 0, 0, 0, extended ? 6 : 0, extended ? 1 : 0, extended ? 1 : 0, 0);
    storage_section(p, bits, little, 1, 1, 3, 600, sizeof(names), 0, 0, 0);
    storage_section(p, bits, little, 2, 11, 3, 650, 5, 0, 0, 0);
    storage_section(p, bits, little, 3, 19, 2, 700, 2 * stride, 2, 1, stride);
    storage_section(p, bits, little, 4, 27, 8, UINT64_MAX, UINT64_MAX, 0, 0, 0);
    storage_section(p, bits, little, 5, 32, 18, 800, 8, 3, 0, 4);
    memcpy(p + 600, names, sizeof(names)); memcpy(p + 650, "\0foo\0", 5);
    symbol = p + 700 + stride;
    storage_u32(symbol, 1, little);
    if (bits == 32) {
        storage_u32(symbol + 4, 0x55667788u, little); storage_u32(symbol + 8, 7, little);
        symbol[12] = 0x11; symbol[13] = 2; storage_u16(symbol + 14, extended ? 0xffffu : 4, little);
    } else {
        symbol[4] = 0x11; symbol[5] = 2; storage_u16(symbol + 6, extended ? 0xffffu : 4, little);
        storage_u64(symbol + 8, UINT64_C(0x1122334455667788), little); storage_u64(symbol + 16, 7, little);
    }
    storage_u32(p + 804, 4, little);
}
static void test_elf(void) {
    unsigned char raw[1025], *p = raw + 1;
    mulib_elf_t v;
    mulib_elf_section_t s;
    mulib_elf_program_t ph;
    mulib_elf_symbol_t symbol;
    const unsigned char *bytes;
    const char *text;
    size_t n, count;
    unsigned bits;
    int little, extended;
    for (bits = 32; bits <= 64; bits += 32) for (little = 0; little <= 1; ++little)
        for (extended = 0; extended <= 1; ++extended) {
            storage_make_elf(p, bits, little, extended);
            assert(mulib_elf_open(&v, p, 1024) == 0);
            assert(v.bits == bits && v.little_endian == little && v.shnum == 6 && v.phnum == 1 && v.shstrndx == 1);
            assert(mulib_elf_section_name(&v, 3, &text, &n) == 0 && n == 7 && !memcmp(text, ".symtab", n));
            assert(mulib_elf_program(&v, 0, &ph) == 0 && ph.filesz == 32 && ph.memsz == 64 && ph.flags == 5);
            assert(mulib_elf_section(&v, 4, &s) == 0 && s.type == 8);
            assert(mulib_elf_symbol_count(&v, 3, &count) == 0 && count == 2);
            assert(mulib_elf_symbol(&v, 3, 1, &symbol) == 0 && symbol.section_index == 4 && symbol.size == 7);
            assert(symbol.value == (bits == 32 ? UINT64_C(0x55667788) : UINT64_C(0x1122334455667788)));
            assert(mulib_elf_symbol_name(&v, 3, 1, &text, &n) == 0 && n == 3 && !memcmp(text, "foo", 3));
            assert(mulib_elf_symbol(&v, 3, 2, &symbol) == MULIB_ELF_EBOUNDS);
            assert(mulib_elf_bytes(&v, 1024, 0, &bytes) == 0 && bytes == p + 1024);
            assert(mulib_elf_bytes(&v, UINT64_MAX, 1, &bytes) == MULIB_ELF_EBOUNDS);
            for (n = 0; n < 808; ++n) assert(mulib_elf_open(&v, p, n) != 0);
            assert(mulib_elf_open(&v, p, 1024) == 0);
            p[654] = 'x';
            assert(mulib_elf_open(&v, p, 1024) == 0);
            assert(mulib_elf_symbol_name(&v, 3, 1, &text, &n) == MULIB_ELF_EFORMAT);
            storage_make_elf(p, bits, little, extended);
            storage_u32(storage_sh(p, bits, 3) + (bits == 32 ? 24 : 40), 99, little);
            assert(mulib_elf_open(&v, p, 1024) != 0);
            storage_make_elf(p, bits, little, extended);
            memset(storage_sh(p, bits, 3) + (bits == 32 ? 36 : 56), 0, bits == 32 ? 4 : 8);
            assert(mulib_elf_open(&v, p, 1024) == MULIB_ELF_EFORMAT);
            storage_make_elf(p, bits, little, extended);
            storage_u32(p + 64 + (bits == 32 ? 28 : 48), 3, little);
            assert(mulib_elf_open(&v, p, 1024) == MULIB_ELF_EFORMAT);
            storage_make_elf(p, bits, little, extended);
            storage_u32(storage_sh(p, bits, 1) + (bits == 32 ? 16 : 24), 2000, little);
            assert(mulib_elf_open(&v, p, 1024) == MULIB_ELF_EBOUNDS);
            storage_make_elf(p, bits, little, extended);
            storage_u16(p + (bits == 32 ? 46 : 58), 1, little);
            assert(mulib_elf_open(&v, p, 1024) == MULIB_ELF_EBOUNDS);
            storage_make_elf(p, bits, little, 1);
            storage_u32(storage_sh(p, bits, 5) + 4, 0, little);
            assert(mulib_elf_open(&v, p, 1024) == 0);
            assert(mulib_elf_symbol(&v, 3, 1, &symbol) == MULIB_ELF_EFORMAT);
        }
#ifdef __linux__
    {
        mulib_mmfile_t m = MULIB_MMFILE_INIT;
        size_t i;
        assert(mulib_mmfile_open(&m, "/proc/self/exe", 0) == 0);
        assert(mulib_elf_open(&v, m.data, m.size) == 0 && v.phnum > 0 && v.shnum > 0);
        for (i = 0; i < v.shnum; ++i) {
            assert(mulib_elf_section(&v, i, &s) == 0);
            assert(mulib_elf_section_name(&v, i, &text, &n) == 0);
            if (s.type == 2 || s.type == 11) {
                size_t j;
                assert(mulib_elf_symbol_count(&v, i, &count) == 0);
                for (j = 0; j < count; ++j) assert(mulib_elf_symbol_name(&v, i, j, &text, &n) == 0);
            }
        }
        assert(mulib_mmfile_close(&m) == 0);
    }
#endif
}

#if defined(__unix__) || defined(__APPLE__)
static void storage_kv_crc(unsigned char *p, size_t n) {
    assert(n >= 4); mulib_endian_store_le32(p + n - 4, mulib_crc32(p, n - 4));
}
static void storage_bad_kv(const char *path, const void *p, size_t n) {
    mulib_kvdb_t *db = NULL;
    storage_write(path, p, n);
    assert(mulib_kvdb_open(path, 0, &db) == MULIB_KVDB_ECORRUPT && db == NULL);
}
#endif
static void test_kvdb(void) {
#if defined(__unix__) || defined(__APPLE__)
    char path[256], corrupt[256], collision[384], blocked[256];
    mulib_kvdb_t *db = NULL, *other = NULL;
    const unsigned char binary_key[] = { 0, 255, 1 }, binary_value[] = { 1, 0, 255, 4 };
    const void *value, *key;
    size_t n, kn, size, i;
    unsigned char *snapshot, *bad;
    int result;
    storage_path(path, "database"); storage_path(corrupt, "corrupt"); storage_path(blocked, "blocked");
    assert(mulib_kvdb_open(path, 0, &db) == MULIB_KVDB_ENOTFOUND && !db);
    assert(mulib_kvdb_open(path, 1, &db) == 0 && mulib_kvdb_dirty(db));
    assert(mulib_kvdb_set(db, NULL, 0, NULL, 0) == 0);
    assert(mulib_kvdb_set(db, binary_key, sizeof(binary_key), binary_value, sizeof(binary_value)) == 0);
    assert(mulib_kvdb_set(db, "a", 1, "old", 3) == 0);
    assert(mulib_kvdb_set(db, "a\0", 2, "new", 3) == 0);
    assert(mulib_kvdb_get(db, binary_key, sizeof(binary_key), &value, &size) == 0);
    assert(size == sizeof(binary_value) && !memcmp(value, binary_value, size));
    assert(mulib_kvdb_set(db, binary_key, sizeof(binary_key), value, size) == 0);
    assert(mulib_kvdb_size(db) == 4);
    assert(mulib_kvdb_at(db, 0, &key, &kn, &value, &size) == 0 && kn == 0 && size == 0);
    assert(mulib_kvdb_at(db, 1, &key, &kn, &value, &size) == 0 && kn == sizeof(binary_key));
    assert(!memcmp(key, binary_key, kn));
    assert(mulib_kvdb_at(db, 2, &key, &kn, &value, &size) == 0 && kn == 1 && !memcmp(key, "a", kn));
    assert(mulib_kvdb_remove(db, "missing", 7) == MULIB_KVDB_ENOTFOUND);
    result = mulib_kvdb_commit(db); assert(result == 0 || result == MULIB_KVDB_UNCERTAIN);
    assert(!mulib_kvdb_dirty(db) && mulib_kvdb_commit(db) == 0);
    assert(mulib_kvdb_open(path, 0, &other) == 0 && mulib_kvdb_size(other) == 4);
    mulib_kvdb_close(other); other = NULL;
    snapshot = storage_read(path, &n); assert(n > 13);
    assert(mulib_kvdb_set(db, "a", 1, "not committed", 13) == 0);
    mulib_kvdb_close(db); db = NULL;
    assert(mulib_kvdb_open(path, 0, &db) == 0);
    assert(mulib_kvdb_get(db, "a", 1, &value, &size) == 0 && size == 3 && !memcmp(value, "old", 3));
    assert(mulib_kvdb_remove(db, NULL, 0) == 0 && mulib_kvdb_size(db) == 3);
    /* All predictable exclusive temp names already exist: original must survive. */
    for (i = 0; i < 128; ++i) {
        FILE *file;
        int used = snprintf(collision, sizeof(collision), "%s.mulib-%lu-%llu", path,
                            (unsigned long)getpid(), (unsigned long long)i);
        assert(used > 0 && (size_t)used < sizeof(collision));
        file = fopen(collision, "wbx"); assert(file && fputc('x', file) == 'x' && fclose(file) == 0);
    }
    assert(mulib_kvdb_commit(db) == MULIB_KVDB_EIO && mulib_kvdb_dirty(db));
    bad = storage_read(path, &size); assert(size == n && !memcmp(bad, snapshot, n)); free(bad);
    for (i = 0; i < 128; ++i) {
        (void)snprintf(collision, sizeof(collision), "%s.mulib-%lu-%llu", path,
                       (unsigned long)getpid(), (unsigned long long)i);
        assert(remove(collision) == 0);
    }
    result = mulib_kvdb_commit(db); assert(result == 0 || result == MULIB_KVDB_UNCERTAIN);
    mulib_kvdb_close(db); db = NULL;
    assert(mulib_kvdb_open(path, 0, &db) == 0 && mulib_kvdb_size(db) == 3);
    assert(mulib_kvdb_get(db, NULL, 0, &value, &size) == MULIB_KVDB_ENOTFOUND);
    mulib_kvdb_close(db); db = NULL;
    bad = (unsigned char *)malloc(n + 1); assert(bad);
    for (i = 0; i < n; ++i) {
        memcpy(bad, snapshot, n); bad[i] ^= 1; storage_bad_kv(corrupt, bad, n);
        storage_bad_kv(corrupt, snapshot, i);
    }
    memcpy(bad, snapshot, n); bad[n] = 0; storage_bad_kv(corrupt, bad, n + 1);
    /* Count zero plus trailing payload, with a correct CRC, is still corrupt. */
    memcpy(bad, snapshot, n); bad[8] = 0; storage_kv_crc(bad, n); storage_bad_kv(corrupt, bad, n);
    {
        unsigned char nonminimal[] = { 'M','u','K','V','D','B',1,0, 0x80,0, 0,0,0,0 };
        unsigned char duplicate[] = { 'M','u','K','V','D','B',1,0, 2, 1,0,'a', 1,0,'a', 0,0,0,0 };
        unsigned char overflow[] = { 'M','u','K','V','D','B',1,0, 1, 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,2,0, 0,0,0,0 };
        storage_kv_crc(nonminimal, sizeof(nonminimal)); storage_bad_kv(corrupt, nonminimal, sizeof(nonminimal));
        storage_kv_crc(duplicate, sizeof(duplicate)); storage_bad_kv(corrupt, duplicate, sizeof(duplicate));
        duplicate[14] = 'A'; storage_kv_crc(duplicate, sizeof(duplicate)); storage_bad_kv(corrupt, duplicate, sizeof(duplicate));
        storage_kv_crc(overflow, sizeof(overflow)); storage_bad_kv(corrupt, overflow, sizeof(overflow));
    }
    assert(mulib_kvdb_open(blocked, 1, &db) == 0);
    assert(mkdir(blocked, 0700) == 0);
    assert(mulib_kvdb_commit(db) == MULIB_KVDB_EIO && mulib_kvdb_dirty(db));
    mulib_kvdb_close(db);
    assert(rmdir(blocked) == 0);
    free(bad); free(snapshot);
    assert(remove(path) == 0 && remove(corrupt) == 0);
#else
    assert(mulib_kvdb_commit(NULL) == MULIB_KVDB_EINVAL);
#endif
}

#ifdef __linux__
static size_t storage_fd_count(void) {
    DIR *directory = opendir("/proc/self/fd");
    struct dirent *entry;
    size_t count = 0;
    assert(directory);
    while ((entry = readdir(directory)) != NULL)
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) ++count;
    assert(closedir(directory) == 0);
    return count;
}
#endif
static void test_drm(void) {
    mulib_drm_device_t d = MULIB_DRM_DEVICE_INIT;
    mulib_drm_dumb_t b = MULIB_DRM_DUMB_INIT;
    _Static_assert(sizeof(mulib_drm_cap_request_t) == 16, "DRM cap ABI");
    _Static_assert(sizeof(mulib_drm_create_request_t) == 32, "DRM create ABI");
    _Static_assert(sizeof(mulib_drm_map_request_t) == 16, "DRM map ABI");
    _Static_assert(sizeof(mulib_drm_destroy_request_t) == 4, "DRM destroy ABI");
    _Static_assert(offsetof(mulib_drm_create_request_t, size) == 24, "DRM size offset");
    _Static_assert(offsetof(mulib_drm_map_request_t, offset) == 8, "DRM mmap offset");
    _Static_assert(offsetof(mulib_drm_version_request_t, name_len) == (sizeof(void *) == 8 ? 16u : 12u), "DRM native version offset");
#ifdef __linux__
    {
        mulib_drm_version_t v, saved;
        uint64_t cap = 777;
        size_t descriptors, i;
        int fd;
        void *data;
        assert(sizeof(mulib_drm_version_request_t) == (sizeof(void *) == 8 ? 64u : 36u));
        assert(mulib_drm_ioctl_request(MULIB_DRM_VERSION) == _IOWR('d', 0, mulib_drm_version_request_t));
        assert(mulib_drm_ioctl_request(MULIB_DRM_GET_CAP) == _IOWR('d', 0x0c, mulib_drm_cap_request_t));
        assert(mulib_drm_ioctl_request(MULIB_DRM_CREATE_DUMB) == _IOWR('d', 0xb2, mulib_drm_create_request_t));
        assert(mulib_drm_ioctl_request(MULIB_DRM_MAP_DUMB) == _IOWR('d', 0xb3, mulib_drm_map_request_t));
        assert(mulib_drm_ioctl_request(MULIB_DRM_DESTROY_DUMB) == _IOWR('d', 0xb4, mulib_drm_destroy_request_t));
        assert(mulib_drm_ioctl_request((enum mulib_drm_operation)99) == 0);
        assert(mulib_drm_open(&d, "/mulib-no-such-device-743918") == MULIB_DRM_EIO && d.fd == -1);
        assert(mulib_drm_open(&d, "/dev/null") == 0);
        assert(mulib_drm_open(&d, "/dev/null") == MULIB_DRM_ESTATE);
        memset(&v, 0x5a, sizeof(v)); saved = v;
        assert(mulib_drm_get_version(&d, &v) == MULIB_DRM_EIO && errno == ENOTTY);
        assert(!memcmp(&v, &saved, sizeof(v)));
        assert(mulib_drm_get_cap(&d, MULIB_DRM_CAP_DUMB_BUFFER, &cap) == MULIB_DRM_EIO && cap == 777);
        assert(mulib_drm_dumb_create(&d, 0, 1, 32, &b) == MULIB_DRM_EINVAL);
        assert(mulib_drm_dumb_create(&d, 1, 1, 65, &b) == MULIB_DRM_EINVAL);
        assert(mulib_drm_dumb_create(&d, UINT32_MAX, 1, 64, &b) == MULIB_DRM_EOVERFLOW);
        descriptors = storage_fd_count();
        for (i = 0; i < 16; ++i)
            assert(mulib_drm_dumb_create(&d, 64, 64, 32, &b) == MULIB_DRM_EIO && errno == ENOTTY);
        assert(storage_fd_count() == descriptors);
        assert(b.fd == -1 && !b.data && !b.handle);
        b.fd = dup(d.fd); assert(b.fd >= 0); b.handle = 7; b.size = 4096;
        assert(mulib_drm_dumb_map(&b) == MULIB_DRM_EIO && !b.data);
        assert(mulib_drm_dumb_destroy(&b) == MULIB_DRM_EIO && b.fd >= 0 && b.handle == 7);
        assert(close(b.fd) == 0); b.fd = -1; b.handle = 0; b.size = 0;
        assert(mulib_drm_close(&d) == 0 && d.fd == -1);
        assert(mulib_drm_close(&d) == 0);
        fd = open("/dev/zero", O_RDWR); assert(fd >= 0);
        data = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
        assert(data != MAP_FAILED && close(fd) == 0);
        b.data = data; b.mapped_size = 4096;
        assert(mulib_drm_dumb_unmap(&b) == 0 && !b.data && !b.mapped_size);
        assert(mulib_drm_dumb_destroy(&b) == 0 && mulib_drm_dumb_destroy(&b) == 0);
    }
#else
    assert(mulib_drm_open(&d, "unused") == MULIB_DRM_ENOTSUP);
    assert(mulib_drm_dumb_destroy(&b) == MULIB_DRM_ENOTSUP);
#endif
}

int main(void) {
#if defined(__unix__) || defined(__APPLE__)
    int n = snprintf(storage_root, sizeof(storage_root), "mulib-storage-%lu", (unsigned long)getpid());
    assert(n > 0 && (size_t)n < sizeof(storage_root));
    assert(mkdir(storage_root, 0700) == 0);
#else
    (void)snprintf(storage_root, sizeof(storage_root), ".");
#endif
    test_archive(); test_mmfile(); test_dynload(); test_elf(); test_kvdb(); test_drm();
#if defined(__unix__) || defined(__APPLE__)
    assert(rmdir(storage_root) == 0);
#endif
    puts("storage tests passed");
    return 0;
}
