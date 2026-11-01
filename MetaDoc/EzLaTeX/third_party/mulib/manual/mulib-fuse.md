# mulib-fuse

This module is a **userspace low-level request-record dispatch adapter** for
`mulib-vfs`, with no third-party dependency. It is **not** a kernel filesystem,
libfuse ABI, Linux FUSE wire-protocol parser, inode/open-handle table,
`/dev/fuse` transport or mount implementation. Calling it never makes a host
mount. An application may place its own transport above these typed records;
the records must not be sent directly to the kernel as FUSE packets.

## Integration and example

Provide `MULIB_IMPL_FUSE` **and** its sole dependency `MULIB_IMPL_VFS` in exactly
one C implementation translation unit each. Include `mulib-fuse.h` normally;
dependency implementation switches are never enabled implicitly. C11 is
required; declarations can be included from C++.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/fuse.c -o /tmp/fuse-example
/tmp/fuse-example
```

This runs a real VFS read dispatch and a read-only write rejection against the
in-memory backend, without privileges or kernel mounts.

## Adapter and ownership

`mulib_fuse_init(&adapter, &vfs, read_only)` borrows an initialized VFS at a stable
address. A nonzero `read_only` rejects writes before backend dispatch. The
adapter allocates nothing, needs no destructor, owns no VFS/backend, and does not
retain requests, responses, buffers, visitors or user pointers. The VFS and its
mounted contexts must outlive dispatch. Calls are synchronous; callers provide
all synchronization and may not mutate the mount table during a dispatch.

## Request/response contract

`mulib_fuse_dispatch(adapter, request, output, output_capacity, visit, user,
response)` initializes the entire response, copies `request.id` (zero for a NULL
request), and returns the same result stored in `response.result`. A NULL
response returns `EINVAL` without writing anything. For other invalid arguments
the response is still initialized. IDs are opaque 64-bit values, not allocated
or checked for uniqueness.

| Opcode | Inputs | Response |
| --- | --- | --- |
| `MULIB_FUSE_GETATTR` | Absolute `path` | `info.kind`/`info.size` on success. |
| `MULIB_FUSE_READ` | `path`, `offset`, requested `length`; caller output buffer | `transferred` actual bytes; successful EOF is zero. No NUL padding. Requested length exceeding output capacity is `ELIMIT`, not silent clamping. |
| `MULIB_FUSE_WRITE` | `path`, `offset`, borrowed `data`, `length` | Actual `transferred` bytes; offset write, no truncation. NULL data is allowed only for zero length. |
| `MULIB_FUSE_READDIR` | `path`, non-NULL visitor/user | Calls the VFS immediate-child visitor and counts callbacks in `transferred`. Cancellation returns `ECANCELED`, including the final callback in the count. |

Unused request fields/output/visitor arguments are ignored for the selected
operation. Unknown opcodes return `ENOTSUP`, not apparent success. Failed
reads/writes leave `transferred` zero, but a backend may already have produced
buffer or filesystem side effects. READDIR can deliver entries before failure.
Response metadata is zero for operations other than successful GETATTR.

Paths use VFS normalization, component-prefix routing and the 4095-byte input
limit. Malformed/traversal paths are rejected by VFS. Listing order and mount
visibility are backend-defined as documented by VFS. There is no directory
pagination/cookie protocol: one READDIR enumerates once until completion or
cancellation. There are no hidden request queues, file handles, permissions,
thread pools, native calls or platform-specific fallback behavior.

## Errors and tests

Results use the shared `mulib_vfs_result` codes, not errno. `EINVAL` covers NULL
arguments, malformed paths or offset arithmetic overflow; `ELIMIT` covers read
buffer mismatch/path limits; `EROFS` covers the adapter or backend being
read-only; `ENOTSUP` covers an unknown opcode or missing operation. Backend
errors (`ENOENT`, `EISDIR`, etc.) propagate. Dispatch itself has no allocations;
the routed backend may fail with `ENOMEM`.

`tests/test_virtual_files.c` covers GETATTR/READ/WRITE/READDIR with VFS memory,
read-only policy, short output capacity, cancellation counts, invalid requests,
unknown opcodes, and traversal rejection.
