#include <assert.h>
#include <errno.h>
#include <stdio.h>
#define MULIB_IMPL_ASYNCIO
#define MULIB_IMPL_SELECT
#define MULIB_IMPL_SYSCALL
#include "mulib-asyncio.h"
#include "mulib-syscall.h"
typedef struct example_state { int writer, calls; uint64_t watch; } example_state;
static void send_job(mulib_asyncio *loop, void *user) {
    example_state *s = (example_state *)user; size_t count;
    (void)loop;
    assert(mulib_syscall_write(s->writer, "!", 1, &count) == 0 && count == 1);
}
static void receive_ready(mulib_asyncio *loop, int fd, unsigned events, void *user) {
    example_state *s = (example_state *)user; size_t count; char byte;
    assert(events & MULIB_SELECT_READ);
    assert(mulib_syscall_read(fd, &byte, 1, &count) == 0 && count == 1 && byte == '!');
    assert(mulib_asyncio_unwatch(loop, s->watch) == 0); ++s->calls;
}
int main(void) {
    int pair[2]; mulib_asyncio *loop; example_state state = {0, 0, 0};
    if (mulib_syscall_pipe(pair) < 0) {
        if (errno == ENOSYS || errno == EPERM || errno == EACCES || errno == EMFILE || errno == ENFILE) {
            perror("SKIP reactor pipe unavailable"); return 0;
        }
        assert(0);
    }
    loop = mulib_asyncio_create(1, 1); assert(loop); state.writer = pair[1];
    assert(mulib_asyncio_watch(loop, pair[0], MULIB_SELECT_READ, receive_ready, &state, &state.watch) == 0);
    assert(mulib_asyncio_post(loop, send_job, &state) == 0);
    assert(mulib_asyncio_run_once(loop, 1000) == 2 && state.calls == 1);
    assert(mulib_asyncio_destroy(loop) == 0);
    assert(mulib_syscall_close(pair[0]) == 0 && mulib_syscall_close(pair[1]) == 0);
    return 0;
}
