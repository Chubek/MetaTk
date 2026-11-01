#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_WAYLAND
#include "mulib-wayland.h"
#include <assert.h>
#include <stdlib.h>
int main(void) {
    mulib_wayland_frame_t frame = {1,0,NULL,0}, parsed; unsigned char *wire; size_t length, used;
    assert(mulib_wayland_frame_encode(&frame,&wire,&length) == MULIB_WAYLAND_OK);
    assert(mulib_wayland_frame_parse(wire,length,&parsed,&used) == MULIB_WAYLAND_OK && used == length && parsed.object == 1);
    free(wire); return 0;
}
