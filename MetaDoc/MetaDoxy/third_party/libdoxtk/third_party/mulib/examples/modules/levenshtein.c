#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_LEVENSHTEIN
#include "mulib-levenshtein.h"
int main(void){
  assert(mulib_levenshtein_distance("cat","cut")==1);
  return 0;
}
