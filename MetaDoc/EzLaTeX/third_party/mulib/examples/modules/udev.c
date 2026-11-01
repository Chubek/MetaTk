#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_UDEV
#include "mulib-udev.h"
#include <assert.h>
#include <string.h>
int main(void) {
    const char packet[] = "add@/devices/example\0ACTION=add\0SUBSYSTEM=input\0";
    mulib_udev_event_t event;
    assert(mulib_udev_parse(packet,sizeof(packet),&event) == MULIB_UDEV_OK);
    assert(!strcmp(mulib_udev_get(&event,"SUBSYSTEM"),"input"));
    return 0;
}
