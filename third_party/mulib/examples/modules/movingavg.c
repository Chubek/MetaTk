#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_MOVINGAVG
#include "mulib-movingavg.h"
int main(void){
  mulib_movingavg_t m; assert(mulib_movingavg_init(&m,3)); assert(mulib_movingavg_push(&m,4)==4); mulib_movingavg_free(&m);
  return 0;
}
