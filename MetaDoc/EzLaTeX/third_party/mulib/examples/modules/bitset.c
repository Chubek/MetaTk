#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_BITSET
#include "mulib-bitset.h"
int main(void){
  mulib_bitset_t b; assert(mulib_bitset_init(&b,100)); assert(mulib_bitset_set(&b,42,1)); assert(mulib_bitset_test(&b,42)); mulib_bitset_free(&b);
  return 0;
}
