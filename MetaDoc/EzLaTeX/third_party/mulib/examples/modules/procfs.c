#define MULIB_IMPL_VFS
#define MULIB_IMPL_PROCFS
#include "mulib-procfs.h"
#include <assert.h>
#include <string.h>
int main(void) {
    const char stat_text[] = "123 (a tricky (comm)) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21\n";
    const char status_text[] = "Name:\ta tricky (comm)\nPid: 123\nPPid: 1\nUid: 1 1 1 1\nThreads: 1\nVmRSS: 42 kB\n";
    mulib_vfs vfs; mulib_vfs_mem mem; mulib_vfs_ops ops = mulib_vfs_mem_ops();
    mulib_procfs_process process;
    assert(mulib_vfs_init(&vfs, 1) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_init(&mem, 3, 4096) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_mkdir(&mem, "/123") == MULIB_VFS_OK);
    assert(mulib_vfs_mem_put(&mem, "/123/stat", stat_text, sizeof(stat_text) - 1) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_put(&mem, "/123/status", status_text, sizeof(status_text) - 1) == MULIB_VFS_OK);
    assert(mulib_vfs_mount(&vfs, "/proc", &ops, &mem) == MULIB_VFS_OK);
    assert(mulib_procfs_query(&vfs, "/proc", 123, &process) == MULIB_VFS_OK);
    assert(!strcmp(process.stat.comm, "a tricky (comm)"));
    assert(process.stat.user_ticks == 11 && process.stat.resident_pages == 21);
    assert(process.status.resident_kib == 42 && (process.status.present & MULIB_PROCFS_HAS_RSS));
    mulib_vfs_free(&vfs); mulib_vfs_mem_free(&mem);
    return 0;
}
