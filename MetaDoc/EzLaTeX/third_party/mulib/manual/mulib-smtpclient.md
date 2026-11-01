# mulib-smtpclient

A bounded SMTP session/envelope client with complete-message validation and
DATA dot stuffing. It is transport independent and intentionally supports only
the server's documented 7-bit subset, not arbitrary Internet mail, TLS, AUTH,
8BITMIME, SMTPUTF8, BINARYMIME, MIME or message delivery guarantees.

## Integration and dependencies

`mulib-smtpclient.h` includes `mulib-smtpserver.h` for its public message
validator. Define **both** `MULIB_IMPL_SMTPSERVER` and `MULIB_IMPL_SMTPCLIENT`
before including the client in one C11 translation unit (or explicitly place the
two implementations in separate units). Neither switch defines another switch.
Other units may use C or C++ declarations. No heap allocation occurs.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/smtpclient.c -o /tmp/opencode/smtpclient
/tmp/opencode/smtpclient
```

## Envelope and reply states

`init(c,line,capacity,message_limit,line_limit,send,context)` borrows scratch
storage (capacity >=8), output callback/context, and sets explicit maximum
unstuffed message bytes and per-line bytes (excluding CRLF). It emits nothing.
Feed a `220` greeting, then submit `mulib_smtpclient_request` commands:

| Enum (prefix `MULIB_SMTP_`) | arg / prerequisite |
| --- | --- |
| `HELO` | Nonempty printable nonspace hostname <=255 bytes |
| `MAIL` | Bare unquoted path, possibly empty for `<>`; accepted HELO required |
| `RCPT` | Nonempty bare path; accepted MAIL required; <=16 accepted recipients |
| `DATA` | NULL/empty arg; accepted MAIL and at least one recipient |
| `RSET`, `NOOP`, `QUIT` | NULL/empty arg |

Path alphabet/255-byte limit matches `manual/mulib-smtpserver.md`. Paths are
wrapped in `<...>` by the client; do not supply angle brackets or ESMTP
parameters. Every argument is validated before any output; CRLF/control/high-byte
injection, wrong state, pending partial replies, wrong args and insufficient
scratch capacity return zero without sending. Commands plus CRLF must fit the
scratch capacity. New MAIL invalidates the previous client envelope immediately,
matching the paired server even if MAIL is rejected. Successful HELO/RSET and
DATA completion reset the transaction. No EHLO API/extensions are implemented.

State is 0 awaiting greeting, 1 ready, 2 awaiting reply, 3 ready for message
after `354`, 4 closed, or -1 terminal failure. Only one command may be outstanding.
`code` records each completed reply. HELO/MAIL/RSET/NOOP require `250`; RCPT accepts
`250/251/252`; QUIT requires `221`. DATA requires `354`, then message completion
requires `250`. 4xx/5xx complete a command unsuccessfully; `421` closes. Unexpected
successful/intermediate codes are terminal. A feed returning one can represent
a refusal: inspect code/state, not just the return value.

`feed` accepts arbitrary chunk boundaries with exact CRLF. Lines contain
printable ASCII/tab, at most `capacity-1` bytes before CRLF/NUL. Replies require
three digits (200–599) and space/hyphen; multiline replies require the same code
on **every** line, ending with space. Unsolicited, malformed, overlong or unexpected
replies fail terminally. Multiline count is not globally bounded; enforce an
external timeout/input quota. NULL input is permitted only for zero bytes.

## Sending DATA

In state 3 call `mulib_smtpclient_message(c,data,n)`. It validates **all** bytes
before sending anything with `mulib_smtp_message_valid`: printable ASCII/tab,
strict CRLF, no binary/NUL/high bytes, within configured total and per-line
bounds, and final CRLF for nonempty content. Empty content (NULL,0) is valid.
There is no newline normalization or implicit appended content CRLF. A partial
unsolicited reply buffered after `354` blocks message submission, including an
empty message, without sending anything; it cannot become a completion reply
for DATA that had not yet been sent.

The function then emits an extra `.` before each dot-leading content line,
the original CRLF lines, and `.` CRLF as terminator. It uses no temporary message
allocation; output is one callback per line, an extra callback for each stuffed
dot, plus the terminator. Message_limit counts original bytes, not stuffing.
Set line_limit <= paired server line capacity minus two to leave room for the
stuffing dot and NUL. The complete input span must remain readable throughout
the call. Invalid messages leave state 3 for correction; output failure can
leave a partial DATA exchange and permanently fails the client. Successful
submission leaves state 2 until the final server reply, not yet delivered.

## Ownership and failure contract

All memory remains caller-owned; scratch/session/context must outlive their
uses. Input/message/argument spans may not alias scratch/session storage and
must be readable (strings through NUL). No input is retained after a call.
The output callback consumes/copies all bytes before returning nonzero, or
returns zero; it must not retain pointers or reenter either side of a connected
session. Queue outputs and feed the peer outside callbacks, as the example does.
No concurrent access without application locking; public fields are observations.

Return one means success, zero means failure. Invalid init leaves the object
unchanged; ordinary validation/state errors leave a live object usable.
Framing/overflow/unexpected reply or send failure sets state -1. Failed feed
may have consumed a prefix; never retry its remainder. Closed/failed sessions
reject feeds. On disconnect, partial/pending states mean incomplete exchange;
there is no recovery/destructor or implicit resubmission. Caller buffers are
not securely erased. Checked capacity subtraction and fixed envelope limits
avoid unchecked arithmetic; there is no allocation failure path. Composition,
empty/dot/binary rejection and malformed replies are tested in
`tests/test_mail_ftp.c`.
Only init accepts uninitialized session storage; do not copy a live session and
share its scratch buffer.
