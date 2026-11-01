# MuLIB TUI

`include/mulib-tui.h` is a small immediate-mode UI and double-buffered terminal
renderer. It provides clipped text, boxes, buttons, row/column splits, event
activation helpers and transactional changed-cell output. Applications own I/O,
terminal modes, the event loop, widget identity/focus and resize notification.

## Integration and layering

Define these implementation switches **before any include**, in one translation
unit:

```c
#define MULIB_IMPL_GRID
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TUI
/* Also define MULIB_IMPL_TERMKEY if decoding input in this implementation TU. */
```

TUI uses vterm's validated cell/style model and grid storage, utf8's decoding,
strbuf's owned output, terminfo's compiled/builtin capabilities and numeric
expansion, and termkey's public event values. **TERMKEY implementation is not
needed merely to render or inspect supplied events.** No dependency switch is
implicitly defined by a header. Declarations are C++17 compatible; implementation
is ISO C11 with no external libraries, termios, curses, PTY or global state.

All Unicode scalars occupy **one cell**, exactly as in vterm. This renderer does
not know real terminal display widths or combining behavior; use single-column
text when actual terminal geometry must match. It is not a general Unicode
layout engine, retained widget tree, or replacement for a full terminal toolkit.

## Lifecycle and drawing API

`mulib_tui_init(ui,w,h)` initializes fresh storage with two buffers, default
spaces and a hidden desired cursor; failure leaves a freeable object. Screen
limits are vterm's nonzero dimensions and at most 1048576 cells. Do not re-init a
live object or copy its owned pointers. `mulib_tui_free` releases both buffers
and zeroes state. `mulib_tui_resize` is transactional: it preserves the back
buffer's top-left overlap, fills new cells with default spaces, clamps the desired
cursor and invalidates the front buffer. New sizes require the application to
resize its actual terminal/test emulator separately. Other vterm parser state
in `back` is not retained by TUI resize.

`mulib_tui_rect_t` is `{x,y,width,height}` in zero-based cells. Drawing clips to
screen bounds without computing an overflowing x+width/y+height. Widgets operate
on the **clipped** rectangle; an oversized border is redrawn at the clipped edge,
not treated as a larger offscreen widget. Members of owned UI state are for
inspection, not direct mutation.

| Function | Contract |
| --- | --- |
| `mulib_tui_cell(ui,x,y)` | Borrow back cell, NULL out of bounds; resize/free invalidates pointer |
| `mulib_tui_put(ui,x,y,cell)` | Validate and write an in-bounds cell, 1/0 |
| `mulib_tui_fill(ui,rect,cell)` | Validated clipped fill; empty/outside succeeds |
| `mulib_tui_clear(ui,style)` | Fill back buffer with spaces in style |
| `mulib_tui_text(ui,rect,text,n,style)` | Validate entire UTF-8 input/style first, then clipped/wrapped drawing |
| `mulib_tui_box(ui,rect,style)` | ASCII `+ - |` border, including narrow/short rectangles |
| `mulib_tui_button(ui,rect,label,n,focused,style)` | Fill/box and center single-line label; focused ORs inverse into style |
| `mulib_tui_button_event(rect,event,focused)` | Boolean activation helper, no state changes |
| `mulib_tui_split(rect,vertical,first,gap,a,b)` | Columns for vertical=0, rows otherwise; first extent and explicit gap |
| `mulib_tui_cursor(ui,x,y,visible)` | Desired in-bounds cursor, 1/0 |
| `mulib_tui_invalidate(ui)` | Force next render to repaint every cell |
| `mulib_tui_render(ui,info,output)` | Transactionally append output and update front buffer, 1/0 |

Text wraps one scalar per cell. LF starts the next row (an LF immediately after
a full row does not double-wrap), CR is ignored, and TAB fills spaces to the
next four-column stop relative to the rectangle. Other ASCII/C1 controls become
U+FFFD; embedded NUL is accepted by the explicit byte length and replaced, not
interpreted as a string terminator. Invalid UTF-8 or style returns 0 without
changing cells. NULL text is allowed for length zero.

Button labels must be valid UTF-8 without control scalars. Content is drawn on
the middle interior row; overlong labels are clipped, not scrolled. Dimensions
smaller than 3x3 get a border/fill without a label. Activation requires either
unmodified Enter/Space with `focused!=0`, or an unmodified left-button mouse
press inside the supplied rectangle. Hit testing is not screen-clipped: pass
your actual on-screen rectangle. No implicit focus, tab navigation, release
tracking or click debouncing is provided.

