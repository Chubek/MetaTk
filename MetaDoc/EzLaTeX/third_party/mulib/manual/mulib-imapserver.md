# mulib-imapserver

A bounded tagged-command **IMAP-inspired subset**, not a complete RFC IMAP
service. It intentionally advertises `X-MULIB-FOCUSED`, **not** IMAP4rev1/rev2:
SELECT supplies only EXISTS/read-only status, with no FLAGS, UID model, FETCH,
mailbox mutations, asynchronous updates, STARTTLS or SASL. No sockets, storage
engine or secure authentication transport is supplied. Use a configured paired
client, not a claim of interoperability with arbitrary mail readers. LOGIN
credentials are cleartext unless the enclosing transport protects them.

## Integration

Define `MULIB_IMPL_IMAPSERVER` once before including `mulib-imapserver.h`.
No MuLIB dependencies, heap allocations or hidden switches. The public quote
helper is reused by `mulib-impaclient.h`. C11 implementation, C++ declarations.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/imapserver.c -o /tmp/opencode/imapserver
/tmp/opencode/imapserver
```

## Grammar and state

`init(s,line,capacity,io,context)` borrows caller scratch of at least 16 bytes,
copies callbacks, borrows context, and emits `* OK MuLIB focused IMAP subset`.
`feed` incrementally handles multiple lines and any CRLF fragmentation.
At most `capacity-1` printable ASCII bytes precede exact CRLF; controls, tabs,
NUL/high bytes, bare LF, bad CRLF and overflow are terminal framing errors.

Every command has a tag of 1–63 ASCII letters/digits/underscore/hyphen/dot,
a space, an unquoted case-insensitive command, and zero or more space-separated
arguments. Multiple/trailing spaces are accepted; leading spaces/invalid tags
get untagged `* BAD`. Tag matching is case-sensitive. At most four tokens total
(tag, command, two args) are accepted.

Arguments may be atoms or quoted strings. Atoms exclude spaces and
`() { % * " \\ ] }` (the space in this displayed list is a separator).
Quoted strings accept printable ASCII, including spaces, with **only** `\"`
and `\\` escapes; unknown/trailing escapes, unterminated quotes or text adjacent
to closing quotes are BAD. No literals, literal+, continuation prompts, UTF-8,
modified UTF-7 decoding or multiline arguments are implemented. Literal markers
such as `{3}` are rejected on the command line with tagged BAD; the sender must
not send a literal body. Any further bytes are new framed commands, never a
silently consumed literal. Braces **inside quotes** are ordinary characters.

| Command | Behavior |
| --- | --- |
| `tag CAPABILITY` | `* CAPABILITY X-MULIB-FOCUSED`, then tagged OK |
| `tag LOGIN user password` | Authorization callback; OK/NO; already authenticated BAD |
| `tag SELECT mailbox` | Requires login; callback yields EXISTS count and read-only OK |
| `tag NOOP` | Tagged OK, any live state |
| `tag LOGOUT` | Untagged BYE then tagged OK, closes |

All wrong arities/unknown commands return tagged BAD. Unauthorized/unavailable
SELECT returns NO. A new SELECT clears previous selected state (including on
failure); successful LOGIN sets authorized state. Empty quoted credentials or
mailbox names are passed to callback policy; atoms themselves are nonempty.
Commands after LOGOUT are not accepted: trailing bytes in the same feed fail.

## Callbacks and quoting API

`send(ctx,bytes,n)` consumes/copies all bytes synchronously, returning nonzero,
or fails with zero. No partial-write semantics: buffer real socket writes.
`authorize(ctx,user,password)` and `select(ctx,mailbox,&exists)` return nonzero
for acceptance; NULL denies. SELECT count is a `size_t` with no artificial count
cap, formatted without overflow; zero is valid. The callbacks implement all
credential/mailbox/storage policy. They may not mutate the session, retain
borrowed pointers, or reenter it. Synchronize external concurrency.

`mulib_imap_quote(text,out,capacity,&written)` is a shared public utility. It
validates a readable NUL-terminated printable ASCII string, escapes quotes and
backslashes and surrounds it with quotes. Capacity includes a final NUL;
written excludes that NUL and includes quotes/escapes. Empty string encodes as
`""` (three bytes including NUL). All arguments are required, spans/output/written
must not overlap, and failure leaves output and written unchanged. Length
expansion uses overflow-checked addition, not unchecked `2*n` arithmetic.

## Ownership, limits and failures

Session/scratch/context/input remain caller-owned. Persistent storage must
outlive the session and be disjoint; feed input must not alias session/scratch
and must be readable for n bytes. NULL input is allowed only at n=0.
Callbacks borrow decoded strings only during invocation; they reside in mutable
line storage and must be copied if retained. No secure credential erasure occurs.
Public fields are observational; do not edit them to force transitions.

One means processed, including BAD/NO protocol rejections; zero means invalid
arguments/state or a terminal framing/output failure. Invalid init leaves an
existing object unchanged. Valid init can fail while sending the greeting and
leaves an initialized failed object. Output can be partially sent and feed can
consume a prefix on failure: never retry. Closed/failed feeds return zero.
At disconnect, partial lines are incomplete, not implicit commands. Reinitialize
explicitly; no destructor/heap cleanup is necessary. The line/token/tag bounds
limit per-command work and memory; impose external total-session quotas/timeouts.
There is no allocation failure path. See `tests/test_mail_ftp.c` for quoted
roundtrips, malformed literals/escapes, state errors and pairing tests.
Only init accepts uninitialized session storage; do not copy a live session and
share its scratch buffer.
