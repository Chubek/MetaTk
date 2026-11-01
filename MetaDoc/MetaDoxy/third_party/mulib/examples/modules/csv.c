#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_CSV
#include "mulib-csv.h"
static int mulib_example_csv_cb(void*u,const char*const*f,size_t n){(void)u;return n==2&&!strcmp(f[0],"A")&&!strcmp(f[1],"B");}
int main(void){
  assert(mulib_csv_parse("A,B\n",mulib_example_csv_cb,NULL));
  return 0;
}
