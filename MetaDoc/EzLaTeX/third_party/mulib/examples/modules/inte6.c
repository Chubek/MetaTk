#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#include "mulib-inte6.h"
#include <assert.h>
#include <string.h>
int main(void) {
    mulib_inet6_t address; mulib_inte6_t alias; char text[40];
    assert(mulib_inet6_parse("2001:DB8::1",11,&address));
    assert(mulib_inet6_format(&address,text,sizeof(text)) && !strcmp(text,"2001:db8::1"));
    assert(mulib_inte6_parse("::1", 3, &alias));
    assert(mulib_inte6_format(&alias, text, sizeof(text)) && !strcmp(text, "::1"));
    return 0;
}
