# MuLIB archive: checked USTAR memory archives

**Header:** `include/mulib-archive.h`  
**Implementation switches:** `MULIB_IMPL_ARCHIVE`, `MULIB_IMPL_BUFFER`  
**Link flags:** none. ISO C11; declarations are C++17-compatible.

## Scope and safety

This is an uncompressed POSIX USTAR reader and deterministic writer for regular
files and directories. It does **not** extract files. It rejects links, devices,
FIFOs, PAX/GNU extension records, sparse records, non-octal/base-256 numeric fields,
and non-USTAR headers instead of interpreting them as ordinary files. It is not
a general-purpose tar compatibility layer or compression library.

Both reading and writing require portable relative paths: no leading `/`, any
backslash or colon, empty components, `.` components, or `..` components. A final
slash is allowed only for directories. Paths are byte strings, not necessarily
UTF-8. They fit the USTAR 155-byte prefix plus slash plus 100-byte name (256 bytes
at most); some shorter names still cannot be represented if no suitable slash
exists. A writer reports `EOVERFLOW` for those names. Duplicate paths are allowed;
no normalization or duplicate resolution occurs.

Checksums use unsigned header bytes with the checksum field treated as spaces.
The reader requires the `ustar\0` magic and `00` version, checks payload bounds,
requires zero-size directories, and accepts regular-file type `0` or NUL.
Two zero blocks terminate an archive; any remaining bytes must also be zero.
An entry being valid does not establish that later entries or the end marker are
valid: keep calling `mulib_archive_next` until it returns zero. Input must remain
unchanged for the entire iteration.

There is deliberately no extraction API. **Do not treat the lexical path check as
a filesystem sandbox**: an extractor would additionally need symlink-safe,
directory-relative traversal, resource limits, duplicate policy, and safe mode
handling. Reader metadata can contain privileged mode bits; do not blindly apply
it to a filesystem.

## API and ownership

- `mulib_archive_safe_path(path, directory)` returns a boolean lexical check.
- `mulib_archive_reader_init(&reader, bytes, size)` borrows a complete byte range.
  The size must be at least 1024 and a multiple of 512. Failed initialization
  leaves the reader unchanged.
- `mulib_archive_next(&reader, &entry)` returns `MULIB_ARCHIVE_ENTRY` (1),
  `MULIB_ARCHIVE_OK` (0, valid end), or a negative error. An error poisons the
  cursor; subsequent calls return `ESTATE` until reinitialization. End is
  idempotent. The entry output changes only on an entry result.
- `mulib_archive_entry_t` contains its own terminated `path[257]`; its `data`
  pointer borrows the archive. `size`, `mode`, `uid`, `gid`, `mtime`, and canonical
  type `'0'`/`'5'` are decoded scalars. Directory data has size zero.
- `mulib_archive_writer_init(&writer)` starts an empty owning buffer. It does
  not free an already live writer. Never copy a live writer.
- `mulib_archive_add(&writer, path, data, size, mode, mtime)` copies a regular
  file. `(NULL, 0)` is valid data. Input and path must not alias writer storage.
- `mulib_archive_add_directory(&writer, path, mode, mtime)` adds a directory.
  Writer uid/gid are zero, mode is limited to `07777`, and size/mtime to
  8589934591 (eleven octal digits). Names, user/group strings, and unused fields
  are otherwise zero-filled. Writer metadata does not consult the clock.
- `mulib_archive_finish(&writer)` appends exactly two zero blocks. Repeating it
  succeeds without adding bytes. Further additions return `ESTATE`.
- Borrow `writer.bytes.data` and `.length` for the completed archive; additions
  or finishing can reallocate it. `mulib_archive_writer_free` releases/reset it.

Writer operations return zero on success. Failed additions preserve the logical
buffer length/content, though capacity and its address may have changed.
Results are `EINVAL` (bad arguments), `EFORMAT` (malformed/truncated input),
`EUNSAFE` (path/type policy), `ENOMEM`, `EOVERFLOW`, or `ESTATE`. No `errno`
contract or global error state. No implicit dependency implementation switches.

## Runnable example

```c
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ARCHIVE
#include "mulib-archive.h"
#include <assert.h>
#include <string.h>
int main(void) {
    mulib_archive_writer_t w;
    mulib_archive_reader_t r;
    mulib_archive_entry_t e;
    mulib_archive_writer_init(&w);
    assert(mulib_archive_add(&w, "hello.txt", "hello", 5, 0644, 0) == 0);
    assert(mulib_archive_finish(&w) == 0);
    assert(mulib_archive_reader_init(&r, w.bytes.data, w.bytes.length) == 0);
    assert(mulib_archive_next(&r, &e) == 1 && e.size == 5);
    assert(!memcmp(e.data, "hello", 5));
    assert(mulib_archive_next(&r, &e) == 0);
    mulib_archive_writer_free(&w);
    return 0;
}
```

Build the fuller example with
`cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/archive.c -o archive-example`
and run `./archive-example`. `tests/test_storage.c` covers long USTAR paths,
binary data, forbidden types/paths, bad checksums/numbers, truncation, and endings.
