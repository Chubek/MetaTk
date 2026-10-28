#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_UUID
#include "mulib-uuid.h"
int main(void){
  mulib_uuid_t u; assert(mulib_uuid_parse("550e8400-e29b-41d4-a716-446655440000",&u)); char s[37]; mulib_uuid_format(&u,s); assert(strlen(s)==36);
  return 0;
}
