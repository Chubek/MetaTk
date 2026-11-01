#define MULIB_IMPL_VFS
#include "mulib-vfs.h"
#include <assert.h>
#include <string.h>
static int child(void *user, const char *name, const mulib_vfs_stat *info) {
    size_t *count = (size_t *)user;
    assert(!strcmp(name, "hello") && info->kind == MULIB_VFS_FILE && info->size == 5);
    ++*count; return 0;
}
int main(void) {
    mulib_vfs vfs; mulib_vfs_mem mem; mulib_vfs_ops ops = mulib_vfs_mem_ops();
    char bytes[8], path[32]; size_t n, children = 0;
    assert(mulib_vfs_init(&vfs, 4) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_init(&mem, 16, 1024) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_put(&mem, "/hello", "world", 5) == MULIB_VFS_OK);
    assert(mulib_vfs_mount(&vfs, "/memory", &ops, &mem) == MULIB_VFS_OK);
    assert(mulib_vfs_normalize("/memory/./a/../hello", path, sizeof(path)) == MULIB_VFS_OK);
    assert(mulib_vfs_read(&vfs, path, 0, bytes, sizeof(bytes), &n) == MULIB_VFS_OK);
    assert(n == 5 && !memcmp(bytes, "world", n));
    assert(mulib_vfs_write(&vfs, path, 0, "W", 1, &n) == MULIB_VFS_OK && n == 1);
    assert(mulib_vfs_list(&vfs, "/memory", child, &children) == MULIB_VFS_OK && children == 1);
    assert(mulib_vfs_read(&vfs, "/../escape", 0, bytes, sizeof(bytes), &n) == MULIB_VFS_EINVAL);
    mulib_vfs_free(&vfs); /* Does not destroy the borrowed backend. */
    mulib_vfs_mem_free(&mem);
    return 0;
}
