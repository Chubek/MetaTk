/* Focused filesystem tests; no kernel mounts, privileged writes or dependencies. */
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#if defined(__linux__)
#include <unistd.h>
#endif
static size_t vf_calls, vf_fail, vf_live;
typedef union vf_header { max_align_t alignment; size_t size; } vf_header;
static void *vf_malloc(size_t n) {
    vf_header *p;
    if (++vf_calls == vf_fail) return NULL;
    if (n > SIZE_MAX - sizeof(*p)) return NULL;
    p = (vf_header *)malloc(sizeof(*p) + n);
    if (!p) return NULL;
    p->size = n; ++vf_live;
    return p + 1;
}
static void *vf_calloc(size_t n, size_t stride) {
    void *p;
    if (stride && n > SIZE_MAX / stride) return NULL;
    p = vf_malloc(n * stride);
    if (p) memset(p, 0, n * stride);
    return p;
}
static void *vf_realloc(void *p, size_t n) {
    vf_header *next, *old = p ? (vf_header *)p - 1 : NULL;
    if (++vf_calls == vf_fail) return NULL;
    if (n > SIZE_MAX - sizeof(*next)) return NULL;
    next = (vf_header *)malloc(sizeof(*next) + n);
    if (!next) return NULL;
    next->size = n;
    if (old) { memcpy(next + 1, p, n < old->size ? n : old->size); free(old); }
    else ++vf_live;
    return next + 1;
}
static void vf_free(void *p) { if (p) { assert(vf_live); --vf_live; free((vf_header *)p - 1); } }
#define malloc vf_malloc
#define calloc vf_calloc
#define realloc vf_realloc
#define free vf_free
#define MULIB_IMPL_VFS
#define MULIB_IMPL_FUSE
#define MULIB_IMPL_PROCFS
#define MULIB_IMPL_SYSFS
#define MULIB_IMPL_PATHDB
#include "mulib-vfs.h"
#include "mulib-fuse.h"
#include "mulib-procfs.h"
#include "mulib-sysfs.h"
#include "mulib-pathdb.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

