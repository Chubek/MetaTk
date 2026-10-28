#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_CLAMP
#include "mulib-clamp.h"
int main(void){
  assert(mulib_clamp_i64(20,0,10)==10);
  return 0;
}
