#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_PATH
#include "mulib-path.h"
int main(void){
  assert(!strcmp(mulib_path_basename("/foo/bar"),"bar"));
  return 0;
}
