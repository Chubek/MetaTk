#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_SPLIT
#include "mulib-split.h"
int main(void){
  char text[]="a,b"; char*fields[2]; assert(mulib_split_inplace(text,',',fields,2)==2);
  return 0;
}
