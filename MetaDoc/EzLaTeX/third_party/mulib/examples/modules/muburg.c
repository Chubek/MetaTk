#define MULIB_IMPL_MUBURG
#include "mulib-muburg.h"
#include <assert.h>
static int emit(void *user, size_t node, size_t rule, unsigned instruction) {
    unsigned *sum = (unsigned *)user;
    (void)node; (void)rule; *sum += instruction; return 1;
}
int main(void) {
    /* Two terminals followed by ADD. Goal 1 represents a cheap immediate. */
    mulib_muburg_node_t nodes[] = { {0,0,{0,0}}, {0,0,{0,0}}, {1,2,{0,1}} };
    mulib_muburg_rule_t rules[] = {
        {0,0,0,{0,0},2,10}, {1,0,0,{0,0},0,0},
        {0,1,2,{0,0},3,20}, {0,1,2,{0,1},1,30}
    };
    mulib_muburg_limits_t limits = { 3, 4, 2, 6 };
    mulib_muburg_plan_t plan = { 0 };
    unsigned sum = 0;
    size_t emitted = 0;
    assert(mulib_muburg_select(nodes,3,rules,4,2,2,0,limits,&plan,NULL) == MULIB_MUBURG_OK);
    assert(plan.cost == 3 && plan.choices[2 * 2] == 3);
    assert(mulib_muburg_emit(&plan,3,emit,&sum,&emitted) == MULIB_MUBURG_OK);
    assert(emitted == 3 && sum == 40);
    mulib_muburg_free(&plan);
    return 0;
}
