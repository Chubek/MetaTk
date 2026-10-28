#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_MICROTCL
#include "mulib-microtcl.h"
int main(void){
  mulib_microtcl_t*t=mulib_microtcl_loadall(); assert(t); assert(mulib_microtcl_eval(t,"set x 3; incr x")); assert(!strcmp(mulib_microtcl_result(t),"4")); mulib_microtcl_destroy(t);
  return 0;
}
