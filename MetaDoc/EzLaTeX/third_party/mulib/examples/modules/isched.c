#define MULIB_IMPL_ISCHED
#include "mulib-isched.h"
#include <assert.h>
int main(void) {
    mulib_isched_node_t nodes[] = { { 4 }, { 1 }, { 2 } };
    mulib_isched_edge_t edges[] = { { 0, 2 }, { 1, 2 } };
    mulib_isched_limits_t limits = { 3, 2, 100 };
    mulib_isched_schedule_t schedule = { 0 };
    assert(mulib_isched_run(nodes, 3, edges, 2, limits, &schedule, NULL) == MULIB_ISCHED_OK);
    assert(schedule.order[0] == 0 && schedule.start[1] == 1);
    assert(schedule.start[2] == 4 && schedule.makespan == 6);
    mulib_isched_free(&schedule);
    edges[1].before = 2; edges[1].after = 0;
    assert(mulib_isched_run(nodes, 3, edges, 2, limits, &schedule, NULL) == MULIB_ISCHED_CYCLE);
    return 0;
}
