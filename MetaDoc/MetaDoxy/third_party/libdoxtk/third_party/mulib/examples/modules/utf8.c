#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_UTF8
#include "mulib-utf8.h"
int main(void){
  uint32_t cp=0; assert(mulib_utf8_decode((const unsigned char*)"A",1,&cp,NULL)&&cp==65);
  return 0;
}
