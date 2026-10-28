#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_TIMER
#include "mulib-timer.h"
int main(void){
  mulib_timer_t t; assert(mulib_timer_start(&t)); assert(isfinite(mulib_timer_elapsed(&t)));
  return 0;
}
