#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_MANPAGE
#include "mulib-manpage.h"
int main(void){
  mulib_manpage_t*m=mulib_manpage_linux(); assert(m); assert(mulib_manpage_set(m,"APP","1","app --help","A tool")); char*t=mulib_manpage_render(m); assert(t); free(t); mulib_manpage_destroy(m);
  return 0;
}
