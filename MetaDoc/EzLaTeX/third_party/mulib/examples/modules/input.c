#define MULIB_IMPL_INPUT
#include "mulib-input.h"
#include <assert.h>
int main(void) {
    mulib_input_t device = MULIB_INPUT_INIT;
    mulib_input_event_t press = {0,MULIB_INPUT_KEY,30,1};
    assert(mulib_input_event_valid(&press));
    assert(mulib_input_close(&device) == MULIB_INPUT_OK);
    /* Explicit native use: open a chosen /dev/input/eventN, then read until AGAIN. */
    return 0;
}
