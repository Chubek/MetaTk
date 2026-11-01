# mulib-ftpserver

An allocation-free FTP **control-session subset**, with application-owned binary
data transport and storage. This is not a socket listener, complete FTP service,
or security boundary. TLS, passive/active endpoint negotiation, filesystem access,
timeouts, quotas and credential protection are the application's responsibility.
Passwords travel in cleartext unless the enclosing transport protects them.

## Integration

Define `MULIB_IMPL_FTPSERVER` before including `mulib-ftpserver.h` in exactly one
C11 translation unit. There are no MuLIB implementation dependencies. Other
translation units include the declarations normally; C++ declarations have C
linkage. No heap allocation occurs. Fixed caller storage is more appropriate
than an allocating container for this bounded framing/session API.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/ftpserver.c -o /tmp/opencode/ftpserver
/tmp/opencode/ftpserver
```

## Session and grammar

`mulib_ftpserver_init` borrows a line buffer (capacity at least two), copies the
callback table, borrows its context, initializes the session and emits `220`.
`feed` accepts arbitrary fragments, including a CR/LF split across calls and
multiple commands per call. Only printable ASCII control-line bytes and exact
CRLF are accepted. Capacity includes the NUL: at most `capacity-1` bytes before
CRLF. A bare LF, control/high byte, CR not followed by LF, or overlong command is
a terminal framing error. Empty lines get an unsupported/malformed diagnostic.

Verbs are case-insensitive. A single space separates the verb from the argument;
the remaining argument is passed verbatim, including further spaces. There is no
FTP path quoting or URL decoding. Supported commands are:

| Command | Behavior |
| --- | --- |
| `USER name` | Nonempty name, at most 127 bytes; clears login; `331` |
| `PASS password` | Requires USER; password may be empty; authorize callback; `230/530` |
| `NOOP`, `QUIT` | No argument; `200` or `221` and closed |
| `TYPE I` | Requires login; binary mode only; `200`; other modes `504` |
| `PWD` | Requires login; storage callback; quoted `257` result |
| `CWD path`, `DELE path` | Nonempty path, login, storage callback; `250/550` |
| `RETR path`, `STOR path` | Nonempty path, login, storage/data adapter; `150`, then finish |

All other commands, including PORT, PASV, LIST, AUTH and REST, get `502`.
State/argument/storage errors get explicit 4xx/5xx replies and do not poison the
session. While a transfer is active, **all** control commands get `450`; use
`finish(s,0)` for cancellation before sending another command. No pipelined
commands are accepted after QUIT: trailing bytes make the feed fail.

## Callbacks and binary transfer

`send(context,bytes,n)` must consume/copy all bytes synchronously and return
nonzero, or return zero. It is not a partial socket-write callback: use an output
queue. Pointers are borrowed for the call only. No callback may reenter a session
or mutate its public state; sessions need external synchronization.

`authorize(context,user,password)` is deny-by-default if NULL. A USER is consumed
by one PASS attempt. `command(context,verb,arg,result,capacity)` receives a
canonical uppercase verb and borrows strings until return. NULL/zero return is a
storage failure. Only PWD uses result: write a NUL-terminated path of at most 255
printable ASCII bytes, excluding quotes; invalid results produce `451` without
injection. Callbacks must respect the supplied bounds.

Successful RETR/STOR `command` opens an application resource. A failing command
must clean up its own partial work. A successful open is closed exactly once via
`close(context,success)`, including on later framing/output failure. Transfers
require close and the corresponding read/write callback; otherwise `425`.

* `mulib_ftpserver_write` delivers STOR chunks verbatim, including NUL, 0xff,
  CRLF and dot bytes. Chunks are borrowed; copy before return if retained.
* `mulib_ftpserver_read` calls the RETR reader with a positive capacity. The
  callback sets `*written <= capacity`; zero is EOF. The application sends these
  bytes on its separate data channel. Read does not automatically finish.
* `mulib_ftpserver_finish(s,success)` closes the resource, then emits `226` or
  `451`. A storage I/O callback failure also finishes unsuccessfully.

There is no aggregate transfer-size bound: data is streamed in bounded caller
chunks. Storage adapters **must enforce their own quotas and path policy**. No
data-channel connection is created, accepted or authenticated by this module.

## Ownership and failure

The session, line buffer, callback context and data chunks are caller-owned.
They must not overlap, and retained session storage must outlive the session.
Zero-length feed/write permits NULL data; positive lengths require readable
spans. Read requires non-NULL output/written and positive capacity. Invalid
arguments/state return zero without sending; invalid init leaves the session
unchanged. A valid init can fail while emitting its greeting and then leaves a
terminal session. Public fields are observational, not mutation interfaces.

Functions return one for success, zero for failure; `finish(s,0)` deliberately
returns zero after emitting `451`. A successful read sets written; failed reads
leave it unchanged, but callback-written output may be altered. Output failure
can leave a partial wire exchange and is terminal (`failed`). Feed may already
have consumed part of its input on failure; never retry its remainder. Closed
and failed sessions reject further feeds. A disconnect must abort any open
transfer with `finish(s,0)`; discard partial control lines, never infer completion.
Reinitialize only after releasing open resources. There is no destructor and no
allocation failure path. No secure password wipe is promised; wipe owned buffers
when required. Bound calculations use subtraction, never unchecked allocation
size multiplication.

Only initialization may be called on uninitialized session storage; other calls
require an initialized object. Do not copy a live session and share its scratch.

See `examples/modules/ftpclient.c` for control-session composition and
`tests/test_mail_ftp.c` for binary upload/download and malformed-input tests.
