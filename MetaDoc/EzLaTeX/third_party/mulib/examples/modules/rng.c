#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_RNG
#include "mulib-rng.h"
int main(void){
  mulib_rng_t r; mulib_rng_seed(&r,5); uint64_t n=mulib_rng_next(&r); assert(n!=0);
  return 0;
}
