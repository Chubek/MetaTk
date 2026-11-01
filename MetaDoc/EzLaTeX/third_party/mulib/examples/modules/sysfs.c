#define MULIB_IMPL_VFS
#define MULIB_IMPL_SYSFS
#include "mulib-sysfs.h"
#include <assert.h>
int main(void) {
    mulib_vfs vfs; mulib_vfs_mem mem; mulib_vfs_ops ops = mulib_vfs_mem_ops();
    mulib_sysfs sys; uint64_t speed; int enabled;
    assert(mulib_vfs_init(&vfs, 1) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_init(&mem, 3, 128) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_mkdir(&mem, "/device") == MULIB_VFS_OK);
    assert(mulib_vfs_mem_put(&mem, "/device/speed", "1000\n", 5) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_put(&mem, "/device/enabled", "0\n", 2) == MULIB_VFS_OK);
    assert(mulib_vfs_mount(&vfs, "/sys", &ops, &mem) == MULIB_VFS_OK);
    assert(mulib_sysfs_init(&sys, &vfs, "/sys", 128) == MULIB_VFS_OK);
    assert(mulib_sysfs_read_u64(&sys, "device/speed", &speed) == MULIB_VFS_OK && speed == 1000);
    assert(mulib_sysfs_write_bool(&sys, "device/enabled", 1) == MULIB_VFS_OK);
    assert(mulib_sysfs_read_bool(&sys, "device/enabled", &enabled) == MULIB_VFS_OK && enabled == 1);
    assert(mulib_sysfs_write_bool(&sys, "../escape", 1) == MULIB_VFS_EINVAL);
    mulib_vfs_free(&vfs); mulib_vfs_mem_free(&mem);
    return 0;
}
