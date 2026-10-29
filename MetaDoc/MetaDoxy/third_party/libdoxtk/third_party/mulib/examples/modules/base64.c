#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_BASE64
#include "mulib-base64.h"
int main(void){
  char s[16]; assert(mulib_base64_encode("Hi",2,s,sizeof s)==4); assert(!strcmp(s,"SGk="));
  return 0;
}
