# mulib-vfs

`mulib-vfs.h` implements a lexical mount table and a useful in-memory filesystem.
It does not mount anything in the operating system, resolve native symlinks,
enforce permissions, or implement a POSIX filesystem. It is ISO C11; declarations
have C++ linkage guards. There are no third-party or MuLIB dependencies.

## Integration and runnable example

Define `MULIB_IMPL_VFS` in exactly one C translation unit before including the
header. Other translation units include declarations only. No implementation
switch is enabled by the header.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/vfs.c -o /tmp/vfs-example
/tmp/vfs-example
```

The example mounts an owned memory store at `/memory`, normalizes a path, reads
and writes file bytes, lists a directory, and checks a traversal rejection.

## Paths and routing

`mulib_vfs_normalize(path, out, capacity)` requires a NUL-terminated absolute
slash-separated byte string of at most **4095 bytes**, excluding NUL. It collapses
repeated slashes and `.` components and applies `..` lexically. Attempting to pop
above root is `EINVAL`, not a clamped path. Trailing slashes disappear except for
root. Backslashes, bytes 0x01..0x1f and 0x7f are rejected; bytes >= 0x80 are opaque,
not Unicode-validated. `capacity` includes the NUL. Failure leaves output
unchanged; input and output may alias, including in-place normalization.

`mulib_vfs_init(&vfs, max_mounts)` initializes an **uninitialized or freed** object.
The limit must be positive and fit allocation arithmetic. Do not reinitialize a
live object: free it first. `mount` copies the normalized path and operation table
and borrows its context. Duplicate normalized paths fail with `EEXIST`. Mounts
need not have a parent directory. `/data` matches `/data` and `/data/x`, never
`/database`. Routing chooses the longest matching mount; no match is `ENOENT`.
The backend receives `/` for the mount itself and `/x` for a child. Normalization
happens **before** routing: `/data/../else` routes as `/else`.

`unmount` removes an exact normalized mount. `free` releases names and the table,
zeros the object and never destroys backend contexts. A backend must remain
alive and at a stable address until all mounts using it are removed/freed.
Operation tables are copied even when supplied from the existing mount table.

## Backend operations

The four callbacks in `mulib_vfs_ops` implement:

| Operation | Contract |
| --- | --- |
| `read` | Offset/capacity byte read. EOF is success with zero bytes. A successful short read denotes EOF; bytes are not NUL-padded. |
| `write` | Offset/length byte write. Creation is backend-defined; no truncation is implied. A successful short write is allowed and reported. |
| `stat` | Return `MULIB_VFS_FILE` or `MULIB_VFS_DIRECTORY`, with a `uint64_t` byte size; directory sizes are backend-defined. |
| `list` | Immediate child names only, excluding `.`/`..`; borrowed metadata/name valid only during the callback. Nonzero visitor return must stop enumeration with `ECANCELED`. |

The public wrappers are `mulib_vfs_read`, `mulib_vfs_write`,
`mulib_vfs_getstat` and `mulib_vfs_list`. Read/write require a count pointer and
set it to zero before validation; NULL data is valid only for zero capacity or
length. Offset plus capacity/length must fit `size_t`. Backend count results
larger than the supplied buffer are rejected as `EIO` (this cannot undo an
incorrect backend's buffer write). On error, read output bytes may have changed;
write side effects cannot be rolled back by the router. Stat output is unchanged
on failure. The router does not synthesize mount names into directory listings:
an independently mounted child appears only if its parent backend lists it.

A NULL callback yields `ENOTSUP`, except a missing write yields `EROFS`.
Custom backends must honor the normalized-path, valid-type, count and listing
contracts. They may do synchronous nested reads/listings. Do not mutate the
mount table or enumerated backend during dispatch/list callbacks. Objects have
no internal locks; callers synchronize access, including against free/unmount.

## In-memory backend

`mulib_vfs_mem_init(&mem, max_nodes, max_file_bytes)` creates an empty store with
an implicit root directory. Root consumes no node or allocation. `max_nodes` is
positive and arithmetic-checked; `max_file_bytes` may be zero. Both are hard
limits, not initial allocations. Node lookup/listing is linear, and insertion
grows the array to exactly the required count. Total storage is bounded by node
and per-file limits plus copied names/table; there is no separate total-byte
quota.

* `mulib_vfs_mem_mkdir` creates **one** directory; parents must exist. Existing
  names give `EEXIST`; a file parent gives `ENOTDIR`.
* `mulib_vfs_mem_put` copies and completely replaces a file's bytes, including
  zero-length/binary data. It creates a file when absent, but never parents.
  Source bytes may refer to the store's existing bytes. Allocation failure
  preserves the old contents and node count.
* `mulib_vfs_mem_ops()` returns operations by value; mount them with `&mem` as
  context. Routed writes preserve the existing suffix and zero-fill gaps. A
  zero-length write still creates/extends to its offset; it is not a truncation
  operation. Extending/replacing writes are allocation-failure atomic. They use
  a temporary full-file copy, so peak file memory includes two full-size new
  buffers plus the preexisting allocation.
* `mulib_vfs_mem_remove` removes a file or empty directory. A nonempty directory
  gives `EEXIST`; removing root gives `EINVAL`. It is not recursive.
* `mulib_vfs_mem_free` releases all owned names/bytes and zeros the store. Unmount
  it first. List order is insertion order, not sorted. Directory size is zero.

Public struct members permit stack allocation and inspection but must not be
modified by callers. Pointers into tables can be invalidated by mutation.

## Errors and limits

All operations return `mulib_vfs_result`: `OK` (0), or negative `EINVAL`
(bad argument/path), `ENOENT`, `ENOMEM`, `ELIMIT` (configured bound/capacity),
`EEXIST`, `ENOTDIR`, `EISDIR`, `EROFS`, `EIO`, `ENOTSUP`, `ECANCELED`,
or `EFORMAT` (used by composed parsers/indexes). These are library codes, **not
errno numbers**. Backend errors propagate unchanged. Allocator failures are
reported, not aborts. Sizes/offsets are `size_t`, metadata is 64-bit; the supported
model is ordinary 32-/64-bit C targets. No native backend is claimed: portable
memory storage and user-supplied callbacks are the backend implementations.

`tests/test_virtual_files.c` exercises normalization boundaries, mount-prefix
selection, ownership, binary/sparse data, malformed backend counts, dispatch
composition, and injected allocation failures with always-moving realloc.
