# MuLIB kvdb: binary-safe persistent snapshots

**Header:** `include/mulib-kvdb.h`  
**Implementation switches:** `MULIB_IMPL_KVDB`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_VARINT`, `MULIB_IMPL_CRC32`, `MULIB_IMPL_ENDIAN`  
**Link flags/external libraries:** none.

## Scope and design

A database owns a sorted in-memory vector of copied binary keys and values.
Lookup uses binary search; insertion/removal shifts the vector. This is useful
for small/medium persistent tables, not a large disk-resident B-tree, SQL engine,
append log, or multi-writer transaction service. Keys/values can be empty or
contain arbitrary NUL bytes; all data APIs take explicit `size_t` lengths. NULL
with length zero is valid. Keys compare lexicographically as unsigned bytes,
with a shorter equal-prefix key first. There is exactly one value per key.

Buffer, LEB128, CRC32, and endian operations reuse the corresponding MuLIB
modules. The string-keyed `mulib-map` is intentionally not used: it cannot index
embedded-NUL keys. Dependency implementation switches are never enabled
implicitly. Public declarations need only `<stddef.h>` and C/C++ linkage.

Reads and in-memory operations use ISO C11. Atomic persistent commits require
POSIX Unix/Apple APIs; elsewhere commit explicitly returns `ENOTSUP`. OS headers
are implementation-only and do not set global feature-test macros.

## API and ownership

`mulib_kvdb_open(path, create, &db)` allocates an opaque owning database. Pass
`create=0` to require an existing snapshot; `create=1` also accepts a missing
path and starts an empty dirty database. Missing files are **not** created until
commit. Empty existing files are corrupt, not empty databases. Every existing
snapshot is completely read and integrity-checked before open succeeds. Output
is unchanged on failure.

| API | Contract |
| --- | --- |
| `mulib_kvdb_size(db)` | Pair count; NULL returns zero |
| `mulib_kvdb_dirty(db)` | Boolean changed/new snapshot; NULL returns zero |
| `mulib_kvdb_get(db, key, key_size, &value, &value_size)` | Borrow value or return ENOTFOUND |
| `mulib_kvdb_set(db, key, key_size, value, value_size)` | Copy insert/replace; safe for borrowed key/value arguments |
| `mulib_kvdb_remove(db, key, key_size)` | Remove or return ENOTFOUND without changing state |
| `mulib_kvdb_at(db, index, &key, &key_size, &value, &value_size)` | Borrow pair at sorted enumeration index |
| `mulib_kvdb_commit(db)` | Explicitly persist the current snapshot |
| `mulib_kvdb_close(db)` | Release everything; NULL allowed; never commits |

Borrowed key pointers survive until their removal/close; value pointers until
their replacement/removal/close. Enumeration indexes can change on insertion or
removal. Commit does not invalidate borrowed pointers. Failure preserves the
logical database; successfully replacing even an identical value marks it dirty.
Output parameters change only on success. No internal locks: coordinate all
access to a mutable object yourself.

## Commit and failure semantics

A dirty commit serializes a complete new snapshot in memory, exclusively creates
a mode-0600 temporary sibling named `PATH.mulib-PID-SEQUENCE`, writes all bytes,
fsyncs the temporary file, closes it, atomically renames it over `PATH`, then
fsyncs the parent directory. Up to 128 occupied temporary names are skipped.
Existing temporary files are never overwritten/deleted. Pre-replacement errors
close/unlink only the temporary file created by this attempt and leave the old
snapshot untouched. This is not an in-place partial update.

Return codes distinguish the important rename boundary:

- `MULIB_KVDB_OK` (0): commit succeeded, or the database was already clean.
- `MULIB_KVDB_UNCERTAIN` (1): replacement **already succeeded**, but parent
  directory fsync failed. New contents are visible; power-loss durability cannot
  be promised. Do not treat this as a rollback. Dirty is cleared.
- Negative result: replacement did not occur; dirty remains set and commit can
  be retried. Allocation, directory open, exclusive creation, write, file fsync,
  close, or rename can fail.

OK/UNCERTAIN both clear dirty. An unchanged clean commit does not rewrite or
revalidate a path changed externally. Filesystems/hardware must honor atomic
rename and fsync for the usual crash-consistency guarantee. Old files replaced
by a commit acquire the new snapshot's 0600 mode; ownership/ACLs/hardlinks are not
preserved. No implicit locking or conflict detection: two processes may read old
snapshots and overwrite one another (last completed commit wins). Use an external
lock for coordinated updates. Use a **trusted, stable parent directory**; this
is not a defense against an attacker renaming directory components. Input paths
are ordinary NUL-terminated native file paths, and reads follow symlinks; rename
replaces the path itself. Descriptor close-on-exec setup is not atomic with a
concurrent fork/exec.

## On-disk format and validation

The file contains, in order:

1. Eight bytes `4d 75 4b 56 44 42 01 00` (`MuKVDB`, version 1, NUL).
2. Canonical unsigned LEB128 pair count.
3. For each sorted pair: canonical LEB128 key length, canonical LEB128 value
   length, exact key bytes, exact value bytes.
4. A little-endian IEEE CRC-32 of **all** preceding bytes.

The decoder rejects truncated/overflowing/nonminimal varints, unavailable byte
ranges, duplicate/out-of-order keys, unsupported magic/version, extra bytes,
and checksum mismatches. It bounds pair allocation by remaining file bytes and
checks allocation arithmetic. CRC detects accidental corruption, **not** hostile
tampering or cryptographic authenticity. Whole-file reads, live values and
serialization all need memory; there is no streaming or configurable quota.

Other negative results: `EINVAL`, `ENOMEM`, `EIO`, `ECORRUPT`, `ENOTFOUND`,
`ENOTSUP`, `EOVERFLOW`. Use the result rather than assuming an `errno` contract
for all stdio/format failures. A missing open returns ENOTFOUND, not EIO.

## Runnable example (POSIX persistence)

```c
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_VARINT
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_KVDB
#include "mulib-kvdb.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    mulib_kvdb_t *db = NULL;
    const void *value;
    size_t size;
    int result;
    assert(mulib_kvdb_open("kv-demo.tmp", 0, &db) == MULIB_KVDB_ENOTFOUND);
    assert(mulib_kvdb_open("kv-demo.tmp", 1, &db) == 0);
    assert(mulib_kvdb_set(db, "k\0", 2, "v\0x", 3) == 0);
    result = mulib_kvdb_commit(db);
    assert(result == 0 || result == MULIB_KVDB_UNCERTAIN);
    mulib_kvdb_close(db);
    assert(mulib_kvdb_open("kv-demo.tmp", 0, &db) == 0);
    assert(mulib_kvdb_get(db, "k\0", 2, &value, &size) == 0 && size == 3);
    mulib_kvdb_close(db);
    assert(remove("kv-demo.tmp") == 0);
    return 0;
}
```

Run only in a scratch directory with no existing `kv-demo.tmp`. Build the full
example using
`cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/kvdb.c -o kvdb-example`.
Tests reopen snapshots, cover binary/empty/prefix keys, uncommitted close,
removal/replacement, every corrupted/truncated byte, valid-CRC malformed records,
and deterministic pre-rename failures that preserve the previous file.
