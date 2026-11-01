#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_RLE
#include "mulib-rle.h"
int main(void){
  unsigned char out[32]; unsigned char in[]={1,1,2,2}; assert(mulib_rle_encode(in,4,out,sizeof out)==4);
  return 0;
}
