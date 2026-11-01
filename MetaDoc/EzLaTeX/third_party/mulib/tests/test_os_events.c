/* Standard headers deliberately precede every implementation switch. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

static int allocation_budget = -1;
static size_t live_allocations;
static void *events_allocate(size_t size) {
    void *p;
    if (allocation_budget == 0) return NULL;
    if (allocation_budget > 0) --allocation_budget;
    p = malloc(size);
    if (p) ++live_allocations;
    return p;
}
static void events_free(void *p) {
    if (p) { assert(live_allocations); --live_allocations; free(p); }
}
#define MULIB_ASYNCIO_ALLOC(size) events_allocate(size)
#define MULIB_ASYNCIO_FREE(pointer) events_free(pointer)
#define MULIB_IMPL_ASYNCIO
#define MULIB_IMPL_SELECT
#define MULIB_IMPL_EPOLL
#define MULIB_IMPL_EVENTFD
#define MULIB_IMPL_TIMERFD
#define MULIB_IMPL_PERF
#define MULIB_IMPL_SIGNAL
#define MULIB_IMPL_SYSCALL
#define MULIB_IMPL_FSWATCH
#include "mulib-asyncio.h"
#include "mulib-epoll.h"
#include "mulib-eventfd.h"
#include "mulib-timerfd.h"
#include "mulib-perf.h"
#include "mulib-signal.h"
#include "mulib-syscall.h"
#include "mulib-fswatch.h"

static int restricted(const char *operation) {
    int error = errno;
    if (error == ENOSYS || error == EPERM || error == EACCES || error == EMFILE ||
        error == ENFILE || error == ENOSPC || error == EROFS) {
        fprintf(stderr, "SKIP %s: %s\n", operation, strerror(error)); return 1;
    }
    return 0;
}
static volatile sig_atomic_t received_signal;
static void record_signal(int number) { received_signal = number; }
static void signals_and_clocks(void) {
    mulib_signal_guard guard = {0, NULL, 0};
    mulib_signal_guard inner = {0, NULL, 0};
    uint64_t begin = 0, end, elapsed = 99;
    assert(mulib_signal_install(NULL, SIGINT, record_signal) == -1 && errno == EINVAL);
    assert(mulib_signal_install(&guard, 0, record_signal) == -1 && !guard.active);
    assert(mulib_signal_install(&guard, SIGINT, SIG_ERR) == -1 && !guard.active);
    assert(mulib_signal_install(&guard, SIGINT, record_signal) == 0);
    assert(mulib_signal_install(&guard, SIGINT, record_signal) == -1 && errno == EBUSY);
    assert(mulib_signal_install(&inner, SIGINT, SIG_IGN) == 0 && inner.previous == record_signal);
    assert(raise(SIGINT) == 0 && received_signal == 0);
    assert(mulib_signal_restore(&inner) == 0);
    assert(raise(SIGINT) == 0 && received_signal == SIGINT);
    assert(mulib_signal_restore(&guard) == 0 && !guard.active);
    assert(mulib_signal_restore(&guard) == 0);
    assert(mulib_signal_restore(NULL) == -1);
    assert(mulib_perf_now(NULL) == -1 && errno == EINVAL);
    assert(mulib_perf_elapsed(2, 1, &elapsed) == -1 && elapsed == 99);
    assert(mulib_perf_elapsed(0, UINT64_MAX, &elapsed) == 0 && elapsed == UINT64_MAX);
    assert(mulib_perf_elapsed(9, 9, &elapsed) == 0 && elapsed == 0);
    if (mulib_perf_now(&begin) == -1) { assert(restricted("monotonic clock")); return; }
    assert(mulib_perf_now(&end) == 0 && end >= begin);
    assert(mulib_perf_elapsed(begin, end, &elapsed) == 0 && elapsed == end - begin);
}
static void decoder(void) {
    unsigned char unaligned[40];
    uint32_t words[4] = {7, MULIB_FSWATCH_CREATE, 42, 4};
    size_t offset = 0;
    mulib_fswatch_event event = {99, 0, 0, NULL, 0};
    unsigned char *bytes = unaligned + 1;
    memcpy(bytes, words, 16); memcpy(bytes + 16, "x\0\0", 4);
    assert(mulib_fswatch_next(bytes, 20, &offset, &event) == 1);
    assert(offset == 20 && event.watch == 7 && event.mask == MULIB_FSWATCH_CREATE);
    assert(event.cookie == 42 && event.name_length == 1 && event.name[0] == 'x');
    assert(mulib_fswatch_next(bytes, 20, &offset, &event) == 0);
    offset = 0; event.watch = 99;
    assert(mulib_fswatch_next(NULL, 0, &offset, &event) == 0 && event.watch == 99);
    assert(mulib_fswatch_next(NULL, 1, &offset, &event) == -1 && offset == 0);
    assert(mulib_fswatch_next(bytes, 15, &offset, &event) == -1 && event.watch == 99);
    words[3] = UINT32_MAX; memcpy(bytes, words, 16);
    assert(mulib_fswatch_next(bytes, 20, &offset, &event) == -1 && offset == 0);
    words[3] = 4; memcpy(bytes, words, 16); memset(bytes + 16, 'x', 4);
    assert(mulib_fswatch_next(bytes, 20, &offset, &event) == -1 && event.watch == 99);
    words[0] = UINT32_MAX - 1; words[3] = 0; memcpy(bytes, words, 16);
    assert(mulib_fswatch_next(bytes, 16, &offset, &event) == -1);
    words[0] = UINT32_MAX; words[1] = MULIB_FSWATCH_OVERFLOW; memcpy(bytes, words, 16);
    assert(mulib_fswatch_next(bytes, 16, &offset, &event) == 1);
    assert(event.watch == -1 && !event.name && !event.name_length);
    offset = 17;
    assert(mulib_fswatch_next(bytes, 16, &offset, &event) == -1 && offset == 17);
    assert(mulib_fswatch_next(bytes, 16, NULL, &event) == -1);
    assert(mulib_fswatch_next(bytes, 16, &offset, NULL) == -1);
    offset = 0;
    assert(mulib_fswatch_next(bytes, SIZE_MAX, &offset, &event) == -1 && offset == 0);
}

#if defined(__unix__) || defined(__APPLE__)
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
static void fd_flags(int fd) {
    assert((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0);
    assert((fcntl(fd, F_GETFL) & O_NONBLOCK) != 0);
}
static int ready(int fd, unsigned events, int timeout) {
    mulib_select_entry entry = {fd, events, 0};
    int count = mulib_select_wait(&entry, 1, timeout);
    assert(count >= 0);
    return count;
}
static void pipes_and_select(void) {
    int p[2] = {-7, -8};
    char byte;
    size_t count = 77;
    mulib_select_entry entries[2];
    assert(mulib_syscall_pipe(NULL) == -1 && errno == EINVAL);
    if (mulib_syscall_pipe(p) < 0) { assert(restricted("pipe")); return; }
    fd_flags(p[0]); fd_flags(p[1]);
    assert(mulib_syscall_nonblock(p[0], 2) == -1 && errno == EINVAL);
    assert(mulib_syscall_nonblock(p[0], 0) == 0);
    assert(!(fcntl(p[0], F_GETFL) & O_NONBLOCK));
    assert(mulib_syscall_nonblock(p[0], 1) == 0);
    assert(mulib_syscall_read(p[0], &byte, 1, &count) == -1 && errno == EAGAIN && count == 77);
    assert(mulib_syscall_read(p[0], NULL, 1, &count) == -1 && errno == EINVAL);
    assert(mulib_syscall_read(p[0], &byte, SIZE_MAX, &count) == -1 && errno == EINVAL);
    assert(mulib_syscall_write(p[1], NULL, 1, &count) == -1 && errno == EINVAL);
    assert(mulib_syscall_write(p[1], NULL, 0, &count) == 0 && count == 0);
    assert(mulib_select_wait(NULL, 0, 0) == 0);
    assert(mulib_select_wait(NULL, 1, 0) == -1 && errno == EINVAL);
    assert(mulib_select_wait(NULL, 0, -2) == -1 && errno == EINVAL);
    entries[0].fd = mulib_select_limit(); entries[0].events = MULIB_SELECT_READ;
    entries[0].revents = 91;
    assert(mulib_select_wait(entries, 1, 0) == -1 && entries[0].revents == 91);
    assert(mulib_select_wait(entries, (size_t)mulib_select_limit() + 1, 0) == -1);
    entries[0].fd = p[0]; entries[0].events = 0;
    assert(mulib_select_wait(entries, 1, 0) == -1);
    entries[0].events = 8;
    assert(mulib_select_wait(entries, 1, 0) == -1);
    assert(!ready(p[0], MULIB_SELECT_READ, 0));
    assert(ready(p[1], MULIB_SELECT_WRITE, 0));
    assert(mulib_syscall_write(p[1], "a", 1, &count) == 0 && count == 1);
    entries[0].events = MULIB_SELECT_READ; entries[1] = entries[0];
    assert(mulib_select_wait(entries, 2, 1000) == 2); /* duplicate entries count separately */
    assert(mulib_syscall_read(p[0], &byte, 1, &count) == 0 && byte == 'a' && count == 1);
    assert(mulib_syscall_close(p[1]) == 0);
    assert(ready(p[0], MULIB_SELECT_READ, 1000));
    assert(mulib_syscall_read(p[0], &byte, 1, &count) == 0 && count == 0);
    assert(mulib_syscall_close(p[0]) == 0);
    assert(mulib_syscall_read(p[0], &byte, 1, &count) == -1 && errno == EBADF);
    assert(mulib_syscall_close(-1) == -1 && errno == EINVAL);
    {
        pid_t child = fork();
        int status;
        assert(child >= 0);
        if (!child) {
            struct rlimit limit;
            int pair[2] = {-7, -8};
            if (getrlimit(RLIMIT_NOFILE, &limit)) _exit(2);
            limit.rlim_cur = 0;
            if (setrlimit(RLIMIT_NOFILE, &limit)) _exit(2);
            if (mulib_syscall_pipe(pair) != -1 || errno != EMFILE ||
                pair[0] != -7 || pair[1] != -8) _exit(1);
            _exit(0);
        }
        assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    }
}
static void loopback(void) {
    struct sockaddr_in address;
    socklen_t length = sizeof address;
    int server = socket(AF_INET, SOCK_STREAM, 0), client, accepted, error;
    size_t count;
    char byte;
    if (server < 0) { assert(restricted("loopback socket")); return; }
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server, (struct sockaddr *)&address, sizeof address) < 0) {
        assert(restricted("loopback bind")); assert(close(server) == 0); return;
    }
    assert(getsockname(server, (struct sockaddr *)&address, &length) == 0);
    assert(listen(server, 1) == 0 && mulib_syscall_nonblock(server, 1) == 0);
    client = socket(AF_INET, SOCK_STREAM, 0); assert(client >= 0);
    assert(mulib_syscall_nonblock(client, 1) == 0);
    error = connect(client, (struct sockaddr *)&address, sizeof address);
    if (error < 0 && errno != EINPROGRESS) {
        assert(restricted("loopback connect")); assert(close(client) == 0); assert(close(server) == 0); return;
    }
    assert(ready(client, MULIB_SELECT_WRITE, 1000));
    length = sizeof error;
    assert(getsockopt(client, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0);
    assert(ready(server, MULIB_SELECT_READ, 1000));
    accepted = accept(server, NULL, NULL); assert(accepted >= 0);
    assert(mulib_syscall_nonblock(accepted, 1) == 0);
    assert(mulib_syscall_write(client, "L", 1, &count) == 0 && count == 1);
    assert(ready(accepted, MULIB_SELECT_READ, 1000));
    assert(mulib_syscall_read(accepted, &byte, 1, &count) == 0 && count == 1 && byte == 'L');
    assert(close(accepted) == 0 && close(client) == 0 && close(server) == 0);
}
typedef struct reactor_state { int output, jobs, reads; uint64_t id; } reactor_state;
static void next_job(mulib_asyncio *loop, void *user) {
    reactor_state *state = (reactor_state *)user;
    (void)loop; ++state->jobs;
}
static void write_job(mulib_asyncio *loop, void *user) {
    reactor_state *state = (reactor_state *)user;
    size_t count;
    assert(mulib_asyncio_run_once(loop, 0) == -1 && errno == EBUSY);
    assert(mulib_asyncio_destroy(loop) == -1 && errno == EBUSY);
    assert(mulib_syscall_write(state->output, "R", 1, &count) == 0 && count == 1);
    assert(mulib_asyncio_post(loop, next_job, state) == 0);
    ++state->jobs;
}
static void read_callback(mulib_asyncio *loop, int fd, unsigned events, void *user) {
    reactor_state *state = (reactor_state *)user;
    size_t count;
    char byte;
    assert(events & MULIB_SELECT_READ);
    assert(mulib_syscall_read(fd, &byte, 1, &count) == 0 && count == 1 && byte == 'R');
    assert(mulib_asyncio_unwatch(loop, state->id) == 0);
    ++state->reads;
}
typedef struct mutation_state { uint64_t first, second, replacement; int calls, second_fd; } mutation_state;
static void must_not_run(mulib_asyncio *loop, int fd, unsigned events, void *user) {
    (void)loop; (void)fd; (void)events; (void)user; assert(!"stale readiness dispatched");
}
static void mutate_callback(mulib_asyncio *loop, int fd, unsigned events, void *user) {
    mutation_state *s = (mutation_state *)user;
    (void)fd; (void)events;
    assert(mulib_asyncio_unwatch(loop, s->first) == 0);
    assert(mulib_asyncio_unwatch(loop, s->second) == 0);
    assert(mulib_asyncio_watch(loop, s->second_fd, MULIB_SELECT_READ, must_not_run, NULL, &s->replacement) == 0);
    ++s->calls;
}
static void reactor(void) {
    mulib_asyncio *loop;
    reactor_state state = {0, 0, 0, 0};
    mutation_state mutation = {0, 0, 0, 0, 0};
    int p[2], q[2], failure;
    uint64_t id = 999;
    size_t count;
    assert(!mulib_asyncio_create(0, 1) && errno == EINVAL);
    assert(!mulib_asyncio_create(1, 65537) && errno == EINVAL);
    assert(!mulib_asyncio_create(SIZE_MAX, 1) && errno == EINVAL);
    for (failure = 0; failure < 5; ++failure) {
        allocation_budget = failure;
        assert(!mulib_asyncio_create(1, 1) && errno == ENOMEM);
        assert(live_allocations == 0);
    }
    allocation_budget = -1;
    if (mulib_syscall_pipe(p) < 0) { assert(restricted("reactor pipe")); return; }
    assert(mulib_syscall_pipe(q) == 0);
    loop = mulib_asyncio_create(2, 1); assert(loop && live_allocations == 5);
    state.output = p[1];
    assert(mulib_asyncio_watch(loop, p[0], MULIB_SELECT_READ, read_callback, &state, &state.id) == 0);
    assert(mulib_asyncio_watch(loop, p[0], MULIB_SELECT_READ, read_callback, &state, &id) == -1 && errno == EEXIST && id == 999);
    assert(mulib_asyncio_watch(loop, -1, MULIB_SELECT_READ, read_callback, &state, &id) == -1);
    assert(mulib_asyncio_watch(loop, p[1], 8, read_callback, &state, &id) == -1);
    assert(mulib_asyncio_post(loop, write_job, &state) == 0);
    assert(mulib_asyncio_post(loop, next_job, &state) == -1 && errno == ENOSPC);
    assert(mulib_asyncio_run_once(loop, 1000) == 2 && state.jobs == 1 && state.reads == 1);
    assert(mulib_asyncio_run_once(loop, 1000) == 1 && state.jobs == 2);
    assert(mulib_asyncio_unwatch(loop, state.id) == -1 && errno == ENOENT);
    assert(mulib_asyncio_run_once(loop, -1) == -1 && errno == EINVAL);
    assert(mulib_asyncio_watch(loop, p[0], MULIB_SELECT_READ, mutate_callback, &mutation, &mutation.first) == 0);
    assert(mulib_asyncio_watch(loop, q[0], MULIB_SELECT_READ, must_not_run, NULL, &mutation.second) == 0);
    assert(mulib_asyncio_watch(loop, q[1], MULIB_SELECT_WRITE, must_not_run, NULL, &id) == -1 && errno == ENOSPC);
    mutation.second_fd = q[0];
    assert(mulib_syscall_write(p[1], "a", 1, &count) == 0);
    assert(mulib_syscall_write(q[1], "b", 1, &count) == 0);
    assert(mulib_asyncio_run_once(loop, 1000) == 1 && mutation.calls == 1);
    assert(mulib_asyncio_unwatch(loop, mutation.replacement) == 0);
    loop->next_id = UINT64_MAX;
    assert(mulib_asyncio_watch(loop, p[0], MULIB_SELECT_READ, must_not_run, NULL, &id) == 0 && id == UINT64_MAX);
    assert(mulib_asyncio_watch(loop, q[0], MULIB_SELECT_READ, must_not_run, NULL, &id) == -1 && errno == EOVERFLOW);
    assert(mulib_asyncio_destroy(loop) == 0 && live_allocations == 0);
    fd_flags(p[0]); /* loop destruction does not own or close a descriptor */
    assert(close(p[0]) == 0 && close(p[1]) == 0 && close(q[0]) == 0 && close(q[1]) == 0);
    assert(mulib_asyncio_destroy(NULL) == 0);
}
#endif

