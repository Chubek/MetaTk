# mulib-fileserver

`mulib-fileserver.h` maps strict origin-form targets to safe relative filenames,
loads bounded bytes, and creates owned GET/HEAD responses. Portable C11 uses an
explicit filesystem/virtual-filesystem loader. An opt-in POSIX.1-2008 loader
provides real root-directory-relative file IO with symlinks denied.

## Integration

Define `MULIB_IMPL_FILESERVER`, `MULIB_IMPL_WEBFRAMEWORK`, `MULIB_IMPL_HTTP`,
`MULIB_IMPL_URI`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_INET4`, and `MULIB_IMPL_INET6` in one translation unit. Include
`mulib-fileserver.h`. No implementation switch is implicitly enabled. C++
declarations have C linkage.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/fileserver.c -o /tmp/opencode/fileserver
/tmp/opencode/fileserver
```

The portable assert example serves an immutable binary virtual file. The bounded
loader intentionally does not call `mulib_fs_read`: that whole-file API has no
size bound and follows symlinks. The POSIX loader reuses `mulib_buffer_t`; all
HTTP responses reuse the shared HTTP parser/serializer and framework reply
builder.

## Strict mapping policy

`mulib_fileserver_path(target, index_name, path)` returns a malloc-owned relative
path on HTTP OK; free it with `free`. Output is unchanged on failure. NULL index
selects `index.html`; an explicit index must be one safe non-hidden component.

* Target is an origin path beginning `/`, without authority, scheme or fragment.
  Query is syntactically validated then ignored.
* Percent escapes are decoded exactly once within path components. Decoded `/`,
  backslash, percent, NUL, controls, non-ASCII and punctuation outside
  `[A-Za-z0-9_.-]` are rejected. Encoded separators are not converted to structure.
* Empty/repeated-slash components and every component beginning `.` are rejected.
  This rejects `.`/`..`, encoded traversal and dotfiles. No dot-segment removal
  silently turns an unsafe target into a safe one.
* `/` and a trailing slash append index_name. There are no directory listings.
* Target and index strings each have a 32768-byte bound; allocation and append
  overflow checks are inherited from strbuf.

This deliberately narrow ASCII filename grammar does not support spaces,
Unicode names, hidden files, Windows drive paths or percent-literal filenames.
It is not a general URL-to-native-path decoder.

## Serving and portable loader

`mulib_fileserver_serve(request, options, load, user, response)` validates the HTTP
request, then invokes `load(user, relative_path, maximum, metadata_only, data,
length)`. The options default to 16 MiB maximum, `index.html` and
`application/octet-stream`. Non-NULL content type is HTTP-validated; responses
also include `X-Content-Type-Options: nosniff`.

Loader path/user are borrowed until return. The loader MUST enforce confinement
under its trusted root, reject symlinks in **all** components and reject
non-regular files. It must not percent-decode again. An immutable virtual
filesystem naturally satisfies these requirements. Checking a path and then
calling ordinary `fopen` is **not** race-safe and is not an acceptable loader for
an attacker-writable filesystem.

For GET, FOUND (1) supplies malloc-owned data and its explicit binary length;
empty files can supply NULL. For HEAD (`metadata_only != 0`), supply length and
no data without reading the body. Outputs start NULL/zero. Any returned data
transfers to the server and is freed on **every** result, including failure.
Do not return borrowed/static memory. Keep logical length within maximum; the
server independently rejects an over-limit FOUND result, but cannot undo a
loader's oversized allocation or a false length.

Loader results map as follows: FOUND → 200; NOT_FOUND (0) → 404; FORBIDDEN (-1)
→ 403; LIMIT (-2) → 413; IO (-3)/unknown → 500; NOMEM (-4) → HTTP NOMEM.
Malformed/unsafe mapped targets become 403 without invoking the loader. Other
methods get 405 with `Allow: GET, HEAD`. Library/HTTP validation errors are
returned rather than converted to responses. Error-status bodies are empty.

On HTTP OK, output owns independent HTTP allocations and is released with
`mulib_http_message_free`. On failure it is unchanged; do not pass an output
already holding live allocations. HEAD preserves representation Content-Length
with zero body. Serialize HEAD responses using `MULIB_HTTP_HEAD_RESPONSE`.

## Optional POSIX loader

Before **any system headers**, define `_POSIX_C_SOURCE=200809L`. Define
`MULIB_FILESERVER_POSIX` consistently in code needing its declaration and in the
file-server implementation translation unit. Then pass
`mulib_fileserver_posix_load` as loader and a pointer to an application-owned
`int root_fd` as user. Open that fd on a trusted directory first, for example
with `open(root, O_RDONLY | O_DIRECTORY)`, and close it after all calls finish.

The loader duplicates the fd, walks each component using `openat` with
`O_NOFOLLOW` and intermediate `O_DIRECTORY`, and checks the final file with
`fstat` for a regular-file type. `O_NONBLOCK` avoids hanging on a FIFO before
the type check. It checks stat size, then reads in 4096-byte blocks, handles
EINTR, and rejects growth beyond maximum. HEAD uses metadata only. All temporary
descriptors/buffers are released; the supplied root fd is not consumed. This
requires POSIX openat/O_NOFOLLOW/O_DIRECTORY; it is not enabled on other
platforms by default.

There is no fallback that follows symlinks. Descriptor-relative walking avoids
symlink-check/use races, but is **not a sandbox**: hard links, bind mounts,
renamed open directories, filesystem permission policy and mutable contents
remain application responsibilities. Use a trusted root/tree with controlled
writers. Files modified during reads are not a consistent snapshot. Close
errors on read-only descriptors are not surfaced. Geometric buffers and
response serialization mean max_bytes is not a total-memory budget.

No sockets, TLS, content negotiation, ranges, conditional requests, cache
validation, compression or MIME guessing are provided. The focused suite tests
traversal variants, binary GET/HEAD, limits, missing/forbidden/IO errors, loader
allocation failure, method policy and (on POSIX) real symlink/FIFO rejection.
