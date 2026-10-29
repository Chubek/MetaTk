#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_POPCOUNT
#include "mulib-popcount.h"
int main(void){
  assert(mulib_popcount_u64(15)==4);
  return 0;
}
