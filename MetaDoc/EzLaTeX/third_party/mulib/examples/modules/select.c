#include <assert.h>
#include <errno.h>
#include <stdio.h>
#define MULIB_IMPL_SELECT
#define MULIB_IMPL_SYSCALL
#include "mulib-select.h"
#include "mulib-syscall.h"
int main(void) {
    int pair[2]; size_t count; char byte;
    mulib_select_entry entry;
    if (mulib_syscall_pipe(pair) < 0) {
        if (errno == ENOSYS || errno == EPERM || errno == EACCES || errno == EMFILE || errno == ENFILE) {
            perror("SKIP pipe/select unavailable"); return 0;
        }
        assert(0);
    }
    entry.fd = pair[0]; entry.events = MULIB_SELECT_READ; entry.revents = 0;
    assert(mulib_select_wait(&entry, 1, 0) == 0);
    assert(mulib_syscall_write(pair[1], "s", 1, &count) == 0 && count == 1);
    assert(mulib_select_wait(&entry, 1, 1000) == 1 && (entry.revents & MULIB_SELECT_READ));
    assert(mulib_syscall_read(pair[0], &byte, 1, &count) == 0 && count == 1 && byte == 's');
    assert(mulib_syscall_close(pair[0]) == 0 && mulib_syscall_close(pair[1]) == 0);
    return 0;
}
