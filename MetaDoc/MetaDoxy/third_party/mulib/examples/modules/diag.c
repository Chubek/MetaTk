#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_DIAG
#include "mulib-diag.h"
int main(void){
  assert(!strcmp(mulib_diag_message(MULIB_DIAG_OK),"success"));
  return 0;
}
