#include <assert.h>
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_TERMKEY
#include "mulib-termkey.h"

int main(void) {
    mulib_termkey_t decoder;
    mulib_termkey_event_t event;
    mulib_termkey_init(&decoder);
    assert(mulib_termkey_feed(&decoder, "\033[1;", 4) == 4);
    assert(mulib_termkey_next(&decoder, &event, 0) == 0);
    assert(mulib_termkey_feed(&decoder, "5A", 2) == 2);
    assert(mulib_termkey_next(&decoder, &event, 0) == 1);
    assert(event.key == MULIB_TERMKEY_UP && event.modifiers == MULIB_TERMKEY_CTRL);
    assert(mulib_termkey_feed(&decoder, "\xe2\x82", 2) == 2);
    assert(mulib_termkey_next(&decoder, &event, 0) == 0);
    assert(mulib_termkey_feed(&decoder, "\xac", 1) == 1);
    assert(mulib_termkey_next(&decoder, &event, 0) == 1 && event.codepoint == 0x20ac);
    assert(mulib_termkey_feed(&decoder, "\033", 1) == 1);
    assert(mulib_termkey_next(&decoder, &event, 0) == 0);
    assert(mulib_termkey_next(&decoder, &event, 1) == 1 && event.key == MULIB_TERMKEY_ESCAPE);
    return 0;
}
