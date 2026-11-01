# mulib-webclient

`mulib-webclient.h` implements a synchronous, bounded HTTP/1.1 transaction. It
generates requests and validates/parses responses using the shared
`mulib_http_message_t` model. It does **not** open sockets or provide TLS.

## Integration

In one C11 translation unit define `MULIB_IMPL_WEBCLIENT` and its transitive
dependencies `MULIB_IMPL_HTTP`, `MULIB_IMPL_URI`, `MULIB_IMPL_STRBUF`,
`MULIB_IMPL_BUFFER`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_INET4`, and
`MULIB_IMPL_INET6`, then include `mulib-webclient.h`. No dependency implementation
switch is defined by the library. Other translation units include declarations
only; declarations have C linkage in C++.

Runnable offline example:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/webclient.c -o /tmp/opencode/webclient
/tmp/opencode/webclient
```

## Request API

`mulib_webclient_request(url, method, headers, header_count, body, body_length,
options, transport, user, response)` borrows every input. Body bytes may include
NULs. NULL body requires zero length; NULL headers requires zero count. URL,
method, and header strings must be valid NUL-terminated strings. It generates
an origin-form target, `Host` (including an explicit port), `Connection: close`,
and HTTP framing. User `Host` or `Connection` headers are rejected, even if
their values match. Content-Length, if supplied, must match the body; a supplied
Transfer-Encoding is rejected because this API emits non-chunked requests.

URLs must be absolute `http://` URLs with a nonempty host, no credentials or
fragment, and a numeric port in 1..65535 if present. Scheme comparison is
case-insensitive. Empty paths become `/`; query bytes and escapes are preserved.
IPv6 literals are supported by the shared URI parser. `https`, CONNECT, HTTP/2,
HTTP/1.0, upgrades and tunnels are not supported. A scheme saying `https` cannot
silently turn into plaintext HTTP.

## Transport contract

The synchronous callback receives `(user, endpoint, request, request_length,
receive, receiver)`. `endpoint` is a borrowed parsed `mulib_uri_t` referencing
the URL. Its host slice is not NUL-terminated and includes brackets for literals;
the default port is 80 when `has_port` is false. The callback must:

1. Establish the intended plaintext transport and send **all** request bytes,
   handling short writes itself.
2. Pass received binary chunks to `receive(receiver, bytes, length)` in order.
   Zero-length chunks are allowed. Stop immediately when it returns zero.
3. Return exactly 1 only after response EOF; return another value on IO failure.

Pointers are valid only during the callback. Do not retain them or invoke the
sink asynchronously. The callback owns its sockets/IO resources, DNS policy,
timeouts, cancellation and cleanup. A local in-memory application is also a
valid transport. Callback code must obey its buffer contracts; the library
cannot prevent a callback from reading beyond a borrowed request.

## Response, limits and errors

Success is `MULIB_WEBCLIENT_OK` (1), regardless of HTTP status. `response`
receives independent owned allocations: release with `mulib_http_message_free`.
It must not already contain live allocations. On failure it is unchanged.
Errors are INVALID (-1), LIMIT (-2), NOMEM (-3), TRANSPORT (-4). A receive-limit
or receive-allocation error takes precedence over the callback's failure code.

`options == NULL` and zero fields use:

| Limit | Default |
| --- | --- |
| HTTP header bytes | 32768 |
| Headers | 100, including the two generated request fields |
| Decoded body bytes | 16 MiB, for both request and response |
| Raw response bytes | 32 MiB, including interim responses/chunk framing |

`options.http.flags` must be zero. EOF and HEAD response context are applied
internally. Up to eight informational responses are skipped; 101 is rejected.
Exactly one final response must exhaust the supplied bytes: unsolicited extra
frames/bytes are errors. Framed and EOF-delimited responses are accepted; HEAD
responses retain representation Content-Length but have no body. The strict
HTTP parser rejects ambiguous framing, trailers and chunk extensions; see
[mulib-http](mulib-http.md).

The entire raw response is buffered before parsing, not streamed to the caller.
Buffer capacities may exceed their logical limits due to geometric growth;
there can simultaneously be request wire, response wire and decoded body
allocations. Limits are not exact total-memory budgets. No allocator injection
API is supplied. No redirects, decompression, cookie jar, connection pool,
authentication negotiation, retries, DNS or certificate validation are hidden
inside this module. Do not accept untrusted URLs without an application SSRF
policy; do not pass host strings to a shell. Plain HTTP offers no confidentiality
or peer authentication.

Focused tests: `tests/test_web_stack.c` covers binary chunks, ownership,
informationals, HEAD, EOF bodies, malformed/truncated frames, injection,
transport failure, limits and offline application composition.
