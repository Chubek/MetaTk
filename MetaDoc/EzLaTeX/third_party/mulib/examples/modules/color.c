#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_COLOR
#include "mulib-color.h"
int main(void){
  mulib_color_hsv_t h=mulib_color_to_hsv((mulib_color_rgb_t){1,0,0}); assert(h.h==0);
  return 0;
}
