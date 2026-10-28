#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_DATE
#include "mulib-date.h"
int main(void){
  assert(mulib_date_is_leap(2024)&&mulib_date_valid(2024,2,29));
  return 0;
}
