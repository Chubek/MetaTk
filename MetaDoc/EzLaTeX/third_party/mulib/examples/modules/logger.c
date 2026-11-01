#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_LOGGER
#include "mulib-logger.h"
static void mulib_example_sink(void*u,mulib_logger_level_t level,const char*message){(void)u;(void)level;(void)message;}
int main(void){
  mulib_logger_t l; mulib_logger_init(&l,mulib_example_sink,NULL,MULIB_LOG_INFO); assert(mulib_logger_write(&l,MULIB_LOG_INFO,"test"));
  return 0;
}
