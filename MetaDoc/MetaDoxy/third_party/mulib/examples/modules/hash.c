#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_HASH
#include "mulib-hash.h"
int main(void){
  assert(mulib_hash_string("hello")==mulib_hash_fnv1a("hello",5));
  return 0;
}
