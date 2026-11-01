# mulib-procfs

A focused parser/query API for Linux `/proc/<pid>/stat` and `status`, not a
complete procfs implementation. Parsing and VFS queries are portable ISO C11.
Native access uses Linux stdio and explicitly returns `MULIB_VFS_ENOTSUP` on
other platforms. No feature-test macros, page-size assumptions, third-party
libraries or dependency implementation switches are injected.

## Integration and runnable example

Implement `MULIB_IMPL_PROCFS` and dependency `MULIB_IMPL_VFS` exactly once each;
include `mulib-procfs.h` for C/C++ declarations. The dependency supplies normalized
paths, shared error codes and fixture/backend queries.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/procfs.c -o /tmp/procfs-example
/tmp/procfs-example
```

The example creates a memory-backed `/proc/123` fixture and queries it. It needs
neither a real process with that PID nor host `/proc` availability.

## Parsing stat

`mulib_procfs_parse_stat(text, length, &out)` accepts a bounded byte span; a NUL
terminator is not required and embedded NUL is rejected. Input is at most
**16384 bytes**, inclusive. The parser requires PID, parenthesized comm, a
single ASCII-letter state, and all numeric fields through Linux field 24. It
uses the **last closing parenthesis**, allowing spaces, opening/closing
parentheses and even newlines inside comm. It does not split comm on whitespace.
The copied comm is at most **255 bytes** plus NUL; it is not Unicode-decoded.

The output contains:

| Field | Linux field | Representation |
| --- | --- | --- |
| `pid`, `ppid` | 1, 4 | `uint32_t`; PID nonzero, PPID may be zero. |
| `comm`, `state` | 2, 3 | Owned inline bytes and single character. |
| `user_ticks`, `system_ticks` | 14, 15 | Unsigned 64-bit clock ticks. |
| `start_ticks` | 22 | Unsigned 64-bit ticks since boot. |
| `virtual_bytes` | 23 | Unsigned 64-bit bytes. |
| `resident_pages` | 24 | Signed 64-bit page count, including representable negative input. |

Numeric tokens use decimal digits, optional leading `+`/`-`, and ASCII
whitespace separation. Negative values are rejected for unsigned fields 9..15,
22, 23 and PID/PPID. Signed fields 5..8 and 16..21 are checked against 64-bit
signed range even though not exposed. Other discarded fields and numeric fields
after 24 must fit an unsigned magnitude of 64 bits (negative magnitudes must fit
signed 64-bit). Later nonnumeric extensions are rejected, not silently ignored.
Missing fields, nonnumeric tokens, malformed delimiters or state are `EFORMAT`;
numeric/comm/input limits are `ELIMIT`. Neither clock ticks nor RSS pages are
converted: callers obtain the target system's tick frequency/page size elsewhere.

## Parsing status

`mulib_procfs_parse_status(text, length, &out)` accepts up to **65536 bytes** and
rejects embedded NUL. Lines may end in LF, CRLF, or at end-of-input. Blank LF
lines are ignored. Other lines require a nonempty field name and colon. Unknown
colon-separated fields are deliberately ignored for kernel forward compatibility.

Recognized fields are `Name`, `Pid`, `PPid`, `Uid`, `Gid`, `Threads` and `VmRSS`.
`Name`/`Pid`/`PPid` are required. Duplicate recognized fields fail, even if the
values agree. `present` combines `MULIB_PROCFS_HAS_*` bits so absent RSS/UID/GID/
thread counts are distinguishable from zero. Absent values are zero-initialized.
UID and GID contain exactly four unsigned 32-bit IDs (real/effective/saved/fs).
PID and Threads must be nonzero; PPID may be zero. Numeric values allow leading
plus but not minus, must fit their types, and have no unparsed suffix.

Name is copied inline, limited to 255 bytes, with surrounding ASCII whitespace
trimmed. Linux's textual escapes are preserved literally, not unescaped. `VmRSS`
requires unsigned decimal followed by whitespace and exact unit `kB`; the value
is exposed as `resident_kib` (Linux uses 1024-byte units). There is no attempt to
parse every status field or to treat absent VmRSS in kernel tasks as an error.

## Queries, ownership and errors

`mulib_procfs_query(vfs, root, pid, &out)` normalizes an absolute VFS root and
reads `<root>/<pid>/stat` and `status`. `vfs` must be non-NULL. The files are
bounded independently; an extra-byte probe distinguishes an exact limit from
truncation. Custom VFS short reads mean EOF, per the VFS contract.
`mulib_procfs_query_native(pid, &out)` performs the same query at native `/proc`
on Linux. Files must actually be accessible; lack of `/proc`, process exit,
hidepid policy and permissions are real failures. Neither function caches data.

Both files must contain the requested PID. This guards obvious mismatched
fixtures but does **not** prevent PID reuse, process exit, or field changes
between reads. This API does not promise a coherent/atomic process snapshot or
process identity handle.

All parser/query outputs are unchanged on failure; successful outputs own their
inline strings and numbers and need no destructor. Input spans, VFS and root
strings are borrowed only during the call. Queries allocate one 65536-byte
scratch buffer and free it before returning. Parsers do not allocate.

Results are `mulib_vfs_result`, not errno: `EINVAL` for invalid arguments/zero
query PID/bad root, `EFORMAT` for malformed or PID-mismatched data, `ELIMIT` for
length/range bounds, `ENOMEM` for scratch allocation, `ENOENT` for a missing
native file, `EIO` for other native open/read/close errors, `ENOTSUP` for native
queries off Linux, and propagated VFS errors. No `_POSIX_C_SOURCE` definition is
required, even if this header follows other standard headers in one large TU.

`tests/test_virtual_files.c` covers nested/right parentheses in comm, truncation,
NUL/overflow input, missing/duplicate status fields, exact optional presence,
VFS query ownership/error paths, allocation failure, and optional live Linux
`/proc/self` queries without privileges.