#if defined(__linux__)
static void counters_and_epoll(void) {
    int fd = mulib_eventfd_create(0, 0), epfd, semaphore;
    int pair[2];
    size_t transferred;
    char byte;
    uint64_t value = 17;
    mulib_epoll_event event;
    assert(mulib_eventfd_create(0, 2) == -1 && errno == EINVAL);
    if (fd < 0) { assert(restricted("eventfd")); return; }
    fd_flags(fd);
    assert(mulib_eventfd_read(fd, &value) == -1 && errno == EAGAIN && value == 17);
    assert(mulib_eventfd_read(fd, NULL) == -1 && errno == EINVAL);
    assert(mulib_eventfd_write(fd, UINT64_MAX) == -1 && errno == EINVAL);
    assert(mulib_eventfd_write(fd, UINT64_MAX - 1) == 0);
    assert(mulib_eventfd_write(fd, 1) == -1 && errno == EAGAIN);
    assert(mulib_eventfd_read(fd, &value) == 0 && value == UINT64_MAX - 1);
    assert(mulib_eventfd_write(fd, 0) == 0);
    semaphore = mulib_eventfd_create(2, 1); assert(semaphore >= 0);
    assert(mulib_eventfd_read(semaphore, &value) == 0 && value == 1);
    assert(mulib_eventfd_read(semaphore, &value) == 0 && value == 1);
    assert(mulib_eventfd_read(semaphore, &value) == -1 && errno == EAGAIN);
    assert(mulib_eventfd_close(semaphore) == 0);
    epfd = mulib_epoll_create();
    if (epfd < 0) { assert(restricted("epoll")); assert(mulib_eventfd_close(fd) == 0); return; }
    assert(fcntl(epfd, F_GETFD) & FD_CLOEXEC);
    assert(mulib_epoll_wait(epfd, &event, 0, 0) == -1 && errno == EINVAL);
    assert(mulib_epoll_wait(epfd, &event, 257, 0) == -1 && errno == EINVAL);
    assert(mulib_epoll_wait(epfd, &event, 1, -2) == -1 && errno == EINVAL);
    assert(mulib_epoll_add(epfd, fd, MULIB_EPOLL_ERROR, 0) == -1 && errno == EINVAL);
    assert(mulib_epoll_add(epfd, fd, MULIB_EPOLL_READ | MULIB_EPOLL_ONESHOT, UINT64_MAX) == 0);
    assert(mulib_epoll_add(epfd, fd, MULIB_EPOLL_READ, 0) == -1 && errno == EEXIST);
    assert(mulib_epoll_wait(epfd, &event, 1, 0) == 0);
    assert(mulib_eventfd_write(fd, 5) == 0);
    assert(mulib_epoll_wait(epfd, &event, 1, 1000) == 1 && event.token == UINT64_MAX && (event.events & MULIB_EPOLL_READ));
    assert(mulib_epoll_wait(epfd, &event, 1, 0) == 0);
    assert(mulib_epoll_modify(epfd, fd, MULIB_EPOLL_READ, 3) == 0);
    assert(mulib_epoll_wait(epfd, &event, 1, 1000) == 1 && event.token == 3);
    assert(mulib_eventfd_read(fd, &value) == 0 && value == 5);
    assert(mulib_epoll_remove(epfd, fd) == 0);
    assert(mulib_epoll_remove(epfd, fd) == -1 && errno == ENOENT);
    assert(mulib_syscall_pipe(pair) == 0);
    assert(mulib_epoll_add(epfd, pair[0], MULIB_EPOLL_READ | MULIB_EPOLL_EDGE, 17) == 0);
    assert(mulib_syscall_write(pair[1], "p", 1, &transferred) == 0 && transferred == 1);
    assert(mulib_epoll_wait(epfd, &event, 1, 1000) == 1 && event.token == 17 && (event.events & MULIB_EPOLL_READ));
    assert(mulib_syscall_read(pair[0], &byte, 1, &transferred) == 0 && byte == 'p');
    assert(mulib_epoll_wait(epfd, &event, 1, 0) == 0);
    assert(mulib_syscall_close(pair[1]) == 0);
    assert(mulib_epoll_wait(epfd, &event, 1, 1000) == 1 && (event.events & MULIB_EPOLL_HANGUP));
    assert(mulib_epoll_remove(epfd, pair[0]) == 0 && mulib_syscall_close(pair[0]) == 0);
    assert(mulib_epoll_close(epfd) == 0); fd_flags(fd);
    assert(mulib_eventfd_close(fd) == 0);
    assert(mulib_eventfd_read(fd, &value) == -1 && errno == EBADF);
}
static void timers(void) {
    int fd = mulib_timerfd_create();
    uint64_t count = 7;
    if (fd < 0) { assert(restricted("timerfd")); return; }
    fd_flags(fd);
    assert(mulib_timerfd_read(fd, &count) == -1 && errno == EAGAIN && count == 7);
    assert(mulib_timerfd_set(fd, UINT64_MAX, 0) == -1 && errno == EOVERFLOW);
    assert(mulib_timerfd_set(fd, 0, UINT64_MAX) == -1 && errno == EOVERFLOW);
    assert(mulib_timerfd_set(fd, 1000000, 1000000) == 0);
    assert(ready(fd, MULIB_SELECT_READ, 2000));
    assert(mulib_timerfd_read(fd, &count) == 0 && count >= 1);
    assert(mulib_timerfd_set(fd, 0, 0) == 0);
    assert(mulib_timerfd_read(fd, &count) == -1 && errno == EAGAIN);
    assert(mulib_timerfd_close(fd) == 0);
}
static int directory(char *path, size_t capacity) {
    unsigned i;
    for (i = 0; i < 100; ++i) {
        int n = snprintf(path, capacity, "mulib-os-events-%ld-%u", (long)getpid(), i);
        assert(n > 0 && (size_t)n < capacity);
        if (mkdir(path, 0700) == 0) return 1;
        if (errno != EEXIST) { assert(restricted("local test directory")); return 0; }
    }
    fprintf(stderr, "SKIP local filesystem: all unique test names occupied\n"); return 0;
}
static void filesystem(void) {
    char path[128], child[160], moved[160], buffer[4096];
    int fd = mulib_fswatch_create(), watch, file;
    size_t count, offset = 0;
    unsigned found = 0;
    uint32_t from_cookie = 0, to_cookie = 0;
    mulib_fswatch_event event;
    if (fd < 0) { assert(restricted("inotify")); return; }
    fd_flags(fd);
    if (!directory(path, sizeof path)) { assert(mulib_fswatch_close(fd) == 0); return; }
    assert(mulib_fswatch_add(fd, path, 0) == -1 && errno == EINVAL);
    assert(mulib_fswatch_add(fd, path, MULIB_FSWATCH_OVERFLOW) == -1 && errno == EINVAL);
    watch = mulib_fswatch_add(fd, path, MULIB_FSWATCH_WATCH_MASK);
    if (watch < 0) {
        assert(restricted("inotify watch")); assert(mulib_fswatch_close(fd) == 0); assert(rmdir(path) == 0); return;
    }
    assert(mulib_fswatch_read(fd, buffer, 15, &count) == -1 && errno == EINVAL);
    count = 91;
    assert(mulib_fswatch_read(fd, buffer, sizeof buffer, &count) == -1 && errno == EAGAIN && count == 91);
    assert(snprintf(child, sizeof child, "%s/item", path) > 0);
    assert(snprintf(moved, sizeof moved, "%s/moved", path) > 0);
    file = open(child, O_WRONLY | O_CREAT | O_EXCL, 0600); assert(file >= 0);
    assert(write(file, "x", 1) == 1 && close(file) == 0);
    assert(rename(child, moved) == 0 && unlink(moved) == 0);
    assert(ready(fd, MULIB_SELECT_READ, 1000));
    assert(mulib_fswatch_read(fd, buffer, sizeof buffer, &count) == 0);
    while (mulib_fswatch_next(buffer, count, &offset, &event) == 1) {
        assert(event.watch == watch && event.name_length > 0);
        found |= event.mask;
        if (event.mask & MULIB_FSWATCH_MOVED_FROM) from_cookie = event.cookie;
        if (event.mask & MULIB_FSWATCH_MOVED_TO) to_cookie = event.cookie;
    }
    assert(offset == count && from_cookie && from_cookie == to_cookie);
    assert((found & (MULIB_FSWATCH_CREATE | MULIB_FSWATCH_MODIFY | MULIB_FSWATCH_CLOSE_WRITE | MULIB_FSWATCH_DELETE)) ==
        (MULIB_FSWATCH_CREATE | MULIB_FSWATCH_MODIFY | MULIB_FSWATCH_CLOSE_WRITE | MULIB_FSWATCH_DELETE));
    assert(mulib_fswatch_remove(fd, watch) == 0);
    assert(ready(fd, MULIB_SELECT_READ, 1000));
    assert(mulib_fswatch_read(fd, buffer, sizeof buffer, &count) == 0); offset = 0;
    assert(mulib_fswatch_next(buffer, count, &offset, &event) == 1 && (event.mask & MULIB_FSWATCH_IGNORED));
    assert(mulib_fswatch_close(fd) == 0 && rmdir(path) == 0);
}
typedef struct native_state { int kind, calls; uint64_t id; } native_state;
static void native_callback(mulib_asyncio *loop, int fd, unsigned events, void *user) {
    native_state *state = (native_state *)user;
    uint64_t value;
    assert(events & MULIB_SELECT_READ);
    if (state->kind == 0) assert(mulib_eventfd_read(fd, &value) == 0 && value == 3);
    else if (state->kind == 1) assert(mulib_timerfd_read(fd, &value) == 0 && value >= 1);
    else {
        char buffer[4096]; size_t length, offset = 0; mulib_fswatch_event event;
        assert(mulib_fswatch_read(fd, buffer, sizeof buffer, &length) == 0);
        assert(mulib_fswatch_next(buffer, length, &offset, &event) == 1 && (event.mask & MULIB_FSWATCH_CREATE));
    }
    ++state->calls; assert(mulib_asyncio_unwatch(loop, state->id) == 0);
}
static void native_reactor(void) {
    int counter = mulib_eventfd_create(0, 0), timer = -1, fs = -1, watch, file, i;
    char path[128], child[160];
    mulib_asyncio *loop;
    native_state state[3] = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    if (counter < 0) { assert(restricted("reactor eventfd")); return; }
    timer = mulib_timerfd_create();
    if (timer < 0) { assert(restricted("reactor timerfd")); assert(close(counter) == 0); return; }
    fs = mulib_fswatch_create();
    if (fs < 0) { assert(restricted("reactor inotify")); assert(close(counter) == 0 && close(timer) == 0); return; }
    if (!directory(path, sizeof path)) { assert(close(counter) == 0 && close(timer) == 0 && close(fs) == 0); return; }
    watch = mulib_fswatch_add(fs, path, MULIB_FSWATCH_CREATE);
    if (watch < 0) {
        assert(restricted("reactor watch")); assert(close(counter) == 0 && close(timer) == 0 && close(fs) == 0 && rmdir(path) == 0); return;
    }
    loop = mulib_asyncio_create(3, 1); assert(loop);
    assert(mulib_asyncio_watch(loop, counter, MULIB_SELECT_READ, native_callback, &state[0], &state[0].id) == 0);
    assert(mulib_asyncio_watch(loop, timer, MULIB_SELECT_READ, native_callback, &state[1], &state[1].id) == 0);
    assert(mulib_asyncio_watch(loop, fs, MULIB_SELECT_READ, native_callback, &state[2], &state[2].id) == 0);
    assert(mulib_eventfd_write(counter, 3) == 0 && mulib_timerfd_set(timer, 1000000, 0) == 0);
    assert(snprintf(child, sizeof child, "%s/item", path) > 0);
    file = open(child, O_WRONLY | O_CREAT | O_EXCL, 0600); assert(file >= 0 && close(file) == 0);
    for (i = 0; i < 4 && state[0].calls + state[1].calls + state[2].calls < 3; ++i)
        assert(mulib_asyncio_run_once(loop, 1000) >= 0);
    assert(state[0].calls == 1 && state[1].calls == 1 && state[2].calls == 1);
    assert(mulib_asyncio_destroy(loop) == 0 && live_allocations == 0);
    fd_flags(counter); fd_flags(timer); fd_flags(fs);
    assert(close(counter) == 0 && close(timer) == 0 && close(fs) == 0);
    assert(unlink(child) == 0 && rmdir(path) == 0);
}
#endif
int main(void) {
    signals_and_clocks(); decoder();
#if defined(__unix__) || defined(__APPLE__)
    pipes_and_select(); loopback(); reactor();
#else
    { int pair[2]; assert(mulib_syscall_pipe(pair) == -1 && errno == ENOSYS); }
    assert(mulib_select_limit() == -1 && errno == ENOSYS);
    assert(!mulib_asyncio_create(1, 1) && errno == ENOSYS);
#endif
#if defined(__linux__)
    counters_and_epoll(); timers(); filesystem(); native_reactor();
#else
    assert(mulib_epoll_create() == -1 && errno == ENOSYS);
    assert(mulib_eventfd_create(0, 0) == -1 && errno == ENOSYS);
    assert(mulib_timerfd_create() == -1 && errno == ENOSYS);
    assert(mulib_fswatch_create() == -1 && errno == ENOSYS);
#endif
    assert(live_allocations == 0); return 0;
}
