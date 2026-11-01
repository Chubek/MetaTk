#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_GEOMETRY
#include "mulib-geometry.h"
int main(void){
  mulib_geometry_point_t a={0,0},b={3,4}; assert(mulib_geometry_distance(a,b)==5);
  return 0;
}
