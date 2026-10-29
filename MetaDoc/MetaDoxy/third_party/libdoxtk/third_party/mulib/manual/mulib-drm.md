# MuLIB drm: Linux Direct Rendering Manager helpers

**Header:** `include/mulib-drm.h`  
**Implementation switch:** `MULIB_IMPL_DRM`  
**Dependencies/link flags:** Linux native ioctl/mmap APIs only; **no libdrm**, GPU
runtime, third-party headers, or extra link flags.

## Scope

DRM here means **Direct Rendering Manager**, not digital-rights-management.
This focused API opens a Linux device, retrieves its native driver version and
uint64 capabilities, and creates/maps/unmaps/destroys simple dumb buffers. It does
not perform modesetting, choose connectors/CRTCs, create framebuffers, request
master/authentication, submit GPU commands, handle PRIME exports, or implement
rendering synchronization. Dumb-buffer support and device permissions depend on
the driver and node. A render node need not support dumb buffers.

The module describes the small native DRM UAPI structures itself using fixed
width integers plus the version ioctl's explicitly native pointers/`size_t`.
Request codes use Linux `_IOWR`, not hard-coded architecture-independent numbers.
No system headers or OS calls occur in the declaration region; no global feature
macros are introduced. Other platforms return `ENOTSUP` after ordinary argument
validation; `mulib_drm_ioctl_request` returns zero there. No fallback pretends to
provide a usable DRM device.

## Device/version/capability API

```c
mulib_drm_device_t device = MULIB_DRM_DEVICE_INIT;
int mulib_drm_open(mulib_drm_device_t *, const char *path);
int mulib_drm_close(mulib_drm_device_t *);
int mulib_drm_get_version(const mulib_drm_device_t *, mulib_drm_version_t *);
int mulib_drm_get_cap(const mulib_drm_device_t *, uint64_t capability, uint64_t *value);
```

Open takes an **explicit** path such as `/dev/dri/card0`, uses read/write access,
and marks the descriptor close-on-exec. It does not probe or require any GPU
unless the caller asks it to open one. Do not copy live device objects or edit
their fields. Close is idempotent and resets `fd` to -1; a close error may be
reported after resetting because retrying `close` risks closing a reused fd.
Close-on-exec setup uses `fcntl` and is not atomic with concurrent fork/exec.

The version output includes major/minor/patchlevel, terminated name/date/description
arrays of 128/64/256 bytes, and the full lengths returned by the kernel. Strings
may be truncated to capacity minus one; returned lengths can therefore exceed
stored string lengths. Capability queries accept any native uint64 capability;
the header names DUMB_BUFFER (1), DUMB_PREFERRED_DEPTH (3), DUMB_PREFER_SHADOW (4),
and PRIME (5). Outputs are unchanged on failure.

## Dumb-buffer lifecycle

```c
mulib_drm_dumb_t buffer = MULIB_DRM_DUMB_INIT;
int mulib_drm_dumb_create(const mulib_drm_device_t *, uint32_t width,
                          uint32_t height, uint32_t bpp, mulib_drm_dumb_t *);
int mulib_drm_dumb_map(mulib_drm_dumb_t *);
int mulib_drm_dumb_unmap(mulib_drm_dumb_t *);
int mulib_drm_dumb_destroy(mulib_drm_dumb_t *);
```

Create requires nonzero dimensions and 1..64 bits/pixel. It checks representable
minimum pitch, uses zero flags, and checks the returned pitch/size against the
requested dimensions and the process's address space. The output owns both its
kernel handle and a close-on-exec duplicate of the device fd, so it can safely
outlive `mulib_drm_close(&device)`. Never copy live buffers. A duplicate shares
the original DRM open-file description; the module only destroys its own handle.

On success `width`, `height`, `bpp`, `pitch`, and uint64 `size` describe storage.
Pitch may exceed width times bytes-per-pixel, and size may include padding.
Map first obtains the driver's MAP_DUMB offset, checks native `off_t` conversion,
and maps all `size` bytes shared read/write. It exposes `data` and `mapped_size`;
`mapped_size != 0` indicates a mapping. Repeating map is a no-op. No framebuffer
pixel format, GPU visibility, cache-coherency policy, or scanout is established.
Respect returned pitch and size; never assume a packed row layout.

Unmap preserves the mapping on failure and resets it on success. Destroy unmaps,
destroys the kernel handle, closes the duplicate fd, and resets the buffer.
Unmap/ioctl failure retains remaining resources for retry rather than silently
losing ownership. A close error is reported after handle destruction/reset.
Destroying an empty initialized buffer is a no-op on Linux. Borrowed mapped
pointers become invalid after unmap/destroy. No implicit locking is provided.

## Native ABI and errors

`mulib_drm_version_request_t`, `mulib_drm_cap_request_t`,
`mulib_drm_create_request_t`, `mulib_drm_map_request_t`, and
`mulib_drm_destroy_request_t` expose exactly the request layouts used internally.
`mulib_drm_ioctl_request(operation)` returns their native request number for
VERSION, GET_CAP, CREATE_DUMB, MAP_DUMB, or DESTROY_DUMB, and zero for an invalid
operation. These are low-level ABI descriptions, not ownership-safe substitutes
for the lifecycle functions.

Zero means success; negative results are `EINVAL`, `EIO` (native failure),
`ENOTSUP`, `EOVERFLOW`, or `ESTATE` (already live). Native ioctl calls retry EINTR.
For EIO from an OS failure, `errno` is preserved across cleanup; unsupported
ioctls often produce ENOTTY, not ENOTSUP. A driver supporting DRM but not a given
operation can return EINVAL or another native error inside the EIO result.

## Runnable example without a GPU

```c
#define MULIB_IMPL_DRM
#include "mulib-drm.h"
#include <assert.h>
int main(void) {
    mulib_drm_device_t device = MULIB_DRM_DEVICE_INIT;
    uint64_t value = 123;
    assert(mulib_drm_open(&device, "/dev/null") == 0);
    assert(mulib_drm_get_cap(&device, MULIB_DRM_CAP_DUMB_BUFFER, &value) == MULIB_DRM_EIO);
    assert(value == 123);
    assert(mulib_drm_close(&device) == 0);
    return 0;
}
```

This inline example targets Linux. The full `examples/modules/drm.c` also checks
unsupported platforms and failed creation. Build with
`cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/drm.c -o drm-example`.
`tests/test_storage.c` verifies native request sizes/offsets/numbers, unchanged
outputs and fd cleanup on `/dev/null` ioctl failures, retryable destroy state,
and real non-GPU mapping cleanup. It deliberately does not require or claim
successful operation against a physical DRM/GPU device.
