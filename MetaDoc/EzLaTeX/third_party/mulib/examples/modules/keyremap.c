#define MULIB_IMPL_INPUT
#define MULIB_IMPL_KEYREMAP
#include "mulib-keyremap.h"
#include <assert.h>
int main(void) {
    mulib_keyremap_t remap; mulib_input_event_t event = {0,MULIB_INPUT_KEY,30,1}, out;
    mulib_keyremap_init(&remap);
    assert(mulib_keyremap_set(&remap,30,48));
    assert(mulib_keyremap_apply(&remap,&event,&out) == 1 && out.code == 48);
    event.value = 0; assert(mulib_keyremap_apply(&remap,&event,&out) == 1 && out.code == 48);
    return 0;
}
