#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_DEQUE
#include "mulib-deque.h"
int main(void){
  mulib_deque_t d; mulib_deque_init(&d,sizeof(int)); int x=2,y=0; assert(mulib_deque_push_front(&d,&x)); assert(mulib_deque_pop_back(&d,&y)&&y==2); mulib_deque_free(&d);
  return 0;
}
