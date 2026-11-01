# mulib-minicurl

`mulib-minicurl.h` is a small C API for GET and method/body requests. It is not
the curl command line, libcurl, or a TLS implementation. Its real transaction
engine is [mulib-webclient](mulib-webclient.md), not a second HTTP parser.

## Build and dependencies

Define `MULIB_IMPL_MINICURL`, `MULIB_IMPL_WEBCLIENT`, `MULIB_IMPL_HTTP`,
`MULIB_IMPL_URI`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_INET4`, and `MULIB_IMPL_INET6` in exactly one C11 translation unit.
Include `mulib-minicurl.h`. No implementation macro is enabled implicitly.
Declarations can also be included from C++.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/minicurl.c -o /tmp/opencode/minicurl
/tmp/opencode/minicurl
```

The assert-based example sends a binary POST to an offline callback and checks
its generated method, Content-Type, body and response status.

## API

* `mulib_minicurl_get(url, options, transport, user, response)` sends GET with no
  body or additional application headers.
* `mulib_minicurl_send(url, method, content_type, body, length, options,
  transport, user, response)` sends an explicit method and binary body. A
  non-NULL Content-Type adds exactly one header; NULL omits it. A non-NULL empty
  string is an empty field value. Body length is explicit; no strlen is used
  on body bytes. NULL body with nonzero length is invalid.

All arguments other than response output are borrowed for the synchronous call.
Strings must be NUL-terminated. On `MULIB_WEBCLIENT_OK`, response owns its strings,
header array and body and must be released with `mulib_http_message_free`.
The output must not already own allocations. On error it is unchanged. HTTP
404/500 are successfully parsed transactions, not convenience-layer failures;
inspect `response.status` before using the body.

Return values are webclient's OK (1), INVALID (-1), LIMIT (-2), NOMEM (-3), and
TRANSPORT (-4). Header injection in Content-Type/method/URL is rejected by the
shared URI/HTTP validation before transport is invoked. To add arbitrary
application headers, use `mulib_webclient_request` directly rather than
embedding them in the content type.

## Transport, limits and exact scope

Supply `mulib_webclient_transport_fn`: it sends all borrowed request bytes,
delivers response chunks to the supplied sink, stops on sink failure, and returns
1 only after EOF. The transport owns DNS, sockets, timeout policy and cleanup.
See the webclient manual for slice lifetimes and the callback's complete contract.

Default limits are 32768 HTTP header bytes, 100 fields (including generated
Host/Connection), 16 MiB decoded body and 32 MiB raw response. Pass webclient
options to lower/increase them; `http.flags` must be zero. Responses are buffered
in memory, not streamed. Limits bound logical bytes, not exact peak allocation.

Only absolute plaintext `http` URLs without credentials/fragments are accepted.
There are no redirects, cookies, TLS, decompression, proxy discovery, retry logic,
file uploads from paths, or CLI parsing. HEAD is available through `send`, and
CONNECT/upgrades are unsupported. Caller-supplied binary transport does not make
`https` valid. Untrusted destinations require an application SSRF policy; HTTP
has no peer authentication or encryption.

Tests in `tests/test_web_stack.c` cover successful requests, binary POSTs,
Content-Type injection, URL rejection, HEAD, IO errors and composed behavior.
