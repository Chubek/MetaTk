#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_FRACTION
#include "mulib-fraction.h"
int main(void){
  mulib_fraction_t f; assert(mulib_fraction_make(2,4,&f)); assert(f.num==1&&f.den==2);
  return 0;
}
