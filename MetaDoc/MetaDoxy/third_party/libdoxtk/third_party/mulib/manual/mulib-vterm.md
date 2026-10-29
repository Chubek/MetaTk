# MuLIB vterm

`include/mulib-vterm.h` implements a bounded in-memory terminal screen emulator
and the cell/style value types shared with MuLIB TUI. Bytes can be supplied in
arbitrary fragments; there is no PTY, terminal access, or output callback.

## Integration, ownership and limits

Define `MULIB_IMPL_VTERM`, `MULIB_IMPL_GRID` and `MULIB_IMPL_UTF8` in one
translation unit before including headers. Declarations are C++17 compatible;
implementation uses ISO C11 and only MuLIB/standard-library functions.

* `mulib_vterm_init(screen, width, height)` initializes **fresh/uninitialized**
  storage. Both dimensions must be nonzero, at most `INT_MAX`, and the screen
  must contain at most `MULIB_VTERM_MAX_CELLS` (1048576) cells. Grid allocation
  also checks byte-size multiplication. Failure leaves a zero/freeable object.
  Calling init again on a live object leaks; use resize/reset instead.
* `mulib_vterm_free` releases the grid and zeroes state; repeated free is safe.
  Do not copy live owned screens by value.
* `mulib_vterm_resize` allocates transactionally, copies the top-left overlap,
  initializes new cells to default-style spaces, clamps current/saved cursor,
  resets margins to the full screen and clears delayed wrap. Other parser,
  rendition and mode state is retained. Failure leaves the old screen intact.
* `mulib_vterm_reset` clears cells to default spaces, cursor/saved cursor to
  (0,0), rendition to default, margins to full-screen, autowrap/cursor visibility
  on, origin mode off, and clears parser/counters without reallocating.
* Treat public members as read-only. Borrowed cells from `mulib_vterm_cell` stay
  at the same address until resize/free, but their contents can change on feed.
  No operation is implicitly thread-safe for a shared mutable screen.

## Shared cells and drawing APIs

`mulib_term_cell_t` contains a Unicode `codepoint` and `mulib_term_style_t`.
Style contains `foreground`, `background`, and `attributes`:

* Colors 0..255 are indexed; `MULIB_TERM_COLOR_DEFAULT` means terminal default.
  `MULIB_TERM_RGB(r,g,b)` tags a 24-bit RGB value (components masked to 8 bits).
* Attributes: `MULIB_TERM_BOLD`, `DIM`, `ITALIC`, `UNDERLINE`, `BLINK`, `INVERSE`,
  `CONCEAL`, `STRIKE` (all prefixed `MULIB_TERM_`).
* `mulib_term_style_default()` constructs default colors/no attributes.
  `mulib_term_cell_valid(cell)` validates scalar/color/attribute values and
  excludes ASCII/C1 control scalars. `mulib_term_cell_equal(a,b)` compares values
  without comparing struct padding. A blank cell is a space, never NUL.

Every Unicode scalar occupies **exactly one column**, including combining marks,
emoji and East Asian characters. There is no wcwidth table, combining sequence,
normalization, grapheme or double-width continuation-cell support. This is a
deliberate simple screen model, not Unicode display-width conformance.

| API | Behavior |
| --- | --- |
| `mulib_vterm_cell(screen,x,y)` | Borrow a const cell, NULL out of bounds |
| `mulib_vterm_set_cell(screen,x,y,cell)` | Validated in-bounds write, 1/0 |
| `mulib_vterm_fill(screen,x,y,w,h,cell)` | Validated clipped rectangle; empty/outside succeeds, no coordinate-addition overflow |
| `mulib_vterm_move(screen,x,y)` | Absolute in-bounds cursor move, clears delayed wrap, 1/0; API coordinates ignore origin mode |
| `mulib_vterm_scroll(screen,lines)` | Scroll current margins; positive up/negative down, clamped to region size; safe for INT_MIN |
| `mulib_vterm_feed(screen,bytes,n)` | Consume all n bytes; 1 success, 0 invalid arguments/uninitialized screen |
| `mulib_vterm_finish(screen)` | Flush truncated UTF-8 as one replacement and discard incomplete escapes |

Drawing does not move the parser cursor. Erase/edit/scroll operations fill spaces
using the **current rendition**, including background. Reset and new resize
cells instead use default rendition.

## Supported terminal sequences

Coordinates in terminal sequences are one-based, unlike the C API. Zero/missing
motion parameters use the conventional default 1; motions are clamped. Rendering
uses delayed autowrap: writing the last column sets a pending wrap, and the next
printable scalar wraps/scrolls unless a cursor control cancels it. SGR preserves
pending wrap. LF does **not** imply CR. Tabs are fixed every eight columns.

