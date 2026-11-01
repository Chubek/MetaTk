#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_SEXPR
#include "mulib-sexpr.h"
int main(void){
  mulib_sexpr_t*s=mulib_sexpr_parse("(a (b c))",NULL); assert(s); char*t=mulib_sexpr_render(s); assert(t); free(t); mulib_sexpr_destroy(s);
  return 0;
}
