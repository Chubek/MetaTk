#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#define MULIB_IMPL_PERF
#include "mulib-perf.h"
int main(void) {
    uint64_t begin, end, elapsed;
    if (mulib_perf_now(&begin) < 0) {
        if (errno == ENOSYS || errno == EPERM || errno == EACCES) {
            perror("SKIP monotonic clock unavailable"); return 0;
        }
        assert(0);
    }
    assert(mulib_perf_now(&end) == 0 && end >= begin);
    assert(mulib_perf_elapsed(begin, end, &elapsed) == 0 && elapsed == end - begin);
    assert(mulib_perf_elapsed(2, 1, &elapsed) == -1 && errno == EINVAL);
    return 0;
}
