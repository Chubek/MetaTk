#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_GRAPH
#include "mulib-graph.h"
int main(void){
  mulib_graph_t g; assert(mulib_graph_init(&g,2)); assert(mulib_graph_edge(&g,0,1,1)); size_t visited[2]; assert(mulib_graph_bfs(&g,0,visited,2)==2); mulib_graph_free(&g);
  return 0;
}