Split requires nonoverflowing rectangle endpoints, `first+gap <= extent`, and
distinct non-NULL output pointers. On failure outputs stay unchanged. Drawing
returns 1 on success (including an empty clip) or 0 for invalid arguments/style/
UTF-8; no `errno` contract. Nonallocating drawing performs validation before
mutation. State must not be concurrently mutated.

## Renderer and terminal assumptions

`mulib_tui_render(ui, info, output)` appends to an initialized strbuf.
`info=NULL` explicitly selects MuLIB's ANSI/256-color fallback. Passing a loaded
compiled record instead uses its supported core capabilities; painting requires
`cup`, `sgr0` and `op`, plus `civis`/`cnorm` when cursor visibility changes. It
uses attribute capabilities (`bold dim sitm smul blink rev invis smxx`) and
`setaf`/`setab` only when requested by a changed cell. Indexed colors are checked
against `colors` when that capability is present. **RGB and strikeout are only
available with the explicit ANSI fallback**, since no extended terminfo table
is read. Missing capabilities or unsupported tparm syntax produce a 0 result,
not silently degraded styling. No padding delay is performed.

`sgr0` is relied on only for attribute reset: the renderer also emits `op` to
restore both default colors at the start/end of painting and before a new style.
It then reapplies every requested nondefault foreground/background color, since
restoring the pair resets both channels. Even a default-only repaint requires
`op`; a missing or unsupported `op` fails atomically rather than assuming that
`sgr0` also resets colors. Unchanged or cursor-only renders need no color reset.

Each changed contiguous row run is cursor-addressed, styles are reset/reapplied
when needed, and scalars are encoded with MuLIB utf8. Unchanged cells are skipped.
A first/invalidation render writes every cell, including spaces. Successful
rendering leaves default rendition and applies the desired cursor visibility/
position (position is meaningful only when visible). An unchanged screen and
cursor generate zero additional bytes. Hidden-cursor position changes need not
emit output. This is changed-cell output, not an optimal minimal-byte diff.

The target terminal must support UTF-8, ordinary absolute cursor addressing,
the selected profile's capabilities, and **delayed** right-margin autowrap.
The renderer does not change origin/insert/scroll-margin modes, enter an
alternate screen, change terminal modes, probe features, or restore application
state on exit. The application must establish normal full-screen positioning
and output mode first. General non-ANSI/early-wrap terminals are not supported
merely by supplying their compiled terminfo record.

Output is first built in a temporary strbuf, limited to 64 MiB per render. Only
after successful append is the front buffer committed. On any allocation,
capability, style/expansion, or limit failure, **both the caller's output and
the front buffer remain unchanged**, so rendering can be retried. `info` and
output must not alias UI storage. String building may allocate, drawing does not.

Successful render means bytes were generated, **not delivered**. The caller
must write all newly appended bytes. If the write fails/partially succeeds,
discard the failed batch, call `mulib_tui_invalidate`, and render again after
transport recovery. Invalidate after any external output, terminal clear,
capability-profile change or lost screen state. The UI does not automatically
detect such changes or retain a transport acknowledgment protocol.

## Runnable, noninteractive example

```c
#include <assert.h>
#define MULIB_IMPL_GRID
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TUI
#include "mulib-tui.h"
int main(void) {
    mulib_tui_t ui;
    mulib_vterm_t replay;
    mulib_strbuf_t out;
    mulib_tui_rect_t rect = {1, 1, 10, 3};
    size_t old;
    assert(mulib_tui_init(&ui, 16, 5));
    assert(mulib_vterm_init(&replay, 16, 5));
    mulib_strbuf_init(&out);
    assert(mulib_tui_button(&ui, rect, "OK", 2, 1, mulib_term_style_default()));
    assert(mulib_tui_render(&ui, NULL, &out));
    assert(mulib_vterm_feed(&replay, out.data, out.length));
    assert(mulib_term_cell_equal(*mulib_tui_cell(&ui, 5, 2),
                                 *mulib_vterm_cell(&replay, 5, 2)));
    old = out.length;
    assert(mulib_tui_render(&ui, NULL, &out) && out.length == old);
    mulib_strbuf_free(&out);
    mulib_vterm_free(&replay);
    mulib_tui_free(&ui);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/tui.c -o ./tui-example
./tui-example
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude tests/test_terminal.c -o ./test-terminal
./test-terminal
```

Tests compare the renderer's cell/style result against an independently fed
vterm screen, including the bottom-right cell, unchanged renders, missing-capability
failures, a compiled profile whose `sgr0` preserves colors, missing/malformed `op`
atomicity, cursor-only updates, invalidation, clipping, widgets, input events,
resize and layout-overflow checks. No interactive terminal is required.
