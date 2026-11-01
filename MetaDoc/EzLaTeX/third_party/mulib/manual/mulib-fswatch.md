# MuLIB fswatch

**Header:** `include/mulib-fswatch.h` · **Switch:** `MULIB_IMPL_FSWATCH`  
**Dependencies:** none; Linux inotify. C11 / C-linkage declarations.

## Supported native subset

```c
typedef struct mulib_fswatch_event {
    int32_t watch;
    uint32_t mask, cookie;
    const char *name;
    size_t name_length;
} mulib_fswatch_event;
int mulib_fswatch_create(void);
int mulib_fswatch_add(int fd, const char *path, unsigned mask);
int mulib_fswatch_remove(int fd, int watch);
int mulib_fswatch_read(int fd, void *buffer, size_t capacity, size_t *count);
int mulib_fswatch_next(const void *buffer, size_t length, size_t *offset,
                       mulib_fswatch_event *event);
int mulib_fswatch_close(int fd);
```

`create()` calls `inotify_init1(IN_NONBLOCK | IN_CLOEXEC)` and returns a newly
owned fd. `add()` returns a native watch id, replacing interest if the same inode
is already watched on that fd. Watches are **not recursive**. Symlinks follow
native default behavior. No fanotify, polling emulation, Windows/macOS backend,
recursive directory management, only-directory/no-follow/one-shot options, or
automatic rescan is provided. Native operations on non-Linux hosts return
`ENOSYS`; the bounded decoder itself is platform-independent. No late feature
macros, allocations, or worker threads.

## Watch masks and events

The accepted nonempty interest subset uses these `MULIB_FSWATCH_` flags:
`MODIFY`, `ATTRIB`, `CLOSE_WRITE`, `MOVED_FROM`, `MOVED_TO`, `CREATE`, `DELETE`,
`DELETE_SELF`, `MOVE_SELF`. `WATCH_MASK` is their union. No unknown input bits
are silently ignored. Decoder masks retain raw Linux inotify bits, including
`ISDIR`, `UNMOUNT`, `OVERFLOW`, `IGNORED`, and future bits.

Names are directory-relative bytes, not validated Unicode or safe filesystem
paths. No name is supplied for some self/overflow events (`name == NULL`,
`name_length == 0`). Rename matching uses native nonzero cookies, but pairs can
be separated, lost, or refer to locations outside watched directories.
Events coalesce: this is not an audit log. `OVERFLOW` (watch -1) means the
application must rescan/rebuild its state. `IGNORED` means the watch was removed,
explicitly or automatically. Removal leaves its notification queued. Watch ids
are kernel-managed and can eventually be reused; track lifetimes externally.

## Batch reads and alignment-safe decoding

`read()` performs one native read into caller-owned storage. Capacity must be
`16..PTRDIFF_MAX`; use 4096 bytes or more for normal filename-bearing events.
If the next event cannot fit, the OS reports `EINVAL` rather than splitting it.
An empty nonblocking queue reports `EAGAIN`. Output count is unchanged on error.

Initialize `offset = 0` and call `next()` until it returns zero. It returns 1
for one decoded event, 0 only at exact end, or -1/`EINVAL` on malformed input.
It reads the native-endian Linux wire layout (16-byte header followed by native
name length) with memcpy; unaligned input is supported. Length cannot exceed
`PTRDIFF_MAX`. It rejects truncated headers/names, out-of-range offsets, watch
numbers below -1, and nonempty name spans without a NUL. It does not require
padding zeroes or known output mask bits. This is a native event decoder, not a
cross-endian serialized format validator.

On decoding failure, both offset and output remain unchanged; exact end also
does not modify output. The name points into the borrowed input batch and is
valid only until that storage is overwritten/freed. `name_length` excludes the
terminator and native padding. Do not cast arbitrary input to `struct
inotify_event` or retain the returned name after the next batch read.

## Ownership and errors

The fd owns all native watches; closing it releases them without deleting
filesystem objects. `remove()`, `read()`, and `close()` return 0/-1. Invalid
arguments set `EINVAL`; native limits/permissions produce native errno
(`ENOSPC`, `EACCES`, etc.). EINTR propagates without retry. Errno is unspecified
on success. Close once and relinquish ownership even on interruption. The
wrapper cannot check fd provenance. Unwatch a borrowed select/epoll/reactor fd
before closing it. No mandatory third-party library or elevated privilege.

## Example and verification

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/fswatch.c -o /tmp/opencode/fswatch-example
/tmp/opencode/fswatch-example
```

The example explicitly enables FSWATCH and SELECT, creates its own exclusive
local directory/file, waits at most 1000 ms, decodes the create event, and removes
only its own objects. Platform, permission, and watch-limit restrictions print
skips. `tests/test_os_events.c` adds unaligned/malformed batches, transactional
outputs, overflow records, real create/modify/rename/delete/cookie/ignored
events, fd flags, and reactor integration.
