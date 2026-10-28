#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_URL
#include "mulib-url.h"
int main(void){
  char out[32]; assert(mulib_url_encode((const unsigned char*)"A B",3,out,sizeof out)==5);
  return 0;
}
