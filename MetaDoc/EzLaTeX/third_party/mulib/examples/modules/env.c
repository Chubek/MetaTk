#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_ENV
#include "mulib-env.h"
int main(void){
  char out[32]; assert(mulib_env_copy("MULIB_NONEXISTENT_ENV_NAME","fallback",out,sizeof out));
  return 0;
}
