#define MULIB_IMPL_INPUT
#define MULIB_IMPL_VEC
#define MULIB_IMPL_MARIONETTE
#include "mulib-marionette.h"
#include <assert.h>
static int count_event(const mulib_input_event_t *event, void *user) {
    (void)event; ++*(unsigned *)user; return 0;
}
int main(void) {
    mulib_marionette_t recording; mulib_input_event_t e = {10,MULIB_INPUT_KEY,30,1}; unsigned count = 0; size_t sent;
    mulib_marionette_init(&recording,100);
    assert(mulib_marionette_append(&recording,&e) && mulib_marionette_start(&recording,1000));
    assert(!mulib_marionette_tick(&recording,1010,0,count_event,&count,&sent) && sent == 1 && count == 1);
    mulib_marionette_free(&recording); return 0;
}
