#define MULIB_IMPL_EGRAPH
#include "mulib-egraph.h"
#include <assert.h>
int main(void) {
    mulib_egraph_t *graph = NULL;
    mulib_egraph_node_t a = { 0,0,6,{0,0} }, b = { 0,0,7,{0,0} }, f = { 1,1,0,{0,0} };
    mulib_egraph_tree_t tree = { 0 };
    mulib_egraph_extract_limits_t limits = { 1000, 16 };
    size_t x, y, fx, fy;
    assert(mulib_egraph_create(16,&graph) == MULIB_EGRAPH_OK);
    assert(mulib_egraph_add(graph,a,&x) == MULIB_EGRAPH_OK);
    assert(mulib_egraph_add(graph,b,&y) == MULIB_EGRAPH_OK);
    f.child[0] = x; assert(mulib_egraph_add(graph,f,&fx) == MULIB_EGRAPH_OK);
    f.child[0] = y; assert(mulib_egraph_add(graph,f,&fy) == MULIB_EGRAPH_OK);
    assert(mulib_egraph_union(graph,x,y,NULL) == MULIB_EGRAPH_OK);
    assert(mulib_egraph_rebuild(graph,100,NULL) == MULIB_EGRAPH_OK);
    assert(mulib_egraph_find(graph,fx) == mulib_egraph_find(graph,fy));
    assert(mulib_egraph_extract(graph,fx,NULL,NULL,limits,&tree) == MULIB_EGRAPH_OK);
    assert(tree.cost == 2 && tree.count == 2 && tree.nodes[1].child[0] == 0);
    mulib_egraph_tree_free(&tree); mulib_egraph_destroy(graph);
    return 0;
}
