#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_TEXT
#include "mulib-text.h"
int main(void){
  char str[]=" ABC "; assert(!strcmp(mulib_text_trim(str),"ABC"));
  return 0;
}
