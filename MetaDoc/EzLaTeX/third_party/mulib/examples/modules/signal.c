#include <assert.h>
#include <signal.h>
#define MULIB_IMPL_SIGNAL
#include "mulib-signal.h"
static volatile sig_atomic_t notified;
static void handler(int number) { notified = number; }
int main(void) {
    mulib_signal_guard guard = {0, 0, 0};
    assert(mulib_signal_install(&guard, SIGINT, handler) == 0);
    assert(raise(SIGINT) == 0 && notified == SIGINT);
    assert(mulib_signal_restore(&guard) == 0);
    assert(mulib_signal_restore(&guard) == 0);
    return 0;
}
