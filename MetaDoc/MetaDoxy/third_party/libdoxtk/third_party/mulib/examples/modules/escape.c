#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_ESCAPE
#include "mulib-escape.h"
int main(void){
  char out[32]; assert(mulib_escape_c("A\n",out,sizeof out)==3);
  return 0;
}
