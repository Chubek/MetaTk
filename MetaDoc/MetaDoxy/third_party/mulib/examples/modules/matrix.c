#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_MATRIX
#include "mulib-matrix.h"
int main(void){
  mulib_matrix_t m; assert(mulib_matrix_init(&m,2,2)); m.data[0]=5; assert(m.data[0]==5); mulib_matrix_free(&m);
  return 0;
}
