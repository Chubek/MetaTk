#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_DSU
#include "mulib-dsu.h"
int main(void){
  mulib_dsu_t d; assert(mulib_dsu_init(&d,3)); assert(mulib_dsu_union(&d,0,2)); assert(mulib_dsu_find(&d,0)==mulib_dsu_find(&d,2)); mulib_dsu_free(&d);
  return 0;
}
