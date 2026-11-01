#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_PRIME
#include "mulib-prime.h"
int main(void){
  assert(mulib_prime_is_prime(97));
  return 0;
}
