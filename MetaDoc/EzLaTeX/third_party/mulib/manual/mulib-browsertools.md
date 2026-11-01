# browsertools: CDP messages, not a built-in browser

`mulib-browsertools.h` builds Chrome DevTools Protocol (CDP) JSON requests,
tracks pending requests, matches responses by ID **and flattened session ID**,
and inspects `/json/list` target bodies. The application supplies a transport
callback and complete incoming JSON messages. It does not launch browsers,
provide a browser engine, implement WebSocket framing, open sockets, or execute
JavaScript locally. CDP methods remain subject to the remote browser's version
and policy.

## Integration

In one C11 unit define `MULIB_IMPL_BROWSERTOOLS` and independently provide
`MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`, and `MULIB_IMPL_UTF8`.
The module includes the existing HTTP message declaration for target discovery,
but calls no HTTP runtime functions; **HTTP/URI/BUFFER implementation switches
are not required** unless your application uses their APIs. No dependency
implementation switches are silently enabled. C++ declarations use C linkage.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/browsertools.c -lm -o browsertools
./browsertools
```

The runnable example prints a `Runtime.evaluate` JSON request and matches a
simulated reply. It deliberately does not claim a live browser connection.

## Request/transport contract

Create one client per connection with `mulib_browsertools_create(send,user,limits)`.
The send callback receives borrowed UTF-8 JSON and its byte length, valid until
return. Return zero only if the message has been accepted/sent; a nonzero result
becomes TRANSPORT. Copy bytes if an asynchronous sender needs to retain them.
The host must implement WebSocket text framing, authentication, connection
lifecycle, polling and threading. **Do not reenter the client from the callback**;
deliver replies after the request function returns. No concurrent access.

`request(client,method,params,session,&id)` sends one JSON envelope. Method is a
nonempty UTF-8 C string. Params is NULL or a borrowed JSON object, copied before
serialization; the caller keeps ownership. Session is NULL for root requests or
a nonempty UTF-8 flattened session ID. Strings are serialized by the existing
JSON module, not interpolated into JSON syntax. Unknown method names are sent
as-is for browser-side validation. Inputs remain borrowed only for the call.

IDs increase monotonically from 1 through UINT32_MAX and never reuse. Exhaustion
returns LIMIT. A failed transport call consumes its ID (delivery may be uncertain)
but does not keep a pending slot. Construction/size failures do not consume IDs.
The optional ID output changes only on success. Pending entries own their copied
session names. `cancel(id)` removes local tracking for timeouts; it sends no
remote cancellation and leaves IDs unreused.

Convenience builders:

* `navigate(url,session,&id)` sends `Page.navigate` with a JSON-escaped URL.
  URL scheme/security policy is the host/browser's responsibility.
* `evaluate(expression,session,&id)` sends `Runtime.evaluate` with `awaitPromise`
  and `returnByValue` true. This is arbitrary **remote** JavaScript execution,
  not the microjs language. Never accept untrusted expressions without a policy.
* Other commands (e.g. `DOM.getDocument`, `DOM.querySelector`, `Input.dispatchMouseEvent`)
  use `request` with a caller-built params object. This module does not pretend
  that low-level CDP requests implement waits, selectors, retries or automation
  workflows automatically.

## Incoming envelopes and ownership

Initialize `mulib_browsertools_message_t` to zero. `receive` consumes exactly one
complete JSON message; it is not a streaming frame decoder. The root must be an
object, and an optional `sessionId` must be a nonempty string without NUL.

* Events have no ID, a nonempty string `method`, optional object `params`, and no
  `result`/`error`. Return EVENT; they do not change pending state.
* Responses have a positive integral uint32 ID, no method/params, and exactly one
  object `result` or object `error`. Error requires an integral numeric `code`
  and string `message`. The error's optional data and unknown extension fields
  remain in the owned envelope. Remote code values are doubles, not C integers.
* A response retires a pending entry only when both ID and session match exactly.
  Omitted session matches only a root request. Mismatches, duplicate replies and
  canceled replies return UNMATCHED without consuming unrelated requests.
* A matched error returns RESPONSE with `remote_error=1`; transport success does
  not imply browser-command success. `Runtime.evaluate` can additionally report
  JavaScript exceptions inside a successful result, which the host must inspect.

RESPONSE, EVENT and UNMATCHED all transfer the parsed JSON root to the output.
Its members are accessed with ordinary JSON APIs and remain borrowed from that
root. `mulib_browsertools_message_free` destroys and clears it; NULL is harmless.
Do not shallow-copy ownership. Malformed/oversized input leaves output and pending
state unchanged. INVALID reports malformed JSON/protocol shape, MEMORY reports
recognized JSON allocation failures, LIMIT reports byte/depth/work exhaustion.
The JSON parser's ambiguous numeric-range/allocation diagnostic maps to INVALID.
The underlying JSON parser's duplicate-object-key replacement policy also applies.

## Target discovery

`mulib_browsertools_targets` borrows an already parsed `mulib_http_message_t`.
It requires a response status 200 and a body within the message-byte budget.
The body must parse as an array of objects, each containing nonempty string
`id` and `type`, and string `url`. Other fields, such as `title` and
`webSocketDebuggerUrl`, remain available in the returned independently owned
JSON array. An empty array succeeds. NULL means validation, limit or allocation
failure. Content-Type is not enforced; HTTP parsing/framing must already have
been performed by the host. This helper makes no connection and does not follow
returned URLs.

## Resource bounds

NULL limits selects 128 pending entries and 1 MiB per message. Supplied zero
fields are real zero limits. The pending table is allocated once with checked
multiplication. Each stored session is bounded by the message cap. Incoming JSON
is length-limited before parsing and inherits JSON's depth cap (default 128) and
input-scaled parser work/capture budgets.

Outgoing caller-built JSON is traversed before copying: depth <=32 and a budget
of at most `message_bytes` total nodes plus key/string bytes. This conservative
preflight can reject an object whose wire representation happens to fit. The
actual serialized message is checked against the exact byte cap before sending.
Temporary serialization may expand a raw byte up to sixfold and JSON trees have
allocator overhead; the cap is a logical wire limit, not an exact malloc quota.
The client has no background thread, timers or automatic retries. Destroy frees
pending data but never closes caller-owned transport resources.

`tests/test_language_tools.c` verifies escaping/JSON roundtrip, event/error/result
shapes, ID/session mismatch, cancellation, transport failure, empty/malformed
discovery, pending/message budgets, allocation failures, and mutated inputs.
