#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_LIST
#include "mulib-list.h"
int main(void){
  mulib_list_node_t root,node; mulib_list_init(&root); mulib_list_append(&root,&node); assert(!mulib_list_empty(&root)); mulib_list_remove(&node);
  return 0;
}
