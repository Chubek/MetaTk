# mulib-input

Shared copyable input events plus nonblocking Linux evdev access, not libinput's
device discovery, acceleration, gestures or seat handling.

## Integration and event model

Define `MULIB_IMPL_INPUT` in one C11 translation unit. Pure event validation is
portable; device operations return `MULIB_INPUT_ENOTSUP` outside Linux.
`mulib_input_event_t` carries `time_us`, `type`, `code`, `value`. Constants match
evdev for SYN/KEY/REL/ABS. Validation accepts types 0..31; KEY additionally
requires code <768 and value 0 (release), 1 (press), or 2 (repeat). Other event
types retain native code/value semantics; validation is not a capability check.

## Native lifetime and errors

Initialize `mulib_input_t device = MULIB_INPUT_INIT`; never copy an owning handle.
`open(&device,path)` opens an explicit path read-only, nonblocking, close-on-exec.
No path probing, privilege escalation or automatic grabbing. Failures leave it
unchanged. `close` resets and releases any exclusive grab, idempotently.
`grab(&device,enable)` accepts only 0/1. Native failures return EIO with errno.

`read(&device,&event)` retries EINTR, returns OK=0 for a full native record,
AGAIN=1 for EAGAIN/EWOULDBLOCK, EIO=-2 for native failure/EOF/short record. Output
is unchanged on errors; a short record is consumed, not buffered for retry.
Timestamps are converted with overflow checks, preserving the kernel's source
clock domain (no promise of wall-clock or monotonic time). Callers must handle
SYN_DROPPED by resynchronizing key/absolute state through native ioctls.

Other results: EINVAL=-1, ENOTSUP=-3, ESTATE=-4 (already open). Handles require
external synchronization. Keep devices open only while actively consuming events.

## Example

`cc -std=c11 -Iinclude examples/modules/input.c -o input` runs without hardware.
`tests/test_devices.c` exercises native-record translation using a pipe and
nonblocking/error paths without reading the user's keyboard.
