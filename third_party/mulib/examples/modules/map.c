#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_MAP
#include "mulib-map.h"
int main(void){
  mulib_map_t m; mulib_map_init(&m); int x=3; assert(mulib_map_set(&m,"key",&x)); void *p=NULL; assert(mulib_map_get(&m,"key",&p)&&p==&x); mulib_map_free(&m);
  return 0;
}
