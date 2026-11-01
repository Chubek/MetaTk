#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_RING
#include "mulib-ring.h"
int main(void){
  mulib_ring_t r; assert(mulib_ring_init(&r,4,sizeof(int))); int x=9,y=0; assert(mulib_ring_write(&r,&x)); assert(mulib_ring_read(&r,&y)&&y==9); mulib_ring_free(&r);
  return 0;
}
