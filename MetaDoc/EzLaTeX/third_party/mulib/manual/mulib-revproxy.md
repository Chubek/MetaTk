# mulib-revproxy

`mulib-revproxy.h` forwards one parsed HTTP request to a configured fixed origin,
using [mulib-webclient](mulib-webclient.md), and returns an independently owned
response. It is a reverse-proxy transaction core, not a listening proxy daemon.

## Build

Define `MULIB_IMPL_REVPROXY`, `MULIB_IMPL_WEBCLIENT`, `MULIB_IMPL_HTTP`,
`MULIB_IMPL_URI`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_INET4`, and `MULIB_IMPL_INET6` once, then include the header. Dependency
implementations are never silently enabled. C++ declaration inclusion has C
linkage.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/revproxy.c -o /tmp/opencode/revproxy
/tmp/opencode/revproxy
```

The assert-based example checks request Host replacement and Connection-nominated
header removal in both directions without contacting a network.

## Forwarding API

`mulib_revproxy_forward(origin, request, options, transport, user, response)`
borrows every input during the call. Request uses the shared HTTP message model;
its body is already decoded binary data if obtained from `mulib_http_parse`.
It is validated by HTTP serialization before forwarding. Target must be
origin-form, without authority or fragment. The target path/query is preserved
byte-for-byte and appended to origin. Absolute-form, CONNECT and any request
containing Upgrade are rejected; no tunnel is established.

Origin must be a trusted, configured absolute `http://host[:port]` origin.
An empty path or `/` is accepted, but a prefix path, query, credentials or fragment
is not. Port validation follows webclient (1..65535). Origin is not taken from
inbound Host, so clients cannot select a different authority through that field.

## Header and framing policy

In **both** directions remove, case-insensitively:

* Connection, Keep-Alive, Proxy-Connection;
* Proxy-Authenticate, Proxy-Authorization;
* TE, Trailer, Transfer-Encoding, Upgrade;
* every header whose name occurs in **any** Connection field's token list.

Connection lists accept comma-separated nonempty HTTP tokens with optional
space/tab. Empty lists/elements, trailing commas and other malformed syntax
produce INVALID, rather than being partly ignored. All occurrences of a
nominated field are removed, including when the Connection field itself appears
later. Request Host is replaced with upstream authority; webclient generates
its own `Connection: close` for the upstream transaction.

Decoded bodies are forwarded as ordinary Content-Length framing, not the
inbound chunk encoding. Request Content-Length and ordinary response
Content-Length are removed and regenerated. HEAD and 304 response representation
Content-Length is preserved unless Connection nominates it for removal. Shared
strict HTTP validation still rejects ambiguous framing, unsupported trailers,
chunk extensions and malformed start lines/fields. Informational responses are
handled by webclient, not exposed to the downstream caller.

End-to-end fields, including Authorization, Cookie, Set-Cookie and existing
Forwarded/X-Forwarded-* fields, are otherwise preserved. No credentials are
created and no trusted client-address metadata is synthesized. Strip/rewrite
forwarded identity headers at a trusted application boundary before using them
for authorization. No Via, loop detection, load balancing, retry, redirect,
header rewriting, caching or compression policy is supplied.

## Ownership, limits and failures

On `MULIB_WEBCLIENT_OK` (1), response owns its strings/headers/body; release with
`mulib_http_message_free`. Request is unchanged, including header storage and
chunked flags. Output must not already own allocations. On error output is
unchanged and all temporaries are released. Errors are INVALID (-1), LIMIT (-2),
NOMEM (-3), and TRANSPORT (-4). HTTP error statuses remain successful transactions.

Webclient options apply to inbound request validation, upstream transaction and
downstream serialization: default 32768 header bytes, 100 headers, 16 MiB body,
32 MiB raw upstream bytes. `http.flags` must be zero; HEAD/EOF flags are applied
internally. Generating new Host/Connection fields can require extra field budget.
Bodies are fully buffered and copied; geometric capacities plus multiple wire
and decoded copies mean these are not exact peak-memory bounds.

The synchronous binary transport must send the complete borrowed request,
deliver response chunks, stop when the sink fails and return 1 only at EOF.
It owns sockets, DNS, timeouts, cleanup and egress policy. No public network is
needed for tests. Only plaintext HTTP is supported: no TLS, CONNECT, WebSocket,
HTTP/2 or protocol upgrades. Do not expose an untrusted configurable origin;
network address ACLs/SSRF prevention and authenticated upstream connections are
outside this core. Unauthenticated HTTP is unsuitable for sensitive traffic.

`tests/test_web_stack.c` covers chunked-to-length binary forwarding, hop-by-hop
and nominated fields, invalid Connection syntax, origin restrictions, preserved
end-to-end fields, independent ownership and client/router/file-server/HEAD
composition.
