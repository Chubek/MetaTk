#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_ADLER32
#include "mulib-adler32.h"
int main(void){
  assert(mulib_adler32("Wikipedia",9)==0x11e60398u);
  return 0;
}
