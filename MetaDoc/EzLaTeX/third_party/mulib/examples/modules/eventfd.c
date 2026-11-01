#include <assert.h>
#include <errno.h>
#include <stdio.h>
#define MULIB_IMPL_EVENTFD
#include "mulib-eventfd.h"
int main(void) {
    int fd = mulib_eventfd_create(0, 0);
    uint64_t count = 99;
    if (fd < 0) {
        if (errno == ENOSYS || errno == EPERM || errno == EACCES || errno == EMFILE || errno == ENFILE) {
            perror("SKIP eventfd unavailable"); return 0;
        }
        assert(0);
    }
    assert(mulib_eventfd_read(fd, &count) == -1 && errno == EAGAIN && count == 99);
    assert(mulib_eventfd_write(fd, 2) == 0 && mulib_eventfd_write(fd, 3) == 0);
    assert(mulib_eventfd_read(fd, &count) == 0 && count == 5);
    assert(mulib_eventfd_write(fd, UINT64_MAX) == -1 && errno == EINVAL);
    assert(mulib_eventfd_close(fd) == 0);
    return 0;
}
