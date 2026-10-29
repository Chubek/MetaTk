# MuLIB termkey

`include/mulib-termkey.h` is an allocation-free, incremental byte-to-event
decoder. It does not read stdin, change termios settings, query terminfo, start
timers, or assume a terminal is attached.

## Integration

Define `MULIB_IMPL_TERMKEY` and its dependency `MULIB_IMPL_UTF8` exactly once,
before any includes. Declarations support C++17; implementation uses ISO C11
without external libraries or OS calls.

`mulib_termkey_t` stores a 128-byte pending buffer and a paste-mode flag.
`mulib_termkey_init` initializes or resets it; there is no allocation/free.
An event owns its `raw[128]` bytes inline, so it can be retained after the next
decoder operation. Other state members should not be modified directly.

## Feed/pull contract

* `mulib_termkey_feed(decoder, bytes, length)` copies **up to** the available
  capacity and returns the accepted count. NULL bytes are allowed only for zero
  length. Always honor a short count; pull events and retry unaccepted input.
* `mulib_termkey_next(decoder, event, flush)` returns **1** with one event,
  **0** for no complete event, or **-1** for invalid arguments. A decoder with a
  full buffer always makes progress, including with an overlong escape sequence.
* Ordinarily use `flush=0`. A lone Escape is ambiguous with an Alt-prefix/key
  sequence and remains pending. After **your chosen Escape timeout**, or when
  input ends, repeatedly call with `flush=1` until it returns 0. A partial
  CSI/SS3 becomes UNKNOWN; an incomplete UTF-8 scalar becomes replacement text.
  The library has no built-in timing policy.
* Return 0 on empty input leaves the event untouched. Treat it as valid only
  after return 1. Decoder/event objects must not overlap input or each other.

For arbitrary streams, alternate feed and pull, advancing the source by exactly
the accepted count. A short feed is backpressure, not dropped input. Do not flush
between arbitrary read boundaries: both UTF-8 and escapes may be fragmented.

## Events and supported protocols

`mulib_termkey_event_t` contains `type`, `key`, Unicode `codepoint`, modifier
bits, mouse information, and the original `raw` bytes plus `length` (not
NUL-terminated). Coordinates are zero-based. Fields start zeroed; single-character
keys also retain their input scalar in `codepoint` when mapped to a special key.

* KEY: printable UTF-8 CHAR events; Escape, Enter (CR or LF), Tab, Backspace
  (BS or DEL); arrows, Home/End, Insert/Delete, Page Up/Down; F1..F12.
* Common VT/xterm CSI/SS3 finals `A B C D H F P Q R S`, CSI `Z` (Shift-Tab),
  and numeric `~` keys: 1/7 Home, 4/8 End, 2 Insert, 3 Delete, 5/6 pages,
  11..15/17..21/23..24 function keys.
* Xterm `1;modifier` arrows/functions and `number;modifier~`. Modifier parameter
  1..16 encodes SHIFT=1, ALT=2, CTRL=4, SUPER=8 as `parameter-1`.
* Basic `CSI scalar[;modifier]u` Unicode-key reporting, including supplementary
  scalars. No colon subparameters, key-up/repeat events or Kitty negotiation.
* ESC followed by a character produces an ALT-modified key, including fragmented
  multibyte UTF-8. Two ESC bytes resolve as one Escape followed by a pending
  Escape. `ESC [` and `ESC O` always begin sequences, not Alt-[ / Alt-O.
* Traditional control bytes map to CTRL letters (`1..26` -> `a..z`), NUL ->
  CTRL-Space, and `28..31` -> CTRL-`\ ] ^ _`. Dedicated special keys take
  precedence. CSI-u uses the same control-key normalization.
* SGR mouse `CSI <button;x;y M/m`: first three buttons, motion, four wheel
  directions, Shift/Alt/Ctrl, press/release. Buttons are 1 left, 2 middle,
  3 right, 4..7 wheel up/down/left/right; 0 means no button. No legacy X10 mouse,
  pixel coordinates, or extended buttons are implemented.
* Bracketed paste `CSI 200~` / `CSI 201~` produces PASTE_BEGIN/PASTE_END. While
  pasting, all bytes except the end marker produce PASTE_TEXT, not key or
  control events. PASTE_TEXT may split a UTF-8 scalar: it is **raw byte data**,
  not a decoded character. A suffix that could be a fragmented end marker is
  held until more bytes arrive; flush releases it as text. Flush does not invent
  a missing paste-end marker. `init` resets an unterminated paste.

Unknown/malformed/unsupported escape sequences become UNKNOWN with their raw
bytes. Numeric fields are bounded to U+10FFFF, at most four fields are parsed,
and escape storage is 128 bytes. At the limit an unknown prefix is emitted;
remaining bytes can subsequently become ordinary events. Invalid UTF-8 becomes
U+FFFD, consuming one invalid leading byte (so a truncated three-byte encoding
can yield multiple replacements on flush). Validation/decoding uses MuLIB utf8.

This is a useful focused key decoder, **not** a complete terminal-protocol parser,
terminfo key matcher, grapheme recognizer, or full Kitty/Win32 keyboard decoder.
It does not fold CRLF into one key; both bytes are separate Enter events.

## Runnable example

```c
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
    assert(event.key == MULIB_TERMKEY_UP);
    assert(event.modifiers == MULIB_TERMKEY_CTRL);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/termkey.c -o ./termkey-example
./termkey-example
```

The example and `tests/test_terminal.c` require no terminal. Tests feed single
bytes, compare complete/fragmented events, test Escape flushing, malformed UTF-8,
numeric/sequence limits, mouse coordinates and paste-end fragmentation.
