#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_BOUNDS
#include "mulib-bounds.h"
int main(void){
  mulib_bounds_t a={0,0,2,2}; assert(mulib_bounds_contains(a,1,1));
  return 0;
}
