#include <assert.h>
#include <stdlib.h>
#define MULIB_IMPL_TASKPOOL
#define MULIB_IMPL_PARALLEL
#define MULIB_IMPL_PIPELINE
#include "mulib-pipeline.h"
static int double_value(mulib_pipeline_item_t *item, void *context) {
    (void)context; *(int *)item->data *= 2; return 0;
}
static void collect(mulib_pipeline_item_t *item, void *context) {
    *(int *)context += *(int *)item->data; free(item->data);
}
int main(void) {
    mulib_taskpool_t *pool;
    mulib_pipeline_stage_t stage = {double_value, NULL};
    mulib_pipeline_item_t items[2];
    size_t i, completed;
    int sum = 0;
    for (i = 0; i < 2; ++i) {
        items[i].data = malloc(sizeof(int)); assert(items[i].data);
        items[i].size = sizeof(int); *(int *)items[i].data = (int)i + 1;
    }
    assert(!mulib_taskpool_create(&pool, 2, 2));
    assert(!mulib_pipeline_run(pool, &stage, 1, items, 2, collect, &sum, NULL, &completed, NULL));
    assert(completed == 2 && sum == 6 && !items[0].data && !items[1].data);
    mulib_taskpool_destroy(pool, 0); return 0;
}