* Controls: CR, LF/VT/FF (index), BS, HT. BEL/NUL/DEL and other unhandled C0
  bytes are ignored. Encoded C1 control scalars become U+FFFD; raw 8-bit C1
  control sequences are not recognized.
* ESC `D` index, `E` next line, `M` reverse index, `7`/`8` save/restore
  **coordinates and rendition only**, `c` full reset. `=`/`>` are accepted but
  have no screen effect. Character-set/intermediate designations `(`, `)`, `*`,
  `+`, `%`, `#` consume one following byte and are ignored (no DEC graphics set).
* CSI `A B C D` relative cursor motions; `E F` next/previous line at column 1;
  `G`/backtick absolute column; `d` absolute row; `H f` row/column; `a e` relative
  right/down aliases. Relative upward/downward movement stops at the applicable
  scrolling margin even with origin mode off; if already beyond that margin,
  the physical screen edge is the limit instead. Origin mode constrains vertical
  movement to both margins and makes absolute sequence rows region-relative.
  An absolute C API move outside that region is clamped only by a later vertical
  motion, not by horizontal motion, erase, editing, scrolling or rendition.
* CSI `J` erase display modes 0,1,2 and `K` erase line modes 0,1,2; neither homes
  the cursor. ED 3 is unsupported (there is no scrollback).
* CSI `@ P X` insert/delete/erase characters; `L M` insert/delete lines from the
  cursor through the lower scroll margin if the cursor is in the region;
  `S T` scroll the current region up/down.
* CSI `top;bottom r` sets a valid scrolling region with top < bottom and homes
  the cursor. CSI `s u` shares coordinate/rendition save/restore with ESC 7/8.
* DEC private CSI `?6h/l` origin mode, `?7h/l` autowrap, `?25h/l` cursor visibility.
* SGR: reset 0; 1,2,3,4,5,7,8,9 enable supported attribute bits; 22..25,27..29
  disable them; 30..37/40..47 and 90..97/100..107 indexed basic/bright colors;
  39/49 default colors; `38/48;5;index` and `38/48;2;r;g;b` colors (0..255).
  Colon subparameters/underline variants are unsupported. An invalid extended
  color group stops processing the remainder of that SGR command.

Unknown sequences/modes/SGR parameters are ignored, not implemented by guesswork.
OSC payloads are ignored through BEL or ESC-backslash; DCS/APC/PM/SOS payloads
through ESC-backslash. No titles, clipboard operations, hyperlinks, image
protocols, device reports, terminal replies, or escape-string actions are run.
No alternate-screen buffer, scrollback, insert mode, programmable tabs,
double-width lines, selective erase, protected fields or full DEC conformance.

## Incremental parser and errors

UTF-8 is assembled across feeds and validated/decoded by MuLIB utf8. Invalid
leading bytes produce U+FFFD; an invalid assembled encoding produces one
replacement for that assembly. A non-continuation interrupt produces a
replacement then reprocesses the interrupting byte. `finish` replaces any
remaining partial scalar once; unlike termkey this is a screen-oriented policy.

CSI storage is 256 bytes, with at most 32 semicolon parameters of at most 1000000.
Overlong CSI is discarded through its final byte so its payload does not leak
onto the screen. Escape strings allocate no payload storage; after 4096 bytes
the parser records a limit error and keeps discarding through their terminator.
CAN/SUB cancel an active escape/string. ESC restarts an incomplete non-string
escape. Fragmented ESC-backslash terminators work.

`invalid_input` counts malformed UTF-8, malformed numeric/overlong escapes and
unfinished state flushed by `finish`. `ignored_sequences` is a diagnostic count
of ignored actions/parameters (not necessarily one per complete sequence).
Unsupported input is consumed and does not make `feed` fail; there is no
`errno` contract. Feed performs no allocation after initialization.

## Runnable example

```c
#include <assert.h>
#define MULIB_IMPL_GRID
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_VTERM
#include "mulib-vterm.h"
int main(void) {
    mulib_vterm_t screen;
    assert(mulib_vterm_init(&screen, 10, 3));
    assert(mulib_vterm_feed(&screen, "\033[2;3H\033[31mX", 12));
    assert(mulib_vterm_cell(&screen, 2, 1)->codepoint == 'X');
    assert(mulib_vterm_cell(&screen, 2, 1)->style.foreground == 1);
    mulib_vterm_free(&screen);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/vterm.c -o ./vterm-example
./vterm-example
```

`tests/test_terminal.c` checks cell values, margins, delayed wrap, origin mode,
relative vertical margin stops with origin mode off, nonmovement commands after
an absolute API move outside the origin region, editing, erases, rendition,
fragmented escapes/UTF-8, parser limits, resize, random byte fragmentation and
TUI output replay without an interactive terminal.
