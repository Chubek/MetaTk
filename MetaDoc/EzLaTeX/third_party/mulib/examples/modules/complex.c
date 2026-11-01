#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_COMPLEX
#include "mulib-complex.h"
int main(void){
  mulib_complex_t a={1,2},b={3,4},c=mulib_complex_add(a,b); assert(c.real==4);
  return 0;
}
