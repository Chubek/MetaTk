#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_CRC32
#include "mulib-crc32.h"
int main(void){
  assert(mulib_crc32("123456789",9)==0xcbf43926u);
  return 0;
}
