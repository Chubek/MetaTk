#define MULIB_IMPL_INPUT
#define MULIB_IMPL_VDEV
#include "mulib-vdev.h"
#include <assert.h>
int main(void) {
    mulib_vdev_t device = MULIB_VDEV_INIT;
    /* Creation/injection is deliberately opt-in:
       uint16_t keys[] = {30};
       mulib_vdev_create(&device, NULL, "example", keys, 1, 0);
       mulib_vdev_emit(&device, &press); mulib_vdev_sync(&device);
       Emit the matching release and sync before destruction. */
    assert(mulib_vdev_close(&device) == MULIB_INPUT_OK);
    return 0;
}
