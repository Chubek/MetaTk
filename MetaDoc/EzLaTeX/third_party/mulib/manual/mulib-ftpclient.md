# mulib-ftpclient

A bounded, transport-independent incremental FTP reply parser and command/session
client. It composes with `mulib-ftpserver` but does not depend on its header or
implementation. It does not create sockets, negotiate data endpoints, provide
TLS, or implement a full FTP client. Use only with a deliberately configured
adapter for the documented subset, not as a production security service.

## Integration

Define `MULIB_IMPL_FTPCLIENT` once before including `mulib-ftpclient.h`; no
transitive switches are required. The standalone composition example additionally
defines `MULIB_IMPL_FTPSERVER` explicitly. C11 implementation, C++ declarations,
no allocations or third-party dependencies.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/ftpclient.c -o /tmp/opencode/ftpclient
/tmp/opencode/ftpclient
```

## API and states

`mulib_ftpclient_init(c,line,capacity,send,context)` borrows a scratch/reply buffer
of at least eight bytes and an output callback/context. It performs no I/O and
starts in state 0 (greeting). Feed a `220` greeting before submitting commands.
`mulib_ftpclient_feed` accepts arbitrary chunks of exact CRLF replies. Each line
has at most `capacity-1` printable ASCII bytes; tabs, high bytes, NUL, bare LF,
bad CRLF and overflow are terminal errors. Empty input is a no-op in live states.

Replies start with three digits (100–599), then space or hyphen. Multiline
replies begin `ddd-`, can have arbitrary bounded printable intermediate lines,
and end only with the same `ddd ` prefix. Completion of a reply updates `code`.
There is no aggregate multiline-count bound; enforce a transport timeout/quota.
An unsolicited reply, including a multiline reply while ready, is rejected.

`mulib_ftpclient_request(c,command,arg)` sends one command in ready state only.
Commands are `MULIB_FTP_USER`, `PASS`, `NOOP`, `PWD`, `CWD`, `TYPE`, `RETR`, `STOR`,
`DELE`, `QUIT` (each with the `MULIB_FTP_` prefix). NOOP/PWD/QUIT require NULL or
empty arg; PASS permits an empty password. Others require nonempty arg; TYPE
requires exactly `I`. Arguments are NUL-terminated printable ASCII strings;
CR/LF injection and controls/high bytes are rejected before output. Paths are
literal arguments, not quoted or decoded. USER's 127-byte server limit is checked
by that server, not a client-side claim of universal FTP limits.

The generated command and CRLF must fit capacity; no spare NUL byte is required
for outgoing bytes (unlike incoming reply lines). Invalid
arguments, pending/partial-reply state or insufficient capacity send nothing and
leave the session usable. The client does not enforce authentication locally:
the server's reply is authoritative.

| State | Meaning |
| --- | --- |
| 0 | Awaiting 220 greeting |
| 1 | Ready to submit one command |
| 2 | Awaiting command reply |
| 3 | RETR/STOR accepted via 125/150; awaiting transfer completion |
| 4 | Closed by 221/421 |
| -1 | Terminal parser/output failure |

Expected successful replies are USER 230/331, PASS 230, NOOP/TYPE 200, PWD 257,
CWD/DELE 250, QUIT 221, and RETR/STOR 125/150 followed by 226/250. Any 4xx/5xx
command/transfer reply completes unsuccessfully (421 closes). Other success or
intermediate codes are unsupported and terminal, rather than silently ignored.
`code` reports protocol refusal; feed returning one does **not** mean the server
accepted the operation. Data bytes never pass through this control parser: the
application owns a separate data channel. In composition use the server's
read/write/finish APIs, then feed its final 226/451 back into the client.

## Contracts

All buffers/session/context remain caller-owned; do not overlap them. String
arguments must be readable through NUL and may not alias scratch storage. Feed
spans must be readable and disjoint from the session/scratch buffer; NULL is
permitted only at zero length. Send must synchronously consume/copy **all** bytes
and return nonzero, or fail with zero. It may not retain pointers, reenter a
session (including a synchronous client/server feedback loop), or mutate public
fields. Use bounded queues like the example, and synchronize external threads.

Return values are one success, zero failure. Invalid init leaves the object
unchanged. Bad caller arguments return zero without poisoning a valid object.
Malformed replies, line overflow, unexpected protocol transitions and output
failure set state -1. Output failure may leave partial wire bytes; feed failure
may have consumed a prefix and must not be retried. Closed/failed feeds return
zero. At transport EOF, partial line/multiline or pending states are incomplete,
not success; abort/reset externally. No resource destructor is needed. No secure
credential erasure is promised; clear owned buffers after use. Bounds are checked
without allocation arithmetic or multiplication. Full malformed, state, binary
transfer composition and failure tests live in `tests/test_mail_ftp.c`.
Only init accepts uninitialized session storage; do not copy a live session and
share its scratch buffer.
