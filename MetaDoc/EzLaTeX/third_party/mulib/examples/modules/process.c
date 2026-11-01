#include <assert.h>
#include <errno.h>
#include <string.h>
#define MULIB_IMPL_PROCESS
#include "mulib-process.h"
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
int main(void) {
    const char *args[] = {"echo", "literal;not-a-shell-command"};
    mulib_process_t child;
    mulib_process_status_t status;
    mulib_process_io_t io = {-1,-1,-1};
    int fds[2];
    char text[128];
    size_t used = 0;
    assert(!mulib_process_pipe(fds));
    mulib_process_init(&child); io.stdout_fd = fds[1];
    assert(!mulib_process_spawn(&child, "/bin/echo", args, 2, &io));
    assert(!mulib_process_close(fds[1]));
    for (;;) {
        ssize_t n = read(fds[0], text + used, sizeof(text) - used);
        if (n < 0 && errno == EINTR) continue;
        assert(n >= 0);
        if (!n) break;
        used += (size_t)n; assert(used < sizeof(text));
    }
    assert(used == strlen(args[1]) + 1 && !memcmp(text, args[1], used - 1));
    assert(!mulib_process_close(fds[0]));
    assert(!mulib_process_wait(&child, &status, 0) && status.exited && !status.exit_code);
    return 0;
}
#else
int main(void) {
    int fds[2];
    assert(mulib_process_pipe(fds) == MULIB_PROCESS_UNSUPPORTED);
    return 0;
}
#endif
