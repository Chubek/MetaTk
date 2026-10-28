#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_HEX
#include "mulib-hex.h"
int main(void){
  char s[16]; assert(mulib_hex_encode("Hi",2,s,sizeof s)==4); assert(!strcmp(s,"4869"));
  return 0;
}
