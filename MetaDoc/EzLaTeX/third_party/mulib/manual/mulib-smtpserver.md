# mulib-smtpserver

An allocation-free, bounded SMTP envelope/DATA session subset, not a mail listener,
relay, RFC-complete MTA, or secure service. There are no sockets, DNS, TLS,
authentication, MIME parsing, disk queues or delivery retries. Applications must
provide transport protection, access/relay policy, timeouts and durable storage.

## Integration

Define `MULIB_IMPL_SMTPSERVER` exactly once before including `mulib-smtpserver.h`.
There are no MuLIB dependencies. Its public `mulib_smtp_message_valid` helper is
also used by the matching client; clients explicitly link this implementation.
Caller buffers avoid hidden allocation and automatic dependency switches.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/smtpserver.c -o /tmp/opencode/smtpserver
/tmp/opencode/smtpserver
```

## Commands and envelope

`mulib_smtpserver_init` borrows a line buffer (capacity >= 8) and message buffer
(NULL permitted only for zero capacity), copies callbacks, borrows context and
emits `220`. `feed` handles arbitrary chunks and multiple CRLF lines, including
DATA starting/ending inside a chunk. Command lines contain printable ASCII only;
max length is `capacity-1`, excluding CRLF and including a reserved NUL byte.
Verbs are case-insensitive; one space separates verb/argument.

* `HELO name` / `EHLO name`: nonempty printable nonspace name <=255 bytes;
  resets envelope, enables MAIL; `250` without advertised extensions.
* `MAIL FROM:<path>`: requires greeting; resets prior envelope even on failure.
  Empty reverse-path is supported. Accepted policy callback yields `250`.
* `RCPT TO:<path>`: requires accepted MAIL; nonempty path; up to 16 accepted
  recipients. Policy refusal `550`, recipient capacity `452`.
* `DATA`: no arguments; requires accepted MAIL and at least one recipient;
  emits `354`, then receives message through the dot terminator.
* `RSET`: resets transaction but preserves HELO; `250`.
* `NOOP`: `250`. `QUIT`: `221`, closes. Both take no arguments.

Paths contain at most 255 bytes, and only ASCII letters, digits and
``!#$%&'*+-/=?^_`{|}~.@``. This is a focused unquoted path token, **not** full
RFC mailbox validation: local-only tokens are permitted, application policy
must validate addresses. Source routes, quoted local parts, whitespace, ESMTP
parameters and trailing text are rejected (`501`). AUTH, STARTTLS, BDAT, VRFY and
all unknown commands return `502`; sequence errors return `503`. Malformed
argument variants may return `502` if they do not match a supported command form.

## DATA framing, bounds and rejection

DATA is focused **7-bit text**: printable ASCII and tab, exact CRLF, no NUL or
other controls/high bytes. The terminator is a line containing exactly `.`.
A content line beginning with dot must begin `..` on the wire; exactly one dot
is removed. A single dot followed by other bytes is explicitly invalid, not
silently accepted. Message bytes passed to the callback include each content
line's CRLF but no terminator. Empty DATA is a zero-byte message.

Message capacity bounds the unstuffed bytes, including CRLF; there is no extra
NUL or implicit text conversion. Incoming DATA lines fit `capacity-1` wire bytes,
so a dot-leading line's unstuffed limit is `capacity-2`; others `capacity-1`.
Size/line overflow drains boundedly through the terminator and emits `552`;
invalid DATA octets/dot syntax emits `554`. A later invalid line may replace an
earlier size diagnostic. Neither case calls the message callback. Draining has
no total byte/time limit, so transports must impose one. Bare LF and CR not
followed by LF are terminal framing errors even inside DATA. An unterminated
message on disconnect is **never** delivered. Successful or rejected DATA resets
the envelope; later commands in the same feed are parsed normally.

`mulib_smtp_message_valid(data,n,limit,line_limit)` validates a complete unstuffed
message using the same octet policy, total size bound and unstuffed line bound
(excluding CRLF). Empty is valid; nonempty input must end in CRLF. NULL is valid
only at zero length. It writes nothing, allocates nothing, and is shared with
the SMTP client. To guarantee reception for arbitrary leading dots, configure
the client line limit at most server line capacity minus two.

## Callbacks, ownership and failure

`send` must consume/copy all bytes synchronously and return nonzero, or zero to
fail; wrap partial socket writes in a bounded queue. `envelope(ctx,recipient,path)`
is called for each MAIL (0) / RCPT (1); NULL denies. `message(ctx,session,bytes,n)`
examines accepted `sender`, `recipients[0..recipient_count)` and the complete
message; nonzero yields `250 queued`, zero/NULL yields `554`. This means
acceptance, not real-world delivery. Copy/persist all required data before
returning; pointers are borrowed only for the call. Message may be NULL at n=0.
Do not retain the session's envelope pointers across reset. No callback reentry,
public-field mutation, or concurrent access without external synchronization.

Sessions, context, input, line and message buffers are caller-owned. Persistent
buffers must outlive the session; all spans and scratch storage must be disjoint.
Input must be readable for n bytes; NULL is allowed only for zero n. Invalid
arguments/init return zero, leaving an existing object unchanged. Valid init
can fail on output and then leaves a failed initialized object. One means input
processed, including protocol rejections; zero means invalid call or terminal
framing/output/command-capacity error. Output may already be partially sent and
feed may have consumed a prefix on failure; do not retry. Closed/failed sessions
reject further feed calls. At EOF discard partial commands/DATA and reinitialize
explicitly; no destructor is needed. Fields are observational only, and message
buffer contents after rejection/reset are unspecified (not securely erased).
All size additions use checked remaining-capacity arithmetic; no allocations or
allocation-failure paths exist. See `tests/test_mail_ftp.c` for composition,
binary rejection, dot handling, bounds and malformed-input coverage.
Only init accepts uninitialized session storage; do not copy a live session and
share its buffers.
