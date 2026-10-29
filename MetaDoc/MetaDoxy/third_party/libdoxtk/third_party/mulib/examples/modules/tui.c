#include <assert.h>
#define MULIB_IMPL_GRID
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_TERMKEY
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TUI
#include "mulib-tui.h"

int main(void) {
    mulib_tui_t ui;
    mulib_vterm_t replay;
    mulib_strbuf_t output;
    mulib_termkey_t keys;
    mulib_termkey_event_t event;
    mulib_tui_rect_t button = {1, 1, 10, 3};
    size_t previous;
    assert(mulib_tui_init(&ui, 16, 5));
    assert(mulib_vterm_init(&replay, 16, 5));
    mulib_strbuf_init(&output);
    assert(mulib_tui_button(&ui, button, "OK", 2, 1, mulib_term_style_default()));
    assert(mulib_tui_render(&ui, NULL, &output));
    assert(mulib_vterm_feed(&replay, output.data, output.length));
    assert(mulib_term_cell_equal(*mulib_tui_cell(&ui, 5, 2), *mulib_vterm_cell(&replay, 5, 2)));
    previous = output.length;
    assert(mulib_tui_render(&ui, NULL, &output) && output.length == previous);
    mulib_termkey_init(&keys);
    assert(mulib_termkey_feed(&keys, "\r", 1) == 1);
    assert(mulib_termkey_next(&keys, &event, 0) == 1);
    assert(mulib_tui_button_event(button, &event, 1));
    mulib_strbuf_free(&output);
    mulib_vterm_free(&replay);
    mulib_tui_free(&ui);
    return 0;
}
