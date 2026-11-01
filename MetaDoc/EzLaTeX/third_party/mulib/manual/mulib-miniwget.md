# mulib-miniwget

`mulib-miniwget.h` downloads a single HTTP resource into caller-owned memory,
with a hard bound on decoded destination bytes. It uses
[mulib-webclient](mulib-webclient.md) for request generation, transport and HTTP
response processing. It does not write files or implement the wget CLI.

## Integration

Define `MULIB_IMPL_MINIWGET`, `MULIB_IMPL_WEBCLIENT`, `MULIB_IMPL_HTTP`,
`MULIB_IMPL_URI`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_INET4`, and `MULIB_IMPL_INET6` once. Include `mulib-miniwget.h`.
Implementation macros are never enabled by dependencies; declarations have C++
C linkage.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/miniwget.c -o /tmp/opencode/miniwget
/tmp/opencode/miniwget
```

The offline example checks that an undersized destination is unchanged, then
successfully downloads three bytes containing a NUL.

## Download contract

`mulib_miniwget_download(url, options, transport, user, destination, capacity,
length)` sends GET, accepts a final status in 200..299, and copies exactly the
decoded body length to destination. It adds no terminator. `length` is required;
NULL destination is legal only with zero capacity. An empty 2xx download may
succeed with `(NULL, 0)`.

Inputs are borrowed. Destination storage must remain valid during the call and
must not alias URL/options/transport state. On success, `*length` is set; on
**any** failure, destination and `*length` are unchanged. Temporary HTTP storage
is always released. The destination never becomes library-owned.

Results:

| Result | Meaning |
| --- | --- |
| OK (1) | Complete 2xx body copied |
| INVALID (-1) | Bad arguments, unsupported URL, malformed HTTP |
| LIMIT (-2) | Raw/header/body bound exceeded or destination too small |
| NOMEM (-3) | Temporary allocation failed |
| TRANSPORT (-4) | Transport did not complete successfully |
| STATUS (-5) | Valid, within-limit final response was not 2xx |

Framing/limit errors take precedence over STATUS, so an oversized error page can
return LIMIT. There is no partial-success length or partial body exposure.

## Bounds and network responsibilities

The HTTP decoded-body limit is reduced to capacity (or one byte when capacity
is zero, since HTTP option zero means its default). A smaller explicit body
limit is preserved. An empty request has no body. Other limits come from
webclient options: defaults are 32768 header bytes, 100 header fields and 32 MiB
raw response. A larger destination may raise the default decoded-body bound to
its capacity; set an explicit smaller body limit if desired. Set raw response
limits too when accepting large downloads. `http.flags` must be zero.

This is a **buffered**, not streaming, download. Raw response storage, decoded
body and destination coexist, with geometric buffer capacity overhead. Capacity
is not an exact total-memory budget. The sink stops raw input after its logical
limit, but the callback must honor sink failure and implement its own IO timeout.

Only plaintext absolute `http` URLs are supported. The supplied synchronous
transport owns DNS, sockets and cleanup; the core does not contact a public
network. No TLS, redirects, resume/range support, decompression, cookies, retries,
filesystem side effects or checksums are provided. Validate untrusted URLs and
download content according to application policy. HTTP authenticates neither
the peer nor the contents.

`tests/test_web_stack.c` covers exact capacity, smaller capacity, unchanged
outputs, binary content, empty success, status failure and client/router/file
server composition.
