#define MULIB_IMPL_DRM
#include "mulib-drm.h"
#include <assert.h>

int main(void) {
    mulib_drm_device_t device = MULIB_DRM_DEVICE_INIT;
#ifdef __linux__
    mulib_drm_dumb_t buffer = MULIB_DRM_DUMB_INIT;
    uint64_t capability = 123;
    /* /dev/null is deliberately not a DRM device: no GPU needed. */
    assert(mulib_drm_open(&device, "/dev/null") == 0);
    assert(mulib_drm_get_cap(&device, MULIB_DRM_CAP_DUMB_BUFFER, &capability) == MULIB_DRM_EIO);
    assert(capability == 123);
    assert(mulib_drm_dumb_create(&device, 64, 64, 32, &buffer) == MULIB_DRM_EIO);
    assert(buffer.fd == -1 && buffer.data == NULL);
    assert(mulib_drm_dumb_destroy(&buffer) == 0);
    assert(mulib_drm_close(&device) == 0);
#else
    assert(mulib_drm_open(&device, "unused") == MULIB_DRM_ENOTSUP);
#endif
    return 0;
}
