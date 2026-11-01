#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_ROMAN
#include "mulib-roman.h"
int main(void){
  char b[32]; assert(mulib_roman_format(12,b,sizeof b)); assert(!strcmp(b,"XII"));
  return 0;
}
