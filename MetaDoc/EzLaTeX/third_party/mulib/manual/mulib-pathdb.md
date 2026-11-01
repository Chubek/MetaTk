# mulib-pathdb

An owned, sorted path/metadata index with component-prefix and substring queries,
transactional VFS snapshots, and a small portable binary persistence format.
This is not an OS crawler, watcher, live database, content index or SQL engine.
There are no third-party dependencies, native syscalls or feature-test macros.

## Integration and runnable example

Define `MULIB_IMPL_PATHDB` and dependency `MULIB_IMPL_VFS` exactly once each;
include `mulib-pathdb.h` for C11 or C++ declarations. Dependencies are not
implemented implicitly.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/pathdb.c -o /tmp/pathdb-example
/tmp/pathdb-example
```

The example snapshots a VFS directory, persists/restores it through an owned
`tmpfile()` stream, and finds a `.txt` file by prefix and substring. It requires
no native filesystem mount, network service or special permissions.

## Index and ownership

`mulib_pathdb_init(&db, max_entries)` initializes an uninitialized/freed object.
The bound must be in **1..UINT32_MAX** and fit table allocation arithmetic.
Initialization allocates nothing. Do not initialize over a live object; call
`mulib_pathdb_free` first. Free releases all owned paths/table and zeros the
object. Public struct members permit stack allocation and inspection, not caller
mutation.

`mulib_pathdb_put(db, path, &info)` normalizes a VFS absolute path, copying the
path and metadata (`kind`, `size`). Only file/directory kinds are accepted. Root
and even nonexistent paths are valid: put is an indexing operation, not an
existence check. Paths use VFS's **4095-byte input limit**, control/backslash
rejection and above-root traversal rules. Existing normalized names update
metadata without allocation, including at the entry limit. Input metadata may
refer to another existing index entry. Allocation failure leaves the old index
unchanged. `remove` removes one exact normalized path, not its descendants.

Path strings are owned and independent of caller/backend strings. Entry pointers
may be invalidated by mutation; do not retain callback pointers across calls.
Metadata is a snapshot, not a live reference. There are no destructor callbacks
or user value pointers. All calls are synchronous with caller-provided locking.

The table is sorted with bytewise `strcmp`; lookup position is binary-searched,
insertion/removal shifts entries. Insertions use two allocations (name and exact
new table size). Query scans a sorted prefix range and substring-matches full
paths; worst-case query is linear in entries/path bytes. This favors modest
indexes and predictable semantics, not large-scale filesystem search.

## Query

`mulib_pathdb_query(db, prefix, fragment, visit, user, &visited)` normalizes prefix;
NULL means `/`. Matching is **component-aware**: `/a` matches `/a` and `/a/b`,
not `/ab`. NULL or empty fragment matches all; otherwise a case-sensitive byte
substring must occur anywhere in the full path. There are no globs, regexes,
locale folding or Unicode case conversion.

Results are sorted. Each visitor borrows one entry for that callback. Nonzero
return stops immediately with `ECANCELED`; `visited` counts callbacks, including
the final cancelling callback. `visited` is reset before validation. Both visitor
and visited pointer are required. No match is success with zero callbacks. Do
not mutate the index from its callback.

## VFS snapshot composition

`mulib_pathdb_scan(db, vfs, root)` transactionally replaces the entire index with
root and recursively listed children, preserving `max_entries`. It builds a
temporary index and swaps only on success. Missing roots, unsupported backend
operations, allocation failures, invalid child names/types, duplicate child
paths and limit failures preserve the old index. Backend child names must be
single nonempty components, never `.`/`..`; normalization must not change the
joined name. No symlink type exists in this VFS model.

Recursion accepts at most **64 child-component levels below root**. Listing a
child of a directory already at level 64 fails `ELIMIT`. Recursive stack use is
bounded but can approach roughly 1 MiB because bounded path buffers are kept
per level; choose smaller trees on small-stack systems. Node count and 4095-byte
joined-path bounds are also enforced. While building, both old and new indexes
coexist. The operation is not a coherent filesystem transaction: backends must
be stable during enumeration. It follows backend listings, not synthetic mount
entries. An independently mounted descendant is discovered only if the parent
backend lists its name. Each discovered child is statted through the VFS so a
listed mount point uses the mounted backend's effective type/metadata, not stale
metadata from its parent backend.

## Persistence format and stream contracts

`mulib_pathdb_save(db, stream)` writes from the current position and leaves the
caller-owned `FILE *` open. It does not flush, close, truncate, fsync or atomically
replace a destination. Caller checks flush/close errors and chooses safe
replacement if required. `mulib_pathdb_load(db, stream)` reads from the current
position **through EOF**, rejecting trailing data, and replaces the index only
on full success. Failure may advance the stream or set its error indicators,
but never changes the index. Both use only ISO stdio and work with native files
opened by the caller; the encoded data can also be transported by an application
using MuLIB file/storage helpers.

The deterministic **MPD1** format is:

1. Four literal bytes `MPD1`.
2. Four-byte little-endian unsigned entry count.
3. For each entry in strictly increasing bytewise path order: two-byte
   little-endian path length; one-byte kind (`1` file, `2` directory); eight-byte
   little-endian unsigned size; then exactly path-length non-NUL bytes, without
   NUL padding.

The encoded length is `8 + sum(11 + path_length)`. An empty index is eight bytes.
Each path length must be 1..4095. Loaded paths must already be normalized,
absolute and valid; embedded NUL, relative/traversal paths, duplicate or unordered
records, unknown kinds/versions, truncation and extra bytes are rejected. Count
must fit the destination's configured entry limit before allocation. There is no
checksum, compression, endianness dependency, struct-padding serialization,
schema migration or corruption recovery. Loading memory is bounded by entry
limit and path limit plus the old index; it never allocates from unchecked record
length/count multiplication. Input metadata sizes may be any `uint64_t`.

## Errors and tests

All calls use `mulib_vfs_result`, not errno: `EINVAL` for bad arguments/put
metadata/path; `ENOENT` for an absent remove/root; `ELIMIT` for count/path/depth or
record-length limits; `ENOMEM` for allocation; `EFORMAT` for malformed persistence
or backend records; `EIO` for stdio failures; `ECANCELED` for query cancellation.
VFS scan errors propagate (including `ENOTSUP` for a missing operation). A
malformed persisted path always yields `EFORMAT`, even when ordinary put would
report `EINVAL`. No native backend is claimed: scan uses any provided VFS backend.

`tests/test_virtual_files.c` covers sorted prefix/query behavior, copied ownership,
limit/update/removal cases, stream roundtrips and malformed/truncated/trailing
input, recursive VFS snapshots, unchanged failed snapshots, and injected
allocation failure with always-moving realloc.
