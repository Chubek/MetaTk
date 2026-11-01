#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_BLOOM
#include "mulib-bloom.h"
int main(void){
  mulib_bloom_t b; assert(mulib_bloom_init(&b,1024,5)); mulib_bloom_add(&b,"hello",5); assert(mulib_bloom_test(&b,"hello",5)); mulib_bloom_free(&b);
  return 0;
}
