#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_VARINT
#include "mulib-varint.h"
int main(void){
  unsigned char b[10]; size_t n=mulib_varint_encode(300,b); uint64_t value=0; assert(mulib_varint_decode(b,n,&value,NULL)&&value==300);
  return 0;
}
