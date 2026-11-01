# mulib-sysfs

Bounded text attributes and typed decimal values over a VFS fixture/backend or
native Linux files. This is not a sysfs enumerator, device manager, permission
layer, filesystem sandbox or complete Linux sysfs API. No third-party libraries
or late feature-test macros are needed.

## Integration and example

Define `MULIB_IMPL_SYSFS` and dependency `MULIB_IMPL_VFS` exactly once each in C
implementation translation units. Include `mulib-sysfs.h` normally in other C/C++
translation units; no dependency switch is enabled by the header.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/sysfs.c -o /tmp/sysfs-example
/tmp/sysfs-example
```

The example reads a typed speed and writes a boolean to a memory-backed device
fixture; it performs no writes to host `/sys` and needs no privileges.

## Context and safe relative-path policy

`mulib_sysfs_init(&sys, vfs, root, max_bytes)` copies a normalized absolute root,
borrows the VFS pointer and sets a hard positive attribute-byte bound less than
`SIZE_MAX`. A NULL VFS selects **native Linux** stdio; initialization returns
`ENOTSUP` elsewhere. The context owns no allocation and needs no destructor.
VFS contexts/backends must outlive its use.

`mulib_sysfs_validate(relative)` enforces a nonempty slash-separated relative
path of at most 4095 bytes. It rejects absolute paths, repeated/leading/trailing
slashes, `.` and `..` components, backslashes, controls 0x01..0x1f and 0x7f.
Names such as `.hidden` and `name..suffix` are valid. No percent decoding, native
separator rewriting or Unicode normalization is performed. Joining root and
relative must still fit the **4095-byte combined path** limit. Rejections occur
before I/O, and lexical traversal cannot escape the configured root.

**Native symlinks are followed**, as normal sysfs trees commonly contain them.
This is a lexical policy, not symlink-safe filesystem containment. Root and its
tree (including symlink targets) must be trusted. Do not use this API to confine
untrusted paths in a tree an attacker can modify. It has no `openat`/nofollow,
TOCTOU or mount-namespace guarantee. VFS normalization/routing also remains in
force: mounted descendants may intentionally route to another backend.

## Bounded text I/O

`mulib_sysfs_read(sys, relative, output, capacity, &length)` reads complete text
including any newline. The buffer belongs to the caller; capacity includes a
convenience NUL padding byte. At most `min(max_bytes, capacity - 1)` bytes are
read, followed by a one-byte probe when needed. Additional data gives `ELIMIT`,
never silent truncation. An exact-boundary file is accepted. Embedded NUL gives
`EFORMAT`. Empty input is valid, including with capacity 1.

On any failure length is zero and output[0] is NUL when output/capacity are
provided; other bytes may have changed. NULL output, zero capacity or NULL length
is `EINVAL`. No read allocation occurs. Native reads use streaming `fread` plus
an extra-byte probe, never `fseek` or reported file size (sysfs files frequently
report a misleading size).

`mulib_sysfs_write(sys, relative, data, length)` checks the same path and bound,
rejects embedded NUL and writes exactly the supplied text at offset zero. NULL
data is allowed only for zero length. It adds no newline and does not create or
truncate files. VFS writes first stat the attribute to require an existing file;
native Linux opens with `r+b` (requires read **and** write permission), writes,
flushes and closes. This is one stdio write operation; stdio may use multiple
native writes. Some write-only kernel attributes therefore cannot be used
through this native adapter. Short writes are `EIO`, not retried: kernel store
callbacks may have side effects, and retrying could repeat an action.

On an ordinary file or VFS memory file, writing a shorter value **retains the
old suffix**. This API models offset-zero attribute store writes, not whole-file
replacement. Use `mulib_vfs_mem_put` for complete fixture replacement. Real
kernel attributes may reject bytes, trigger hardware actions or impose their own
format/permissions. Errors do not imply rollback; do not write arbitrary sysfs
attributes merely to test this module.

## Typed values

`read_i64`, `read_u64`, `read_bool` use a 128-byte stack buffer: typed input is
limited to **127 bytes**, including whitespace, and also to `max_bytes`. Values
allow surrounding ASCII space/tab/LF/CR/vertical-tab/form-feed. Internal whitespace,
suffixes, hexadecimal/octal syntax, units, empty input, and embedded NUL fail.
Signed decimal permits one leading `+` or `-`; unsigned decimal has **no sign**.
Range checks include exact `INT64_MIN`, `INT64_MAX` and `UINT64_MAX`. Overflow
returns `ELIMIT`. Boolean reads accept numeric 0 or 1 only (leading zeros are
allowed); another representable number is `EFORMAT`.

`write_i64`/`write_u64` emit canonical decimal plus LF using a bounded 32-byte
stack buffer. `write_bool` accepts only integer 0 or 1 and writes `0\n` or `1\n`.
The attribute bound includes the emitted LF. Typed output pointers are required
and their values change only on success. Contexts and buffers are never retained.
All operations are synchronous and caller-synchronized.

## Errors and tests

Results share `mulib_vfs_result` (not errno): `EINVAL` for bad arguments/paths,
`ELIMIT` for path/content/capacity/numeric bounds, `EFORMAT` for bad text, `ENOENT`
for missing files, `EISDIR` for VFS directory writes, and propagated VFS errors.
Native missing files map to `ENOENT`; all other native open/read/write/flush/close
failures map to `EIO`. Native access off Linux is `ENOTSUP`. Lexical/typed helpers
allocate nothing; the chosen VFS backend may return `ENOMEM`.

`tests/test_virtual_files.c` covers traversal variants, missing attributes, exact
buffer/content boundaries, empty/NUL/malformed values, integer extrema/overflow,
typed writes, unchanged failed outputs, and optional read-only native Linux
`/proc/sys/kernel/pid_max` access. No host attribute write is part of the tests.
