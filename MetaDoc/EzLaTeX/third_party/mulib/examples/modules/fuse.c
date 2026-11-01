#define MULIB_IMPL_VFS
#define MULIB_IMPL_FUSE
#include "mulib-fuse.h"
#include <assert.h>
#include <string.h>
int main(void) {
    mulib_vfs vfs; mulib_vfs_mem mem; mulib_vfs_ops ops = mulib_vfs_mem_ops();
    mulib_fuse adapter; mulib_fuse_response response;
    mulib_fuse_request request = {42, MULIB_FUSE_READ, "/note", 0, 5, NULL};
    char bytes[5];
    assert(mulib_vfs_init(&vfs, 1) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_init(&mem, 4, 128) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_put(&mem, "/note", "hello", 5) == MULIB_VFS_OK);
    assert(mulib_vfs_mount(&vfs, "/", &ops, &mem) == MULIB_VFS_OK);
    assert(mulib_fuse_init(&adapter, &vfs, 1) == MULIB_VFS_OK);
    /* A userspace request only: nothing is mounted into the host filesystem. */
    assert(mulib_fuse_dispatch(&adapter, &request, bytes, sizeof(bytes), NULL, NULL, &response) == MULIB_VFS_OK);
    assert(response.id == 42 && response.transferred == 5 && !memcmp(bytes, "hello", 5));
    request.opcode = MULIB_FUSE_WRITE; request.data = "world";
    assert(mulib_fuse_dispatch(&adapter, &request, NULL, 0, NULL, NULL, &response) == MULIB_VFS_EROFS);
    mulib_vfs_free(&vfs); mulib_vfs_mem_free(&mem);
    return 0;
}
