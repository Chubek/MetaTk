#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_SEMVER
#include "mulib-semver.h"
int main(void){
  mulib_semver_t v; assert(mulib_semver_parse("1.2.3",&v)); assert(v.patch==3);
  return 0;
}
