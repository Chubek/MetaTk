#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_QUEUE
#include "mulib-queue.h"
int main(void){
  mulib_queue_t q; mulib_queue_init(&q,sizeof(int)); int x=4,y=0; assert(mulib_queue_push(&q,&x)); assert(mulib_queue_pop(&q,&y)&&y==4); mulib_queue_free(&q);
  return 0;
}
