#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_BUFFER
#include "mulib-buffer.h"
int main(void){
  mulib_buffer_t b; mulib_buffer_init(&b); assert(mulib_buffer_append(&b,"ABC",3)); assert(b.length==3); mulib_buffer_free(&b);
  return 0;
}
