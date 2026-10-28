#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_SET
#include "mulib-set.h"
int main(void){
  mulib_set_t s; mulib_set_init(&s); assert(mulib_set_add(&s,20)&&mulib_set_contains(&s,20)); mulib_set_free(&s);
  return 0;
}
