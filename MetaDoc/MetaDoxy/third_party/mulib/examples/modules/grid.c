#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_GRID
#include "mulib-grid.h"
int main(void){
  mulib_grid_t g; assert(mulib_grid_init(&g,3,3,sizeof(int))); int *p=mulib_grid_at(&g,1,1); assert(p); *p=2; mulib_grid_free(&g);
  return 0;
}
