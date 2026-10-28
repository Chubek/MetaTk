#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_FS
#include "mulib-fs.h"
int main(void){
  assert(mulib_fs_write("mulib-example-tmp.txt","ok",2)); size_t n=0; unsigned char*p=mulib_fs_read("mulib-example-tmp.txt",&n); assert(p&&n==2); free(p); remove("mulib-example-tmp.txt");
  return 0;
}
