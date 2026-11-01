#define MULIB_IMPL_VFS
#define MULIB_IMPL_PATHDB
#include "mulib-pathdb.h"
#include <assert.h>
#include <string.h>
static int found(void *user, const mulib_pathdb_entry *entry) {
    size_t *count = (size_t *)user;
    assert(!strcmp(entry->path, "/docs/note.txt") && entry->info.size == 3);
    ++*count; return 0;
}
int main(void) {
    mulib_vfs vfs; mulib_vfs_mem mem; mulib_vfs_ops ops = mulib_vfs_mem_ops();
    mulib_pathdb db, restored; FILE *file; size_t count = 0, visited;
    assert(mulib_vfs_init(&vfs, 1) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_init(&mem, 2, 128) == MULIB_VFS_OK);
    assert(mulib_vfs_mem_mkdir(&mem, "/docs") == MULIB_VFS_OK);
    assert(mulib_vfs_mem_put(&mem, "/docs/note.txt", "abc", 3) == MULIB_VFS_OK);
    assert(mulib_vfs_mount(&vfs, "/", &ops, &mem) == MULIB_VFS_OK);
    assert(mulib_pathdb_init(&db, 16) == MULIB_VFS_OK && mulib_pathdb_init(&restored, 16) == MULIB_VFS_OK);
    assert(mulib_pathdb_scan(&db, &vfs, "/docs") == MULIB_VFS_OK);
    file = tmpfile(); assert(file && mulib_pathdb_save(&db, file) == MULIB_VFS_OK);
    rewind(file);
    assert(mulib_pathdb_load(&restored, file) == MULIB_VFS_OK && fclose(file) == 0);
    assert(mulib_pathdb_query(&restored, "/docs", ".txt", found, &count, &visited) == MULIB_VFS_OK && count == 1 && visited == 1);
    mulib_pathdb_free(&restored); mulib_pathdb_free(&db);
    mulib_vfs_free(&vfs); mulib_vfs_mem_free(&mem);
    return 0;
}
