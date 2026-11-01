# mulib-impaclient (IMAP client)

**The module's historical spelling is `impaclient`, not `imapclient`.** Its file,
symbols (`mulib_impaclient_*`) and implementation switch preserve that spelling;
the protocol enum prefix is `MULIB_IMAP_`. It implements the useful tagged IMAP
subset of `mulib-imapserver`, not a typo-named unrelated protocol.

This is a transport-independent, allocation-free session client, not a complete
IMAP reader or secure mail service. No sockets, TLS, SASL, FETCH, UID model,
mailbox mutation, extensions or asynchronous notifications. It is designed for
the paired focused server, not arbitrary standards-complete IMAP servers.

## Integration

The header includes `mulib-imapserver.h` for `mulib_imap_quote`. Define both
`MULIB_IMPL_IMAPSERVER` and `MULIB_IMPL_IMPACLIENT` explicitly before including
`mulib-impaclient.h` in one C11 unit, or place each implementation in its own
unit. There are no other dependencies or implicit switches; C++ declarations
have C linkage. Scratch storage avoids unnecessary allocating containers.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/impaclient.c -o /tmp/opencode/impaclient
/tmp/opencode/impaclient
```

## Session API

`init(c,line,capacity,send,context)` borrows scratch of at least 16 bytes and the
output callback/context. It emits nothing. Feed a printable `* OK text` CRLF
greeting; PREAUTH/BYE greetings are explicitly unsupported. State is 0 greeting,
1 ready, 2 awaiting tagged completion, 3 closed, or -1 terminal error.

`request(c,command,a,b)` accepts exactly one outstanding command:

| Enum | Arguments / prerequisites |
| --- | --- |
| `MULIB_IMAP_CAPABILITY` | NULL/empty a,b |
| `MULIB_IMAP_LOGIN` | Nonempty username a, password b (NULL means empty); not authenticated |
| `MULIB_IMAP_SELECT` | Nonempty mailbox a, NULL/empty b; accepted LOGIN required |
| `MULIB_IMAP_NOOP` | NULL/empty a,b |
| `MULIB_IMAP_LOGOUT` | NULL/empty a,b |

Tags are generated as `T1`, `T2`, ... from an unsigned long serial. Exhaustion
at ULONG_MAX fails without wraparound/output. All string arguments are quoted
using the server's shared helper: quote/backslash are escaped, printable ASCII
only, controls/high bytes/CRLF injection rejected. Braces in quoted arguments
are ordinary bytes, **not literals**. NULL for a required nonempty argument
fails. Commands and quotes/CRLF must fit scratch, including temporary quote NUL;
some minimal capacities can receive a greeting but cannot submit long commands.
Partial replies, wrong state/args, capacity or serial exhaustion send nothing
and leave a live session usable. Invalid requests may modify scratch contents
but not session state/serial. Transport authentication/protection is external.

## Incremental replies

`feed` accepts arbitrary chunks and exact CRLF; lines have at most `capacity-1`
printable ASCII bytes excluding CRLF/NUL. A tagged completion must exactly match
the outstanding tag and have `OK`, `NO` or `BAD` (optionally followed by space and
response text). `ok` is one only for OK; NO/BAD are nonterminal protocol refusals.
Successful LOGIN sets authorized. SELECT clears previous selected/count when
requested; successful completion sets selected, and a `* number EXISTS` response
sets `exists`. Count parsing is checked for size_t overflow. Zero is the default
if no EXISTS is supplied. NO/BAD leaves selected false.

Only these untagged responses are accepted while waiting:

* `* CAPABILITY X-MULIB-FOCUSED` for CAPABILITY; sets `capability`.
* `* number EXISTS` for SELECT (decimal digits, then exact suffix).
* `* BYE text` for LOGOUT; an OK logout without BYE fails.

Unknown untagged responses, continuations (`+`), response literals, wrong tags,
unsolicited replies and unsupported status tokens fail terminally rather than
being discarded. Tagged response text is opaque printable text, not a literal
announcement or a parsed extension. There is no total untagged-response count
bound: apply a transport timeout/byte quota. The server advertises no full IMAP
revision and the client deliberately accepts this focused capability only.

## Ownership and failures

All memory is caller-owned. Session/scratch/context must outlive use; input,
NUL-terminated argument strings and scratch/session storage must be disjoint.
Input must be readable for n bytes; NULL is permitted only for zero n. No
argument/input pointer is retained. Send consumes/copies **all** bytes before
returning nonzero, or returns zero; callbacks cannot retain pointers, mutate
fields or reenter sessions. Queue client/server outputs as in the example,
rather than feeding back synchronously. Synchronize external concurrent access.

Functions return one success, zero failure. Invalid init leaves the object
unchanged; malformed reply/framing/count overflow/line overflow and send failure
set state -1. An invalid caller argument does not poison an otherwise live
session. Output may be partially emitted, and a failing feed may have consumed a
prefix; do not retry. Closed/failed feeds return zero. On disconnect, partial or
pending state means incomplete exchange; reset explicitly, never infer success.
No destructor, allocation-failure path or secure buffer erasure is provided.
Capacity arithmetic and quote expansion are checked for overflow.
`tests/test_mail_ftp.c` covers all commands, quoting, composition, malformed
commands/replies, capacity/state errors and output failure.
Only init accepts uninitialized session storage; do not copy a live session and
share its scratch buffer.
