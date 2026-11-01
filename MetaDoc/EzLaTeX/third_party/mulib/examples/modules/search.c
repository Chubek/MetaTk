#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_SEARCH
#include "mulib-search.h"
int main(void){
  assert(mulib_search_kmp("hello",5,"ll",2)==2);
  return 0;
}
