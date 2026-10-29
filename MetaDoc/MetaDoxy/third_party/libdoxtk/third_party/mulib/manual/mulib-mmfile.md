# MuLIB mmfile: whole-file shared mappings

**Header:** `include/mulib-mmfile.h`  
**Implementation switch:** `MULIB_IMPL_MMFILE`  
**MuLIB dependencies/link flags:** none.

## Scope and platforms

On POSIX Unix/Apple targets this module maps an existing regular file in its
entirety with `MAP_SHARED`. It supports read-only or read/write access, synchronous
mapping flush, and deterministic unmapping. It does not resize/create files,
map arbitrary offsets, or provide copy-on-write mappings. The mapping can outlive
the open call's descriptor: that descriptor is closed before open returns.

Other platforms explicitly return `MULIB_MMFILE_ENOTSUP`; there is no emulated
heap buffer masquerading as a memory map. System headers and APIs occur only in
the implementation region; no `_POSIX_C_SOURCE` or other global feature macro is
defined. Native `off_t`, address-space, and `size_t` limits apply.

## API and lifecycle

```c
mulib_mmfile_t m = MULIB_MMFILE_INIT;
int mulib_mmfile_open(mulib_mmfile_t *m, const char *path, int writable);
int mulib_mmfile_flush(mulib_mmfile_t *m);
int mulib_mmfile_close(mulib_mmfile_t *m);
```

`writable` must be exactly zero or one. Read-only mappings must never be written
even though the public `data` pointer has type `void *`. `size` is the file size
observed when opening, `writable` describes access, and `active` distinguishes an
open empty mapping from a closed object. Empty files succeed without `mmap`, with
NULL data and size zero. An active mapping cannot be reopened; close it first.
Opening failure leaves the initialized object unchanged. Do not copy live
mappings or edit their fields.

Flush calls `msync(MS_SYNC)` for nonempty writable mappings. Empty, read-only,
and closed initialized objects are no-ops on POSIX. This flushes mapped data,
not an atomic transaction or directory metadata. Close does not implicitly do a
synchronous flush. On successful close all fields reset, and repeated close is
safe. A failing unmap retains the object for retry. All borrowed pointers become
invalid at close. No mutable shared object is internally synchronized.

**The caller must prevent concurrent file truncation**: accessing a mapping
beyond a shortened file can cause SIGBUS. Changes from other processes are not a
consistent snapshot. Opening a directory/device is rejected. File size checks do
not prevent concurrent modifications between `fstat`, `mmap`, and later access.
The temporary descriptor is marked close-on-exec using `fcntl`; the open-to-fcntl
window is not atomic with another thread's fork/exec.

## Errors

Zero means success. `EINVAL` means invalid arguments/nonregular file, `ESTATE`
means already active, `EOVERFLOW` means an unrepresentable file size, `EIO` means
an OS failure, and `ENOTSUP` means unsupported platform. For `EIO`, `errno`
retains the failing OS call's error even when cleanup needs other calls. Do not
infer an `errno` value for other result codes.

## Runnable example

```c
#define MULIB_IMPL_MMFILE
#include "mulib-mmfile.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    const char *path = "mulib-map-demo.tmp";
    mulib_mmfile_t m = MULIB_MMFILE_INIT;
    FILE *f = fopen(path, "wbx");
    assert(f && fwrite("abc", 1, 3, f) == 3 && fclose(f) == 0);
    assert(mulib_mmfile_open(&m, path, 1) == 0 && m.size == 3);
    ((char *)m.data)[1] = 'B';
    assert(mulib_mmfile_flush(&m) == 0);
    assert(mulib_mmfile_close(&m) == 0);
    assert(remove(path) == 0);
    return 0;
}
```

The inline example requires POSIX and a writable scratch working directory; its
exclusive `fopen` refuses to overwrite an existing file. A platform-aware version
is `examples/modules/mmfile.c`. Build with
`cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/mmfile.c -o mmfile-example`
and run in a scratch directory. Tests cover empty files in both access modes,
reopening state, shared binary writes/flushes, and failure cleanup.
