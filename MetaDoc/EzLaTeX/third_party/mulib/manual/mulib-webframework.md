# mulib-webframework

`mulib-webframework.h` supplies an owned exact-route registry, dispatcher and
binary response builder using `mulib_http_message_t`. It is an application core,
not a listener, web runtime or authorization framework.

## Build

Define `MULIB_IMPL_WEBFRAMEWORK` and `MULIB_IMPL_HTTP`, `MULIB_IMPL_URI`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_INET4`, `MULIB_IMPL_INET6` once, then include the header. Dependency
switches are never defined implicitly. Declaration-only C++ inclusion is
supported.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/webframework.c -o /tmp/opencode/webframework
/tmp/opencode/webframework
```

The offline example registers GET, parses a HEAD request with a query, and
verifies GET fallback with the representation length preserved.

## Registry and matching

`mulib_webframework_init(router, max_routes)` initializes fresh/empty storage;
zero selects 64. Reinitializing a live router leaks registrations: free it first.
`mulib_webframework_route(router, method, path, handler, user)` copies method and
path. Handler and user are borrowed and must outlive dispatches. Method must be
a nonempty HTTP token; path must be an origin path beginning `/` with no
authority, scheme, query or fragment. Each string is limited to 32768 bytes.
Duplicate method/path pairs are INVALID. The configured count limit is enforced
before allocation and allocation multiplication is checked.

Matching is byte-exact and case-sensitive, including percent escape spelling.
Request query is ignored. There are no captures, prefixes, wildcard methods,
automatic slash normalization or percent decoding. HEAD uses an explicit HEAD
route if present, otherwise the matching GET route. Every unmatched method/path
returns an owned 404 response, deliberately not an automatic 405/Allow list.
Registering `/a` does not match `/a/`, `/A` or `/%61`.

`mulib_webframework_free` frees strings and the route array, zeroes the object,
and does not destroy user contexts. NULL is safe. Do not shallow-copy a live
router, mutate its public bookkeeping, or register/free while dispatching.
Concurrent read-only dispatch is possible only if handlers/contexts themselves
are safe; no synchronization is provided.

## Handlers and response construction

A handler receives borrowed `(user, request, response)`; response starts zeroed.
On OK it must contain a valid, fully **owned** HTTP response. Borrowed literals
must not be inserted directly into an owned output; use
`mulib_webframework_reply(response, status, content_type, body, length)` instead.
That builder borrows its inputs and copies/validates through the HTTP serializer
and parser, producing canonical framing. NULL Content-Type omits it. Bodies are
binary; NULL with nonzero length is invalid. Status must be 100..599 and bodyless
statuses must have empty bodies. Common status reason strings are supplied;
other codes use `Response`.

Handlers return `mulib_http_result`. On failure the dispatcher frees any owned
partial response. MORE from a handler is converted to INVALID, not propagated as
a retry protocol. On success handler output is serialized for syntax/framing
validation before ownership reaches the caller. A handler must still return
structurally valid owned storage even on error so cleanup is safe.

For HEAD, a nonempty ordinary handler response is canonicalized to retain a
generated Content-Length and its body is freed/suppressed. A metadata-only HEAD
handler can directly return an owned response with its representation
Content-Length and zero body. Chunked handler output for HEAD is rejected under
the shared strict HTTP HEAD policy. When later serializing a dispatched HEAD
response, pass `MULIB_HTTP_HEAD_RESPONSE` to HTTP serialization as well.

## Errors, ownership and scope

Dispatch first validates the request through HTTP serialization, then requires
origin-form (no absolute-form/authority/asterisk dispatch). On success the output
owns independent allocations; call `mulib_http_message_free`. On error it is
unchanged and must not already own allocations. Reply has the same output rule.
Route addition leaves existing registrations unchanged on failure. Results are
HTTP OK (1), INVALID (-1), LIMIT (-2), NOMEM (-3). Router initialization/free
do not allocate; other operations can fail allocation.

Default HTTP limits apply: 32768 header bytes, 100 fields and 16 MiB body. There
is no per-router HTTP override; enforce smaller ingress limits when parsing.
Validation/response copying can temporarily allocate multiple message bodies.

The module has no sockets, TLS, sessions, cookies, middleware, templates,
decompression or implicit authentication. Exact path matching is not filesystem
confinement; use [mulib-fileserver](mulib-fileserver.md) for static file mapping.
Do not decode a route differently after using its raw spelling for authorization.
Ingress Host/forwarded-header policies, IO timeouts, CSRF and authorization belong
to the application. `tests/test_web_stack.c` tests registration ownership,
duplicates, limits, binary replies, HEAD, malformed/injected inputs and offline
client/file server composition.
