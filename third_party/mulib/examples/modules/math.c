#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_MATH
#include "mulib-math.h"
int main(void){
  assert(mulib_math_gcd(12,20)==4);
  return 0;
}