static int count_child(void *user, const char *name, const mulib_vfs_stat *info) {
    size_t *n = (size_t *)user;
    assert(name && *name && !strchr(name, '/'));
    assert(info->kind == MULIB_VFS_FILE || info->kind == MULIB_VFS_DIRECTORY);
    ++*n; return 0;
}
static int stop_child(void *user, const char *name, const mulib_vfs_stat *info) {
    (void)count_child(user, name, info); return 1;
}
static int count_entry(void *user, const mulib_pathdb_entry *entry) {
    size_t *n = (size_t *)user;
    assert(entry->path[0] == '/'); ++*n; return 0;
}
static int stop_entry(void *user, const mulib_pathdb_entry *entry) {
    (void)count_entry(user, entry); return 1;
}
static void normalization(void) {
    char out[MULIB_VFS_PATH_CAP], long_path[MULIB_VFS_PATH_CAP + 1];
    assert(mulib_vfs_normalize("//a/./b/../c///", out, sizeof(out)) == 0 && !strcmp(out, "/a/c"));
    assert(mulib_vfs_normalize(out, out, sizeof(out)) == 0 && !strcmp(out, "/a/c"));
    assert(mulib_vfs_normalize("/a/..", out, 2) == 0 && !strcmp(out, "/"));
    strcpy(out, "unchanged");
    assert(mulib_vfs_normalize("/a", out, 2) == MULIB_VFS_ELIMIT && !strcmp(out, "unchanged"));
    assert(mulib_vfs_normalize("", out, sizeof(out)) == MULIB_VFS_EINVAL);
    assert(mulib_vfs_normalize("relative", out, sizeof(out)) == MULIB_VFS_EINVAL);
    assert(mulib_vfs_normalize("/../x", out, sizeof(out)) == MULIB_VFS_EINVAL);
    assert(mulib_vfs_normalize("/a/../../x", out, sizeof(out)) == MULIB_VFS_EINVAL);
    assert(mulib_vfs_normalize("/a\\b", out, sizeof(out)) == MULIB_VFS_EINVAL);
    assert(mulib_vfs_normalize("/a\nb", out, sizeof(out)) == MULIB_VFS_EINVAL);
    memset(long_path, 'a', sizeof(long_path)); long_path[0] = '/';
    long_path[MULIB_VFS_PATH_CAP - 1] = 0;
    assert(mulib_vfs_normalize(long_path, out, sizeof(out)) == 0);
    long_path[MULIB_VFS_PATH_CAP - 1] = 'a'; long_path[MULIB_VFS_PATH_CAP] = 0;
    assert(mulib_vfs_normalize(long_path, out, sizeof(out)) == MULIB_VFS_ELIMIT);
}
static mulib_vfs_result bad_read(void *ctx, const char *path, size_t off, void *data, size_t cap, size_t *n) {
    (void)ctx; (void)path; (void)off; (void)data; *n = cap + 1; return MULIB_VFS_OK;
}
static void routing_and_dispatch(void) {
    mulib_vfs vfs;
    mulib_vfs_mem root, nested;
    mulib_vfs_ops ops = mulib_vfs_mem_ops(), bad = {bad_read, NULL, NULL, NULL};
    mulib_vfs_stat info = {MULIB_VFS_FILE, 99};
    mulib_fuse adapter;
    mulib_fuse_request request = {7, MULIB_FUSE_GETATTR, "/data/file", 0, 0, NULL};
    mulib_fuse_response response;
    char bytes[64], source[] = "nested";
    size_t n, children = 0;
    assert(mulib_vfs_init(&vfs, 3) == 0);
    assert(mulib_vfs_mem_init(&root, 8, 64) == 0);
    assert(mulib_vfs_mem_init(&nested, 8, 64) == 0);
    assert(mulib_vfs_mem_put(&root, "/database", "root", 4) == 0);
    assert(mulib_vfs_mem_put(&root, "/data", "hidden by mount", 15) == 0);
    assert(mulib_vfs_mem_put(&nested, "/file", source, 6) == 0); source[0] = 'X';
    assert(mulib_vfs_mount(&vfs, "/", &ops, &root) == 0);
    assert(mulib_vfs_mount(&vfs, "/data/.", &vfs.mounts[0].ops, &nested) == 0);
    assert(mulib_vfs_mount(&vfs, "/data", &ops, &nested) == MULIB_VFS_EEXIST);
    assert(mulib_vfs_read(&vfs, "/data//file", 0, bytes, sizeof(bytes), &n) == 0 && n == 6 && !memcmp(bytes, "nested", 6));
    assert(mulib_vfs_read(&vfs, "/database", 0, bytes, sizeof(bytes), &n) == 0 && n == 4 && !memcmp(bytes, "root", 4));
    assert(mulib_vfs_read(&vfs, "/data/file", 99, NULL, 0, &n) == 0 && !n);
    assert(mulib_vfs_read(&vfs, "/data", 0, bytes, sizeof(bytes), &n) == MULIB_VFS_EISDIR);
    assert(mulib_vfs_getstat(&vfs, "/missing", &info) == MULIB_VFS_ENOENT && info.size == 99);
    assert(mulib_vfs_mem_put(&nested, "/missing/child", "", 0) == MULIB_VFS_ENOENT);
    assert(mulib_vfs_mem_mkdir(&nested, "/dir") == 0);
    assert(mulib_vfs_mem_mkdir(&nested, "/dir") == MULIB_VFS_EEXIST);
    assert(mulib_vfs_mem_put(&nested, "/dir/empty", NULL, 0) == 0);
    assert(mulib_vfs_mem_put(&nested, "/file/child", NULL, 0) == MULIB_VFS_ENOTDIR);
    assert(mulib_vfs_list(&vfs, "/data", count_child, &children) == 0 && children == 2);
    assert(mulib_vfs_list(&vfs, "/data/file", count_child, &children) == MULIB_VFS_ENOTDIR);
    assert(mulib_vfs_mem_remove(&nested, "/dir") == MULIB_VFS_EEXIST);
    assert(mulib_vfs_mem_remove(&nested, "/dir/empty") == 0);
    assert(mulib_vfs_mem_remove(&nested, "/dir") == 0);
    assert(mulib_vfs_mem_remove(&nested, "/") == MULIB_VFS_EINVAL);
    assert(mulib_vfs_write(&vfs, "/data/sparse", 3, "A\0B", 3, &n) == 0 && n == 3);
    memset(bytes, 1, sizeof(bytes));
    assert(mulib_vfs_read(&vfs, "/data/sparse", 0, bytes, sizeof(bytes), &n) == 0 && n == 6 && !memcmp(bytes, "\0\0\0A\0B", 6));
    assert(mulib_vfs_write(&vfs, "/data/sparse", 64, "x", 1, &n) == MULIB_VFS_ELIMIT && !n);
    assert(mulib_vfs_write(&vfs, "/data/sparse", SIZE_MAX, "x", 1, &n) == MULIB_VFS_EINVAL);
    assert(mulib_vfs_mount(&vfs, "/bad", &bad, NULL) == 0);
    assert(mulib_vfs_mount(&vfs, "/fourth", &bad, NULL) == MULIB_VFS_ELIMIT);
    assert(mulib_vfs_read(&vfs, "/bad", 0, bytes, 1, &n) == MULIB_VFS_EIO && !n);
    assert(mulib_vfs_write(&vfs, "/bad", 0, "x", 1, &n) == MULIB_VFS_EROFS);
    assert(mulib_fuse_init(&adapter, &vfs, 0) == 0);
    assert(mulib_fuse_dispatch(&adapter, &request, NULL, 0, NULL, NULL, &response) == 0 && response.id == 7 && response.info.size == 6);
    request.opcode = MULIB_FUSE_READ; request.length = 6;
    assert(mulib_fuse_dispatch(&adapter, &request, bytes, 5, NULL, NULL, &response) == MULIB_VFS_ELIMIT && !response.transferred);
    assert(mulib_fuse_dispatch(&adapter, &request, bytes, sizeof(bytes), NULL, NULL, &response) == 0 && response.transferred == 6);
    request.opcode = MULIB_FUSE_WRITE; request.data = "N"; request.length = 1;
    assert(mulib_fuse_dispatch(&adapter, &request, NULL, 0, NULL, NULL, &response) == 0 && response.transferred == 1);
    adapter.read_only = 1;
    assert(mulib_fuse_dispatch(&adapter, &request, NULL, 0, NULL, NULL, &response) == MULIB_VFS_EROFS);
    request.opcode = MULIB_FUSE_READDIR; request.path = "/data"; children = 0;
    assert(mulib_fuse_dispatch(&adapter, &request, NULL, 0, stop_child, &children, &response) == MULIB_VFS_ECANCELED && children == 1 && response.transferred == 1);
    request.opcode = (mulib_fuse_opcode)100;
    assert(mulib_fuse_dispatch(&adapter, &request, NULL, 0, NULL, NULL, &response) == MULIB_VFS_ENOTSUP);
    request.opcode = MULIB_FUSE_READ; request.path = "/../file";
    assert(mulib_fuse_dispatch(&adapter, &request, bytes, sizeof(bytes), NULL, NULL, &response) == MULIB_VFS_EINVAL);
    assert(mulib_fuse_dispatch(&adapter, NULL, NULL, 0, NULL, NULL, &response) == MULIB_VFS_EINVAL && !response.id);
    {
        mulib_pathdb snapshot;
        assert(mulib_pathdb_init(&snapshot, 8) == 0 && mulib_pathdb_scan(&snapshot, &vfs, "/") == 0);
        assert(snapshot.count == 5 && !strcmp(snapshot.entries[1].path, "/data") && snapshot.entries[1].info.kind == MULIB_VFS_DIRECTORY);
        assert(!strcmp(snapshot.entries[2].path, "/data/file"));
        mulib_pathdb_free(&snapshot);
    }
    assert(mulib_vfs_unmount(&vfs, "/data/.") == 0);
    assert(mulib_vfs_read(&vfs, "/data/file", 0, bytes, sizeof(bytes), &n) == MULIB_VFS_ENOENT);
    assert(mulib_vfs_unmount(&vfs, "/data") == MULIB_VFS_ENOENT);
    mulib_vfs_free(&vfs);
    assert(mulib_vfs_mem_put(&nested, "/still-alive", NULL, 0) == 0);
    mulib_vfs_mem_free(&root); mulib_vfs_mem_free(&nested); assert(!vf_live);
}
static size_t stat_fixture(char *out, size_t capacity, const char *comm, size_t replace_field, const char *replacement) {
    int n = snprintf(out, capacity, "123 (%s) S", comm);
    size_t used, field;
    assert(n > 0 && (size_t)n < capacity); used = (size_t)n;
    for (field = 4; field <= 24; ++field) {
        if (field == replace_field) n = snprintf(out + used, capacity - used, " %s", replacement);
        else n = snprintf(out + used, capacity - used, " %zu", field - 3);
        assert(n > 0 && (size_t)n < capacity - used); used += (size_t)n;
    }
    return used;
}
static const char process_status[] = "Name:\todd (name) tail)\nPid:\t123\nPPid:\t1\nUid:\t1 2 3 4\nGid: 5 6 7 8\nThreads: 2\nVmRSS: 42 kB\nFuture: ignored words\n";
static void process_queries(void) {
    mulib_procfs_stat st, before;
    mulib_procfs_status status;
    mulib_procfs_process process;
    mulib_vfs vfs; mulib_vfs_mem mem; mulib_vfs_ops ops = mulib_vfs_mem_ops();
    char text[2048], comm[MULIB_PROCFS_COMM_CAP + 1];
    size_t n = stat_fixture(text, sizeof(text), "odd (name) tail)", 0, NULL);
    assert(mulib_procfs_parse_stat(text, n, &st) == 0);
    assert(st.pid == 123 && st.ppid == 1 && !strcmp(st.comm, "odd (name) tail)") && st.state == 'S');
    assert(st.user_ticks == 11 && st.system_ticks == 12 && st.start_ticks == 19 && st.virtual_bytes == 20 && st.resident_pages == 21);
    before = st;
    assert(mulib_procfs_parse_stat("", 0, &st) == MULIB_VFS_EFORMAT);
    assert(mulib_procfs_parse_stat("123 (bad S 1", 12, &st) == MULIB_VFS_EFORMAT && !memcmp(&before, &st, sizeof(st)));
    assert(mulib_procfs_parse_stat(text, n - 3, &st) == MULIB_VFS_EFORMAT);
    text[2] = 0;
    assert(mulib_procfs_parse_stat(text, n, &st) == MULIB_VFS_EFORMAT);
    n = stat_fixture(text, sizeof(text), "x", 14, "18446744073709551616");
    assert(mulib_procfs_parse_stat(text, n, &st) == MULIB_VFS_ELIMIT);
    n = stat_fixture(text, sizeof(text), "x", 14, "-1");
    assert(mulib_procfs_parse_stat(text, n, &st) == MULIB_VFS_EFORMAT);
    n = stat_fixture(text, sizeof(text), "x", 24, "-9223372036854775808");
    assert(mulib_procfs_parse_stat(text, n, &st) == 0 && st.resident_pages == INT64_MIN);
    memset(comm, 'x', sizeof(comm)); comm[MULIB_PROCFS_COMM_CAP - 1] = 0;
    n = stat_fixture(text, sizeof(text), comm, 0, NULL);
    assert(mulib_procfs_parse_stat(text, n, &st) == 0);
    comm[MULIB_PROCFS_COMM_CAP - 1] = 'x'; comm[MULIB_PROCFS_COMM_CAP] = 0;
    n = stat_fixture(text, sizeof(text), comm, 0, NULL);
    assert(mulib_procfs_parse_stat(text, n, &st) == MULIB_VFS_ELIMIT);
    assert(mulib_procfs_parse_status(process_status, sizeof(process_status) - 1, &status) == 0);
    assert(status.pid == 123 && status.uid[3] == 4 && status.gid[0] == 5 && status.resident_kib == 42 && status.threads == 2 && (status.present & MULIB_PROCFS_HAS_RSS));
    assert(mulib_procfs_parse_status("Name: x\nPid: 1\nPPid: 0\n", 23, &status) == 0 && !(status.present & MULIB_PROCFS_HAS_RSS));
    {
        const char *bad[] = {"", "Pid: 1\n", "Name: x\nPid: 1\nPPid: 0\nPid: 2\n", "Name: x\nPid: 1\nPPid: 0\nVmRSS: 3 MB\n", "Name: x\nPid: 1\nPPid: 0\nUid: 1 2 3\n", "Name: x\nPid: 1\nPPid: 0\nmalformed\n"};
        size_t i;
        for (i = 0; i < sizeof(bad) / sizeof(*bad); ++i) assert(mulib_procfs_parse_status(bad[i], strlen(bad[i]), &status) == MULIB_VFS_EFORMAT);
    }
    assert(mulib_vfs_init(&vfs, 1) == 0 && mulib_vfs_mem_init(&mem, 4, MULIB_PROCFS_STATUS_LIMIT + 1) == 0);
    assert(mulib_vfs_mount(&vfs, "/proc", &ops, &mem) == 0 && mulib_vfs_mem_mkdir(&mem, "/123") == 0);
    n = stat_fixture(text, sizeof(text), "odd (name) tail)", 0, NULL);
    assert(mulib_vfs_mem_put(&mem, "/123/stat", text, n) == 0);
    assert(mulib_vfs_mem_put(&mem, "/123/status", process_status, sizeof(process_status) - 1) == 0);
    assert(mulib_procfs_query(&vfs, "/proc", 123, &process) == 0 && process.stat.user_ticks == 11 && process.status.uid[0] == 1);
    assert(mulib_procfs_query(&vfs, "/../proc", 123, &process) == MULIB_VFS_EINVAL);
    assert(mulib_procfs_query(&vfs, "/proc", 0, &process) == MULIB_VFS_EINVAL);
    assert(mulib_procfs_query(&vfs, "/proc", 124, &process) == MULIB_VFS_ENOENT);
    vf_fail = vf_calls + 1;
    assert(mulib_procfs_query(&vfs, "/proc", 123, &process) == MULIB_VFS_ENOMEM);
    vf_fail = 0;
    {
        mulib_procfs_process saved = process;
        char *large = (char *)malloc(MULIB_PROCFS_STATUS_LIMIT + 1);
        assert(large); memset(large, 'x', MULIB_PROCFS_STATUS_LIMIT + 1);
        memcpy(large, process_status, sizeof(process_status) - 1);
        memcpy(large + sizeof(process_status) - 1, "Padding:", 8);
        assert(mulib_vfs_mem_put(&mem, "/123/status", large, MULIB_PROCFS_STATUS_LIMIT) == 0);
        assert(mulib_procfs_query(&vfs, "/proc", 123, &process) == 0 && process.status.pid == 123);
        assert(mulib_vfs_mem_put(&mem, "/123/status", large, MULIB_PROCFS_STATUS_LIMIT + 1) == 0);
        assert(mulib_procfs_query(&vfs, "/proc", 123, &process) == MULIB_VFS_ELIMIT && !memcmp(&saved, &process, sizeof(process)));
        assert(mulib_vfs_mem_put(&mem, "/123/status", process_status, sizeof(process_status) - 1) == 0);
        memset(large, ' ', MULIB_PROCFS_STAT_LIMIT + 1); memcpy(large, text, n);
        assert(mulib_vfs_mem_put(&mem, "/123/stat", large, MULIB_PROCFS_STAT_LIMIT) == 0);
        assert(mulib_procfs_query(&vfs, "/proc", 123, &process) == 0);
        assert(mulib_vfs_mem_put(&mem, "/123/stat", large, MULIB_PROCFS_STAT_LIMIT + 1) == 0);
        assert(mulib_procfs_query(&vfs, "/proc", 123, &process) == MULIB_VFS_ELIMIT);
        assert(mulib_vfs_mem_put(&mem, "/123/stat", text, n) == 0);
        free(large);
        assert(mulib_vfs_mem_put(&mem, "/123/status", "Name: x\nPid: 124\nPPid: 1\n", 25) == 0);
        assert(mulib_procfs_query(&vfs, "/proc", 123, &process) == MULIB_VFS_EFORMAT && !memcmp(&saved, &process, sizeof(process)));
    }
    mulib_vfs_free(&vfs); mulib_vfs_mem_free(&mem); assert(!vf_live);
#if defined(__linux__)
    {
        FILE *file = fopen("/proc/self/stat", "rb");
        if (file) { assert(fclose(file) == 0); assert(mulib_procfs_query_native((uint32_t)getpid(), &process) == 0 && process.stat.pid == (uint32_t)getpid()); }
    }
#else
    assert(mulib_procfs_query_native(1, &process) == MULIB_VFS_ENOTSUP);
#endif
}
static void attributes(void) {
    mulib_vfs vfs; mulib_vfs_mem mem; mulib_vfs_ops ops = mulib_vfs_mem_ops();
    mulib_sysfs sys;
    size_t n, i;
    char text[128]; int64_t signed_value = 7; uint64_t unsigned_value = 7; int boolean = 7;
    const char *bad[] = {"", "/abs", "../x", "a/../x", ".", "a/./x", "a//x", "a/", "a\\x", "a\nx"};
    assert(mulib_vfs_init(&vfs, 1) == 0 && mulib_vfs_mem_init(&mem, 8, 256) == 0);
    assert(mulib_vfs_mount(&vfs, "/sys", &ops, &mem) == 0);
    assert(mulib_sysfs_init(&sys, &vfs, "/sys/.", 64) == 0);
    for (i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
        assert(mulib_sysfs_validate(bad[i]) == MULIB_VFS_EINVAL);
        assert(mulib_sysfs_read(&sys, bad[i], text, sizeof(text), &n) == MULIB_VFS_EINVAL && !n && !text[0]);
        assert(mulib_sysfs_write(&sys, bad[i], "x", 1) == MULIB_VFS_EINVAL);
    }
    assert(mulib_sysfs_write(&sys, "missing", "x", 1) == MULIB_VFS_ENOENT);
    assert(mulib_vfs_mem_put(&mem, "/value", "123\n", 4) == 0);
    assert(mulib_sysfs_read(&sys, "value", text, 5, &n) == 0 && n == 4 && !strcmp(text, "123\n"));
    assert(mulib_sysfs_read(&sys, "value", text, 4, &n) == MULIB_VFS_ELIMIT && !n && !text[0]);
    assert(mulib_sysfs_read_i64(&sys, "value", &signed_value) == 0 && signed_value == 123);
    assert(mulib_vfs_mem_put(&mem, "/value", "-9223372036854775808\n", 21) == 0);
    assert(mulib_sysfs_read_i64(&sys, "value", &signed_value) == 0 && signed_value == INT64_MIN);
    assert(mulib_sysfs_read_u64(&sys, "value", &unsigned_value) == MULIB_VFS_EFORMAT && unsigned_value == 7);
    assert(mulib_vfs_mem_put(&mem, "/value", "18446744073709551615\n", 21) == 0);
    assert(mulib_sysfs_read_u64(&sys, "value", &unsigned_value) == 0 && unsigned_value == UINT64_MAX);
    assert(mulib_sysfs_read_i64(&sys, "value", &signed_value) == MULIB_VFS_ELIMIT && signed_value == INT64_MIN);
    assert(mulib_vfs_mem_put(&mem, "/value", "18446744073709551616", 20) == 0);
    assert(mulib_sysfs_read_u64(&sys, "value", &unsigned_value) == MULIB_VFS_ELIMIT);
    assert(mulib_vfs_mem_put(&mem, "/value", "1x", 2) == 0);
    assert(mulib_sysfs_read_i64(&sys, "value", &signed_value) == MULIB_VFS_EFORMAT);
    assert(mulib_vfs_mem_put(&mem, "/value", "\t +42 \n", 7) == 0);
    assert(mulib_sysfs_read_i64(&sys, "value", &signed_value) == 0 && signed_value == 42);
    assert(mulib_vfs_mem_put(&mem, "/value", "2\n", 2) == 0);
    assert(mulib_sysfs_read_bool(&sys, "value", &boolean) == MULIB_VFS_EFORMAT && boolean == 7);
    assert(mulib_sysfs_write_bool(&sys, "value", 1) == 0);
    assert(mulib_sysfs_read_bool(&sys, "value", &boolean) == 0 && boolean == 1);
    assert(mulib_sysfs_write_bool(&sys, "value", 2) == MULIB_VFS_EINVAL);
    assert(mulib_vfs_mem_put(&mem, "/value", NULL, 0) == 0);
    assert(mulib_sysfs_write_i64(&sys, "value", INT64_MIN) == 0);
    assert(mulib_sysfs_read_i64(&sys, "value", &signed_value) == 0 && signed_value == INT64_MIN);
    assert(mulib_sysfs_write_u64(&sys, "value", UINT64_MAX) == 0);
    assert(mulib_sysfs_read_u64(&sys, "value", &unsigned_value) == 0 && unsigned_value == UINT64_MAX);
    assert(mulib_sysfs_write(&sys, "value", "a\0b", 3) == MULIB_VFS_EFORMAT);
    assert(mulib_vfs_mem_put(&mem, "/value", "a\0b", 3) == 0);
    assert(mulib_sysfs_read(&sys, "value", text, sizeof(text), &n) == MULIB_VFS_EFORMAT);
    assert(mulib_sysfs_init(&sys, &vfs, "/sys", 2) == 0);
    assert(mulib_vfs_mem_put(&mem, "/value", "123", 3) == 0);
    assert(mulib_sysfs_read(&sys, "value", text, sizeof(text), &n) == MULIB_VFS_ELIMIT);
    assert(mulib_sysfs_write(&sys, "value", "123", 3) == MULIB_VFS_ELIMIT);
    assert(mulib_vfs_mem_put(&mem, "/value", "12", 2) == 0);
    assert(mulib_sysfs_read(&sys, "value", text, sizeof(text), &n) == 0 && n == 2);
    assert(mulib_vfs_mem_put(&mem, "/empty", NULL, 0) == 0);
    assert(mulib_sysfs_read(&sys, "empty", text, 1, &n) == 0 && !n && !text[0]);
    mulib_vfs_free(&vfs); mulib_vfs_mem_free(&mem); assert(!vf_live);
#if defined(__linux__)
    {
        FILE *file = fopen("/proc/sys/kernel/pid_max", "rb");
        if (file) {
            assert(fclose(file) == 0);
            assert(mulib_sysfs_init(&sys, NULL, "/proc/sys/kernel", 128) == 0);
            assert(mulib_sysfs_read_u64(&sys, "pid_max", &unsigned_value) == 0 && unsigned_value > 0);
        }
    }
#else
    assert(mulib_sysfs_init(&sys, NULL, "/sys", 128) == MULIB_VFS_ENOTSUP);
#endif
}
static void index_and_persistence(void) {
    mulib_pathdb db, copy, limited;
    mulib_vfs_stat info = {MULIB_VFS_FILE, 42};
    mulib_vfs vfs; mulib_vfs_mem mem; mulib_vfs_ops ops = mulib_vfs_mem_ops();
    size_t n, visited; FILE *file; char path[] = "/a/item";
    assert(mulib_pathdb_init(&db, 16) == 0 && mulib_pathdb_init(&copy, 128) == 0 && mulib_pathdb_init(&limited, 1) == 0);
    file = tmpfile(); assert(file && mulib_pathdb_save(&copy, file) == 0); rewind(file);
    assert(mulib_pathdb_load(&copy, file) == 0 && !copy.count && fclose(file) == 0);
    assert(mulib_pathdb_put(&db, path, &info) == 0); path[1] = 'X';
    assert(mulib_pathdb_put(&db, "/ab/item", &db.entries[0].info) == 0);
    assert(mulib_pathdb_put(&db, "/a/./other", &info) == 0);
    assert(mulib_pathdb_put(&db, "/../bad", &info) == MULIB_VFS_EINVAL);
    n = 0; assert(mulib_pathdb_query(&db, "/a", NULL, count_entry, &n, &visited) == 0 && n == 2 && visited == 2);
    n = 0; assert(mulib_pathdb_query(&db, NULL, "item", count_entry, &n, &visited) == 0 && n == 2);
    n = 0; assert(mulib_pathdb_query(&db, NULL, NULL, stop_entry, &n, &visited) == MULIB_VFS_ECANCELED && n == 1 && visited == 1);
    assert(mulib_pathdb_put(&limited, "/first", &info) == 0);
    assert(mulib_pathdb_put(&limited, "/second", &info) == MULIB_VFS_ELIMIT);
    info.size = 99; assert(mulib_pathdb_put(&limited, "/first", &info) == 0 && limited.entries[0].info.size == 99);
    file = tmpfile(); assert(file && mulib_pathdb_save(&db, file) == 0); rewind(file);
    assert(mulib_pathdb_load(&copy, file) == 0 && copy.count == db.count);
    for (n = 0; n < db.count; ++n) assert(!strcmp(db.entries[n].path, copy.entries[n].path) && db.entries[n].info.size == copy.entries[n].info.size && db.entries[n].path != copy.entries[n].path);
    assert(fseek(file, 0, SEEK_END) == 0 && fputc('X', file) != EOF); rewind(file);
    assert(mulib_pathdb_load(&copy, file) == MULIB_VFS_EFORMAT && copy.count == 3);
    rewind(file); assert(mulib_pathdb_load(&limited, file) == MULIB_VFS_ELIMIT && limited.count == 1);
    assert(fclose(file) == 0);
    file = tmpfile(); assert(file && fwrite("MPD1\1\0\0\0\3\0\1", 1, 11, file) == 11); rewind(file);
    assert(mulib_pathdb_load(&copy, file) == MULIB_VFS_EFORMAT && copy.count == 3); assert(fclose(file) == 0);
    file = tmpfile(); assert(file); rewind(file);
    assert(mulib_pathdb_load(&copy, file) == MULIB_VFS_EFORMAT); assert(fclose(file) == 0);
    assert(mulib_vfs_init(&vfs, 1) == 0 && mulib_vfs_mem_init(&mem, 80, 64) == 0);
    assert(mulib_vfs_mount(&vfs, "/", &ops, &mem) == 0);
    assert(mulib_vfs_mem_mkdir(&mem, "/tree") == 0 && mulib_vfs_mem_put(&mem, "/tree/file", "x", 1) == 0);
    assert(mulib_pathdb_scan(&db, &vfs, "/tree") == 0 && db.count == 2 && !strcmp(db.entries[0].path, "/tree"));
    assert(mulib_pathdb_scan(&limited, &vfs, "/tree") == MULIB_VFS_ELIMIT && !strcmp(limited.entries[0].path, "/first"));
    assert(mulib_vfs_mem_remove(&mem, "/tree/file") == 0 && db.entries[1].info.size == 1);
    assert(mulib_pathdb_remove(&db, "/tree/file") == 0 && mulib_pathdb_remove(&db, "/tree/file") == MULIB_VFS_ENOENT);
    {
        char deep[512] = ""; size_t depth;
        for (depth = 0; depth <= MULIB_PATHDB_MAX_DEPTH; ++depth) { strcat(deep, "/d"); assert(mulib_vfs_mem_mkdir(&mem, deep) == 0); }
        assert(mulib_pathdb_scan(&copy, &vfs, "/") == MULIB_VFS_ELIMIT && copy.count == 3);
    }
    mulib_vfs_free(&vfs); mulib_vfs_mem_free(&mem);
    mulib_pathdb_free(&db); mulib_pathdb_free(&copy); mulib_pathdb_free(&limited); assert(!vf_live);
}
static void record_le(FILE *file, uint64_t value, size_t bytes) {
    size_t i;
    for (i = 0; i < bytes; ++i) { assert(fputc((int)(value & 255), file) != EOF); value >>= 8; }
}
static void record_path(FILE *file, const char *path) {
    size_t n = strlen(path);
    record_le(file, n, 2); record_le(file, MULIB_VFS_FILE, 1); record_le(file, 42, 8);
    assert(fwrite(path, 1, n, file) == n);
}
static mulib_vfs_result directory_stat(void *ctx, const char *path, mulib_vfs_stat *info) {
    (void)ctx; (void)path; info->kind = MULIB_VFS_DIRECTORY; info->size = 0; return MULIB_VFS_OK;
}
static mulib_vfs_result malformed_listing(void *ctx, const char *path, mulib_vfs_visit visit, void *user) {
    mulib_vfs_stat info = {MULIB_VFS_FILE, 0};
    (void)path;
    return visit(user, (const char *)ctx, &info) ? MULIB_VFS_ECANCELED : MULIB_VFS_OK;
}
static void boundaries_and_records(void) {
    char path[MULIB_VFS_PATH_CAP + 1], output[8];
    size_t n, i;
    mulib_vfs_mem mem; mulib_vfs vfs;
    mulib_vfs_stat info = {MULIB_VFS_FILE, UINT64_MAX};
    mulib_pathdb db, restored;
    mulib_sysfs sys;
    mulib_vfs_ops ops = mulib_vfs_mem_ops(), malformed = {NULL, NULL, directory_stat, malformed_listing};
    const char *bad[] = {"relative", "/a/../b", "/../escape", "/a//b", "/back\\slash"};
    FILE *file;
    assert(mulib_vfs_init(&vfs, SIZE_MAX) == MULIB_VFS_EINVAL);
    assert(mulib_pathdb_init(&db, SIZE_MAX) == MULIB_VFS_EINVAL);
    assert(mulib_vfs_mem_init(&mem, 1, 0) == 0 && mulib_vfs_init(&vfs, 1) == 0);
    assert(mulib_vfs_mem_put(&mem, "/empty", NULL, 0) == 0);
    assert(mulib_vfs_mem_put(&mem, "/second", NULL, 0) == MULIB_VFS_ELIMIT);
    assert(mulib_vfs_mem_put(&mem, "/empty", "x", 1) == MULIB_VFS_ELIMIT);
    assert(mulib_vfs_mount(&vfs, "/", &ops, &mem) == 0);
    assert(mulib_sysfs_init(&sys, &vfs, "/", 8) == 0);
    memset(path, 'x', sizeof(path)); path[MULIB_VFS_PATH_CAP - 1] = 0;
    assert(mulib_sysfs_validate(path) == 0);
    assert(mulib_sysfs_read(&sys, path, output, sizeof(output), &n) == MULIB_VFS_ELIMIT);
    path[MULIB_VFS_PATH_CAP - 1] = 'x'; path[MULIB_VFS_PATH_CAP] = 0;
    assert(mulib_sysfs_validate(path) == MULIB_VFS_ELIMIT);
    {
        char *unterminated = (char *)malloc(MULIB_VFS_PATH_CAP);
        assert(unterminated); memset(unterminated, 'x', MULIB_VFS_PATH_CAP);
        assert(mulib_sysfs_validate(unterminated) == MULIB_VFS_ELIMIT);
        free(unterminated);
    }
    mulib_vfs_free(&vfs); mulib_vfs_mem_free(&mem);
    assert(mulib_pathdb_init(&db, 2) == 0 && mulib_pathdb_init(&restored, 2) == 0);
    memset(path, 'x', sizeof(path)); path[0] = '/'; path[MULIB_VFS_PATH_CAP - 1] = 0;
    assert(mulib_pathdb_put(&db, path, &info) == 0);
    file = tmpfile(); assert(file && mulib_pathdb_save(&db, file) == 0); rewind(file);
    assert(mulib_pathdb_load(&restored, file) == 0 && restored.entries[0].info.size == UINT64_MAX && !strcmp(restored.entries[0].path, path));
    assert(fclose(file) == 0);
    for (i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
        file = tmpfile(); assert(file && fwrite("MPD1", 1, 4, file) == 4);
        record_le(file, 1, 4); record_path(file, bad[i]); rewind(file);
        assert(mulib_pathdb_load(&restored, file) == MULIB_VFS_EFORMAT && restored.count == 1);
        assert(fclose(file) == 0);
    }
    file = tmpfile(); assert(file && fwrite("MPD1", 1, 4, file) == 4);
    record_le(file, 2, 4); record_path(file, "/same"); record_path(file, "/same"); rewind(file);
    assert(mulib_pathdb_load(&restored, file) == MULIB_VFS_EFORMAT && restored.count == 1); assert(fclose(file) == 0);
    assert(mulib_vfs_init(&vfs, 1) == 0);
    assert(mulib_vfs_mount(&vfs, "/", &malformed, (void *)"../escape") == 0);
    assert(mulib_pathdb_scan(&db, &vfs, "/") == MULIB_VFS_EFORMAT && db.count == 1);
    assert(mulib_vfs_unmount(&vfs, "/") == 0);
    assert(mulib_vfs_mount(&vfs, "/", &malformed, (void *)"back\\slash") == 0);
    assert(mulib_pathdb_scan(&db, &vfs, "/") == MULIB_VFS_EFORMAT && db.count == 1);
    assert(mulib_vfs_unmount(&vfs, "/") == 0);
    {
        char *unterminated = (char *)malloc(MULIB_VFS_PATH_CAP);
        assert(unterminated); memset(unterminated, 'x', MULIB_VFS_PATH_CAP);
        assert(mulib_vfs_mount(&vfs, "/", &malformed, unterminated) == 0);
        assert(mulib_pathdb_scan(&db, &vfs, "/") == MULIB_VFS_ELIMIT && db.count == 1);
        mulib_vfs_free(&vfs); free(unterminated);
    }
    mulib_pathdb_free(&db); mulib_pathdb_free(&restored); assert(!vf_live);
}
static void allocation_failures(void) {
    size_t k;
    mulib_vfs_ops ops = mulib_vfs_mem_ops();
    for (k = 1; k <= 2; ++k) {
        mulib_vfs vfs;
        assert(mulib_vfs_init(&vfs, 1) == 0); vf_fail = vf_calls + k;
        assert(mulib_vfs_mount(&vfs, "/", &ops, NULL) == MULIB_VFS_ENOMEM && !vfs.count);
        vf_fail = 0; mulib_vfs_free(&vfs); assert(!vf_live);
    }
    for (k = 1; k <= 3; ++k) {
        mulib_vfs_mem mem;
        assert(mulib_vfs_mem_init(&mem, 2, 16) == 0); vf_fail = vf_calls + k;
        assert(mulib_vfs_mem_put(&mem, "/file", "old", 3) == MULIB_VFS_ENOMEM && !mem.count);
        vf_fail = 0; assert(mulib_vfs_mem_put(&mem, "/file", "old", 3) == 0);
        vf_fail = vf_calls + 1;
        assert(mulib_vfs_mem_put(&mem, "/file", "new", 3) == MULIB_VFS_ENOMEM && !memcmp(mem.nodes[0].data, "old", 3));
        vf_fail = 0; mulib_vfs_mem_free(&mem); assert(!vf_live);
    }
    for (k = 1; k <= 4; ++k) {
        mulib_vfs_mem mem; mulib_vfs vfs; size_t n;
        assert(mulib_vfs_mem_init(&mem, 2, 16) == 0 && mulib_vfs_init(&vfs, 1) == 0);
        assert(mulib_vfs_mount(&vfs, "/", &ops, &mem) == 0);
        vf_fail = vf_calls + k;
        assert(mulib_vfs_write(&vfs, "/file", 2, "new", 3, &n) == MULIB_VFS_ENOMEM && !n && !mem.count);
        vf_fail = 0;
        assert(mulib_vfs_mem_put(&mem, "/file", "old", 3) == 0);
        if (k <= 2) {
            vf_fail = vf_calls + k;
            assert(mulib_vfs_write(&vfs, "/file", 0, "new", 3, &n) == MULIB_VFS_ENOMEM && !memcmp(mem.nodes[0].data, "old", 3));
            vf_fail = 0;
        }
        assert(mulib_vfs_mem_put(&mem, mem.nodes[0].path, mem.nodes[0].data, 3) == 0);
        assert(mulib_vfs_write(&vfs, "/file", 0, mem.nodes[0].data + 1, 2, &n) == 0 && !memcmp(mem.nodes[0].data, "ldd", 3));
        mulib_vfs_free(&vfs); mulib_vfs_mem_free(&mem); assert(!vf_live);
    }
    for (k = 1; k <= 2; ++k) {
        mulib_pathdb db; mulib_vfs_stat info = {MULIB_VFS_FILE, 1};
        assert(mulib_pathdb_init(&db, 2) == 0); vf_fail = vf_calls + k;
        assert(mulib_pathdb_put(&db, "/a", &info) == MULIB_VFS_ENOMEM && !db.count);
        vf_fail = 0; mulib_pathdb_free(&db); assert(!vf_live);
    }
    for (k = 1; k <= 4; ++k) {
        mulib_pathdb db; mulib_vfs vfs; mulib_vfs_mem mem;
        mulib_vfs_stat info = {MULIB_VFS_FILE, 1};
        assert(mulib_pathdb_init(&db, 4) == 0 && mulib_pathdb_put(&db, "/old", &info) == 0);
        assert(mulib_vfs_init(&vfs, 1) == 0 && mulib_vfs_mem_init(&mem, 1, 1) == 0);
        assert(mulib_vfs_mount(&vfs, "/", &ops, &mem) == 0 && mulib_vfs_mem_put(&mem, "/f", "x", 1) == 0);
        vf_fail = vf_calls + k;
        assert(mulib_pathdb_scan(&db, &vfs, "/") == MULIB_VFS_ENOMEM && db.count == 1 && !strcmp(db.entries[0].path, "/old"));
        vf_fail = 0; mulib_vfs_free(&vfs); mulib_vfs_mem_free(&mem); mulib_pathdb_free(&db); assert(!vf_live);
    }
    for (k = 1; k <= 2; ++k) {
        mulib_pathdb db, restored; mulib_vfs_stat info = {MULIB_VFS_FILE, 1}; FILE *file;
        assert(mulib_pathdb_init(&db, 2) == 0 && mulib_pathdb_put(&db, "/new", &info) == 0);
        assert(mulib_pathdb_init(&restored, 2) == 0 && mulib_pathdb_put(&restored, "/old", &info) == 0);
        file = tmpfile(); assert(file && mulib_pathdb_save(&db, file) == 0); rewind(file);
        vf_fail = vf_calls + k;
        assert(mulib_pathdb_load(&restored, file) == MULIB_VFS_ENOMEM && !strcmp(restored.entries[0].path, "/old"));
        vf_fail = 0; assert(fclose(file) == 0);
        mulib_pathdb_free(&db); mulib_pathdb_free(&restored); assert(!vf_live);
    }
}
int main(void) {
    normalization(); routing_and_dispatch(); process_queries(); attributes();
    index_and_persistence(); boundaries_and_records(); allocation_failures(); assert(!vf_live);
    return 0;
}
