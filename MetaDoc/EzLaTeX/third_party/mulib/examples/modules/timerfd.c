#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#define MULIB_IMPL_TIMERFD
#define MULIB_IMPL_SELECT
#include "mulib-timerfd.h"
#include "mulib-select.h"
int main(void) {
    int fd = mulib_timerfd_create();
    uint64_t count; mulib_select_entry entry;
    if (fd < 0) {
        if (errno == ENOSYS || errno == EPERM || errno == EACCES || errno == EMFILE || errno == ENFILE) {
            perror("SKIP timerfd unavailable"); return 0;
        }
        assert(0);
    }
    entry.fd = fd; entry.events = MULIB_SELECT_READ; entry.revents = 0;
    assert(mulib_timerfd_set(fd, 1000000, 0) == 0);
    assert(mulib_select_wait(&entry, 1, 2000) == 1);
    assert(mulib_timerfd_read(fd, &count) == 0 && count == 1);
    assert(mulib_timerfd_set(fd, 0, 0) == 0);
    assert(mulib_timerfd_close(fd) == 0);
    return 0;
}
