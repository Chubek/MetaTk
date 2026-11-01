#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_INI
#include "mulib-ini.h"
static int mulib_example_ini_cb(void*u,const char*section,const char*key,const char*val){(void)u; return strcmp(section,"server")==0&&strcmp(key,"port")==0&&strcmp(val,"1234")==0;}
int main(void){
  assert(mulib_ini_parse("[server]\nport=1234\n",mulib_example_ini_cb,NULL));
  return 0;
}
