#include <assert.h>
#include <errno.h>
#include <stdio.h>
#define MULIB_IMPL_FSWATCH
#define MULIB_IMPL_SELECT
#include "mulib-fswatch.h"
#include "mulib-select.h"
#if defined(__linux__)
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif
int main(void) {
#if defined(__linux__)
    int fd = mulib_fswatch_create(), watch, file, made = 0;
    unsigned attempt;
    char path[128], child[160], buffer[4096]; size_t length, offset = 0;
    mulib_select_entry entry; mulib_fswatch_event event;
    if (fd < 0) {
        if (errno == ENOSYS || errno == EPERM || errno == EACCES || errno == EMFILE || errno == ENFILE) {
            perror("SKIP inotify unavailable"); return 0;
        }
        assert(0);
    }
    for (attempt = 0; attempt < 100; ++attempt) {
        assert(snprintf(path, sizeof path, "mulib-fswatch-example-%ld-%u", (long)getpid(), attempt) > 0);
        if (mkdir(path, 0700) == 0) { made = 1; break; }
        if (errno == EEXIST) continue;
        if (errno == EACCES || errno == EPERM || errno == EROFS || errno == ENOSPC) {
            perror("SKIP writable local directory unavailable");
            assert(mulib_fswatch_close(fd) == 0); return 0;
        }
        assert(0);
    }
    if (!made) {
        fputs("SKIP filesystem example: test names occupied\n", stderr);
        assert(mulib_fswatch_close(fd) == 0); return 0;
    }
    watch = mulib_fswatch_add(fd, path, MULIB_FSWATCH_CREATE);
    if (watch < 0) {
        int error = errno;
        assert(mulib_fswatch_close(fd) == 0 && rmdir(path) == 0); errno = error;
        if (errno == EPERM || errno == EACCES || errno == ENOSPC || errno == ENOSYS) {
            perror("SKIP inotify watch unavailable"); return 0;
        }
        assert(0);
    }
    assert(snprintf(child, sizeof child, "%s/item", path) > 0);
    file = open(child, O_WRONLY | O_CREAT | O_EXCL, 0600); assert(file >= 0 && close(file) == 0);
    entry.fd = fd; entry.events = MULIB_SELECT_READ; entry.revents = 0;
    assert(mulib_select_wait(&entry, 1, 1000) == 1);
    assert(mulib_fswatch_read(fd, buffer, sizeof buffer, &length) == 0);
    assert(mulib_fswatch_next(buffer, length, &offset, &event) == 1);
    assert(event.watch == watch && (event.mask & MULIB_FSWATCH_CREATE) && event.name_length == 4);
    assert(mulib_fswatch_remove(fd, watch) == 0 && mulib_fswatch_close(fd) == 0);
    assert(unlink(child) == 0 && rmdir(path) == 0);
#else
    assert(mulib_fswatch_create() == -1 && errno == ENOSYS);
    fputs("SKIP inotify requires Linux\n", stderr);
#endif
    return 0;
}
