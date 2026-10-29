# mulib-uri

`mulib-uri.h` parses ASCII URI references and resolves them using RFC 3986
section 5.2. It is not a network client or an IRI/IDNA implementation.

## Integration and ownership

Define `MULIB_IMPL_URI` and `MULIB_IMPL_STRBUF` in exactly one C11 translation
unit before including the header. No external libraries are needed.

`mulib_uri_t` contains borrowed `(data,length)` slices. Keep the original bytes
alive and unchanged. Slices are **not** NUL-terminated. Presence flags distinguish
`?` from no query, `#` from no fragment, and an empty port from no port.
The host slice includes brackets around IPv6/IPvFuture literals.

## API

- `mulib_uri_parse(text,length,&uri)` validates exactly the supplied bytes and
  returns 1; failure returns 0 without changing `uri`. Empty references are valid
  (pass a non-NULL pointer even for length zero).
- `mulib_uri_compose(&uri)` validates components and returns malloc-owned text,
  or NULL for an invalid structure/allocation failure. Components cannot inject
  new delimiters. A path starting `//` with `has_authority==0` is emitted with a
  protective `/.` prefix, so it cannot become an authority. Release with `free`.
- `mulib_uri_resolve(&absolute_base,&reference)` returns malloc-owned resolved
  text or NULL. Inputs are validated before use. The base must have a scheme.

Schemes, authority/userinfo, reg-name hosts, bracketed IPv6 and IPvFuture, numeric
ports, paths, queries and fragments are supported. Generic URI ports can be empty
or larger than 65535; application protocols must apply their own port policy.
Empty hosts are valid generically (e.g. `file:///a`). IPv6 zone identifiers are
not supported. Reg-name hosts are syntactically validated, not checked as DNS
names. Unbracketed numeric names are reg-names, not an IPv4 validity assertion.

Raw non-ASCII bytes, whitespace, controls, malformed escapes, bad literal syntax,
and illegal component characters are rejected. Percent escapes are preserved,
including escaped NUL and escaped delimiters; **never** treat a decoded path as
already validated for filesystem or HTTP purposes. Resolution removes literal
dot segments, not `%2e`, and preserves case, escapes, repeated slashes, and empty
query/fragment distinctions. It does not normalize hosts or default ports.
If removing dot segments exposes a leading `//` without an authority, the result
retains that distinction using `/.`: resolving `..//c` against `file:/a/b`
produces `file:/.//c`, not `file://c`. This protective dot segment is intentional;
the same rule applies to manually composed no-authority paths, with or without
a scheme. Parsed references still compose byte-for-byte unchanged.
Use `mulib-url.h` separately for percent-encoding/decoding individual components;
this module deliberately never decodes URI syntax during parsing.

## Runnable example

See [`examples/modules/uri.c`](../examples/modules/uri.c):

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/uri.c -o ./uri
./uri
```

It resolves `../c?new#part` against `https://example.org/a/b?old`.
