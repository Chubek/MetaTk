#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_BITMAP
#include "mulib-bitmap.h"
int main(void){
  mulib_bitmap_t b; assert(mulib_bitmap_init(&b,10,10)); assert(mulib_bitmap_put(&b,3,4,1)); assert(mulib_bitmap_get(&b,3,4)); mulib_bitmap_free(&b);
  return 0;
}
