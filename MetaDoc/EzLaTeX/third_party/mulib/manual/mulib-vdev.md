# mulib-vdev

Linux uinput virtual keyboards and relative pointers, without libevdev/libinput.
No absolute axes, multitouch, force feedback, LEDs or device-discovery handshake.

## Integration and explicit creation

Define `MULIB_IMPL_INPUT`, `MULIB_IMPL_VDEV` in one C11 translation unit.
Native operations require Linux with modern UI_DEV_SETUP support; other systems
return MULIB_INPUT_ENOTSUP. Initialize with `MULIB_VDEV_INIT`, never shallow-copy.

`create(&device,path,name,keys,key_count,relative_axes)` opens `/dev/uinput` for
NULL path. The fd is nonblocking/close-on-exec. Name must occupy 1..79 bytes;
key codes are 0..767, count at most 768, and the 16-bit axes mask enables REL
codes 0..15. At least one capability is required; Linux may reject reserved or
unsupported codes. IDs are BUS_VIRTUAL, vendor/product zero, version one. Name
and key list are borrowed only during creation. Failure closes partial resources
and leaves the handle unchanged; callers need appropriate device permissions.

## Emission and cleanup

`emit(&device,&event)` accepts validated SYN/KEY/REL events; SYN/REL codes must
be <=15. The application must use only capabilities it enabled. Timestamps are
zeroed so the kernel assigns its clock. `sync` emits SYN_REPORT; normal sequences
are press, sync, release, sync. Creation returns before desktop device discovery
is guaranteed; synchronize with your environment before injecting input.

Results use MULIB_INPUT constants: OK=0, AGAIN=1, EINVAL=-1, EIO=-2,
ENOTSUP=-3, ESTATE=-4. EINTR retries, EAGAIN is AGAIN, short writes EIO. Native
errno is preserved. On AGAIN retry the same event; on success do not resend it.
Caller must coordinate partial multi-event frames and release held keys on error.

`close` closes/reset the fd and destroys the kernel device, idempotently. No
automatic release generation or global input injection occurs merely by inclusion.
`cc -std=c11 -Iinclude examples/modules/vdev.c -o vdev` intentionally creates no
device. Tests cover validation and native failure cleanup; successful injection
requires explicit hardware/environment testing and is not run by the suite.
