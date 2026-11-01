# mulib-udev

Linux **kernel uevents**, parsed into borrowed views, with optional netlink
monitoring. Not a libudev replacement: no device database, rule processing,
enumeration, sysfs traversal, userspace group-2 packets or libudev wire headers.

## Integration and packet grammar

Define `MULIB_IMPL_PARSEC`, `MULIB_IMPL_UDEV` in one C11 translation unit. Packet
decoding is portable; native monitoring requires Linux. The parser uses MuLIB
combinators for `[a-z]{1,32} '@' '/' path`: in actual bytes,
`action@/path\0KEY=value\0...`. Path bytes after the initial slash are
33..255; property keys are nonempty `[A-Z0-9_]+`, values any non-NUL bytes.

`parse(bytes,length,&event)` requires terminated fields and accepts trailing NUL
padding only. Maximum 65,536 packet bytes and 64 unique properties. Duplicate
keys, malformed headers and ACTION/DEVPATH properties contradicting the header
are invalid. Empty values are valid. Return OK=1, INVALID=-1, LIMIT=-2; outputs
unchanged on failure. Parser resource failure is also reported as INVALID.

The event borrows the **entire caller packet**. `action` has explicit length
and is not terminated; `path` and values are terminated. Property keys are spans
ending before '='. `get(&event,key)` returns a borrowed value or NULL. No heap
ownership or free operation for event views; keep the packet alive and unmodified.

## Monitoring

Initialize monitor with `MULIB_UDEV_MONITOR_INIT`; never copy an owning monitor.
`monitor_open` subscribes to NETLINK_KOBJECT_UEVENT kernel multicast group 1,
nonblocking and close-on-exec. `monitor_read` fills caller storage, validates
kernel sender pid zero, rejects truncated packets and decodes a borrowed event.
AGAIN=0 indicates no data; EIO=-3 retains errno, ENOTSUP=-4, ESTATE=-5. Reported
length is zero unless OK; event remains unchanged on failure. An undersized
buffer consumes/drops the datagram and returns LIMIT. `monitor_close` is idempotent.

Netlink is lossy: applications must reconcile state after ENOBUFS and cannot
treat this stream as a complete device database. Authentication here checks the
kernel netlink sender address; it is not a cryptographic event signature.

`cc -std=c11 -Iinclude examples/modules/udev.c -o udev`. Tests decode synthetic
packets, duplicates, contradictions, truncated fields and limits, without devices.
