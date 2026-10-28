#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_HISTOGRAM
#include "mulib-histogram.h"
int main(void){
  mulib_histogram_t h; assert(mulib_histogram_init(&h,5,0,5)); assert(mulib_histogram_add(&h,2.0)); mulib_histogram_free(&h);
  return 0;
}
