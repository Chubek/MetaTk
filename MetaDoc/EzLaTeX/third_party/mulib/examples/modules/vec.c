#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_VEC
#include "mulib-vec.h"
int main(void){
  mulib_vec_t v; mulib_vec_init(&v,sizeof(int)); int x=42; assert(mulib_vec_push(&v,&x)); assert(*(int*)mulib_vec_at(&v,0)==42); mulib_vec_free(&v);
  return 0;
}
