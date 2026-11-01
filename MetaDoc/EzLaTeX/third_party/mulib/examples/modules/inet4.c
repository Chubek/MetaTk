#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#include "mulib-inet4.h"
#include <assert.h>
#include <string.h>
int main(void) {
    mulib_inet4_t address, network; char text[16];
    assert(mulib_inet4_parse("192.0.2.42",10,&address));
    assert(mulib_inet4_network(&address,24,&network));
    assert(mulib_inet4_format(&network,text,sizeof(text)) && !strcmp(text,"192.0.2.0"));
    return 0;
}
