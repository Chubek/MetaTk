#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_INTERPOLATE
#include "mulib-interpolate.h"
int main(void){
  assert(mulib_interpolate_linear(0,10,0.5)==5);
  return 0;
}
