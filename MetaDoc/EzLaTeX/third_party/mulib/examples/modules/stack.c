#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_STACK
#include "mulib-stack.h"
int main(void){
  mulib_stack_t*s=mulib_stack_create(); assert(s); int x=7,y=0; assert(mulib_stack_set_stride(s,sizeof(int))); assert(mulib_stack_push(s,&x)); assert(mulib_stack_pop(s,&y)&&y==7); mulib_stack_destroy(s);
  return 0;
}
