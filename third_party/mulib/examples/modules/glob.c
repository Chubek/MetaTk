#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_GLOB
#include "mulib-glob.h"
int main(void){
  assert(mulib_glob_match("file*.c","file1.c"));
  return 0;
}
