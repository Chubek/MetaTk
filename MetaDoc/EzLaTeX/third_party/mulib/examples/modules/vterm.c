#include <assert.h>
#include <string.h>
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#include "mulib-vterm.h"

int main(void) {
    mulib_vterm_t screen;
    const char *sequence = "Hi\033[2;3H\033[1;31m\xe2\x98\x83";
    size_t i;
    assert(mulib_vterm_init(&screen, 12, 3));
    for (i = 0; i < strlen(sequence); ++i) assert(mulib_vterm_feed(&screen, sequence + i, 1));
    assert(mulib_vterm_cell(&screen, 0, 0)->codepoint == 'H');
    assert(mulib_vterm_cell(&screen, 2, 1)->codepoint == 0x2603);
    assert(mulib_vterm_cell(&screen, 2, 1)->style.foreground == 1);
    assert(mulib_vterm_cell(&screen, 2, 1)->style.attributes & MULIB_TERM_BOLD);
    assert(mulib_vterm_feed(&screen, "\033[2K", 4));
    assert(mulib_vterm_cell(&screen, 2, 1)->codepoint == ' ');
    mulib_vterm_free(&screen);
    return 0;
}
