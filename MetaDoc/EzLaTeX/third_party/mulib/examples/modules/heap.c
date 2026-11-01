#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_HEAP
#include "mulib-heap.h"
int main(void){
  mulib_heap_t h; mulib_heap_init(&h); assert(mulib_heap_push(&h,5)); int64_t x=0; assert(mulib_heap_pop(&h,&x)&&x==5); mulib_heap_free(&h);
  return 0;
}
