#define MULIB_IMPL_VEC
#define MULIB_IMPL_WLROOTS
#include "mulib-wlroots.h"
#include <assert.h>
int main(void) {
    mulib_wlroots_layout_t layout; mulib_wlroots_box_t output = {0,0,1920,1080}, rotated;
    mulib_wlroots_layout_init(&layout);
    assert(mulib_wlroots_layout_set(&layout,1,&output));
    assert(mulib_wlroots_layout_at(&layout,100,100) == 1);
    assert(mulib_wlroots_box_transform(&output,MULIB_WLROOTS_90,1920,1080,&rotated) && rotated.width == 1080);
    mulib_wlroots_layout_free(&layout); return 0;
}
