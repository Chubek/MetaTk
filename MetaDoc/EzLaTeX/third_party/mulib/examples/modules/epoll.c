#include <assert.h>
#include <errno.h>
#include <stdio.h>
#define MULIB_IMPL_EPOLL
#define MULIB_IMPL_EVENTFD
#include "mulib-epoll.h"
#include "mulib-eventfd.h"
int main(void) {
    int epfd = mulib_epoll_create(), fd;
    uint64_t count; mulib_epoll_event event;
    if (epfd < 0) {
        if (errno == ENOSYS || errno == EPERM || errno == EACCES || errno == EMFILE || errno == ENFILE) {
            perror("SKIP epoll unavailable"); return 0;
        }
        assert(0);
    }
    fd = mulib_eventfd_create(0, 0);
    if (fd < 0) {
        int error = errno;
        assert(mulib_epoll_close(epfd) == 0); errno = error;
        if (errno == ENOSYS || errno == EPERM || errno == EACCES || errno == EMFILE || errno == ENFILE) {
            perror("SKIP eventfd unavailable"); return 0;
        }
        assert(0);
    }
    assert(mulib_epoll_add(epfd, fd, MULIB_EPOLL_READ, 42) == 0);
    assert(mulib_eventfd_write(fd, 7) == 0);
    assert(mulib_epoll_wait(epfd, &event, 1, 1000) == 1);
    assert(event.token == 42 && (event.events & MULIB_EPOLL_READ));
    assert(mulib_eventfd_read(fd, &count) == 0 && count == 7);
    assert(mulib_epoll_remove(epfd, fd) == 0);
    assert(mulib_epoll_close(epfd) == 0 && mulib_eventfd_close(fd) == 0);
    return 0;
}
