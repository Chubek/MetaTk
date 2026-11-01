#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_ENDIAN
#include "mulib-endian.h"
int main(void){
  unsigned char b[4]; mulib_endian_store_le32(b,0x12345678u); assert(mulib_endian_load_le32(b)==0x12345678u);
  return 0;
}
