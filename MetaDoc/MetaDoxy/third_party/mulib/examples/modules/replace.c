#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_REPLACE
#include "mulib-replace.h"
int main(void){
  char *s=mulib_replace_all("hello","l","!"); assert(s&&!strcmp(s,"he!!o")); free(s);
  return 0;
}
