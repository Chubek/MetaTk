#include <assert.h>
#include <errno.h>
#include <stdio.h>
#define MULIB_IMPL_SYSCALL
#include "mulib-syscall.h"
int main(void) {
    int pair[2]; size_t count; char buffer[4];
    if (mulib_syscall_pipe(pair) < 0) {
        if (errno == ENOSYS || errno == EPERM || errno == EACCES || errno == EMFILE || errno == ENFILE) {
            perror("SKIP native pipe unavailable"); return 0;
        }
        assert(0);
    }
    assert(mulib_syscall_write(pair[1], "abc", 3, &count) == 0 && count == 3);
    assert(mulib_syscall_read(pair[0], buffer, sizeof buffer, &count) == 0 && count == 3);
    assert(buffer[0] == 'a' && buffer[2] == 'c');
    assert(mulib_syscall_close(pair[1]) == 0);
    assert(mulib_syscall_read(pair[0], buffer, sizeof buffer, &count) == 0 && count == 0);
    assert(mulib_syscall_close(pair[0]) == 0);
    return 0;
}
