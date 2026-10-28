#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_STATS
#include "mulib-stats.h"
int main(void){
  mulib_stats_t s; mulib_stats_init(&s); mulib_stats_add(&s,2); mulib_stats_add(&s,4); assert(s.mean==3);
  return 0;
}
