#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_ARGPARSE
#include "mulib-argparse.h"
int main(void){
  char *argv[]={"app","--mode=fast","file.txt"}; mulib_argparse_t*a=mulib_argparse_load(); assert(a); assert(mulib_argparse_parse(a,3,argv)); assert(!strcmp(mulib_argparse_get(a,"mode"),"fast")); mulib_argparse_destroy(a);
  return 0;
}
