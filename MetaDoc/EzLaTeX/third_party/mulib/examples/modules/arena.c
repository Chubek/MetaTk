#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_ARENA
#include "mulib-arena.h"
int main(void){
  mulib_arena_t a; mulib_arena_init(&a,4096); int *x=mulib_arena_alloc(&a,sizeof(int)); assert(x); *x=9; mulib_arena_reset(&a);
  return 0;
}
