#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#include "mulib-rewrite.h"
#include <assert.h>
int main(void) {
    /* Generic opcodes: 0 = constant, 1 = addition. Rule x+0 = x. */
    mulib_rewrite_pattern_t lhs[] = {
        { MULIB_REWRITE_VARIABLE,0,0,0,0,{0,0} },
        { MULIB_REWRITE_NODE,0,0,0,0,{0,0} },
        { MULIB_REWRITE_NODE,0,1,2,0,{0,1} }
    };
    mulib_rewrite_pattern_t rhs[] = { { MULIB_REWRITE_VARIABLE,0,0,0,0,{0,0} } };
    mulib_rewrite_rule_t rule = { lhs,rhs,3,1 };
    mulib_rewrite_limits_t limits = { 1,8,100,10000 };
    mulib_rewrite_result_t result;
    mulib_egraph_t *graph = NULL;
    mulib_egraph_node_t n = { 0,0,42,{0,0} };
    mulib_egraph_tree_t tree = { 0 };
    mulib_egraph_extract_limits_t extraction = { 1000,16 };
    size_t x, zero, root;
    assert(mulib_egraph_create(16,&graph) == MULIB_EGRAPH_OK);
    assert(mulib_egraph_add(graph,n,&x) == MULIB_EGRAPH_OK);
    n.literal = 0; assert(mulib_egraph_add(graph,n,&zero) == MULIB_EGRAPH_OK);
    n.opcode = 1; n.arity = 2; n.child[0] = x; n.child[1] = zero;
    assert(mulib_egraph_add(graph,n,&root) == MULIB_EGRAPH_OK);
    assert(mulib_rewrite_run(graph,&rule,1,limits,&result) == MULIB_REWRITE_OK);
    assert(result.applications == 1 && mulib_egraph_find(graph,root) == mulib_egraph_find(graph,x));
    assert(mulib_egraph_extract(graph,root,NULL,NULL,extraction,&tree) == MULIB_EGRAPH_OK);
    assert(tree.count == 1 && tree.nodes[0].literal == 42);
    mulib_egraph_tree_free(&tree); mulib_egraph_destroy(graph);
    return 0;
}
