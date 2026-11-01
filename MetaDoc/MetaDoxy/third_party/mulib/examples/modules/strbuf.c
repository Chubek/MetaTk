#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_STRBUF
#include "mulib-strbuf.h"
int main(void){
  mulib_strbuf_t s; mulib_strbuf_init(&s); assert(mulib_strbuf_append(&s,"Hello")); assert(!strcmp(s.data,"Hello")); mulib_strbuf_free(&s);
  return 0;
}
