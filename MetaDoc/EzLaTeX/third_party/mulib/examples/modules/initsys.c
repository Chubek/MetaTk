#include <assert.h>
#define MULIB_IMPL_TASKPOOL
#define MULIB_IMPL_TASKDAG
#define MULIB_IMPL_PROCESS
#define MULIB_IMPL_INITSYS
#include "mulib-initsys.h"
static int start_resource(void *p) { assert(!*(int *)p); *(int *)p = 1; return 0; }
static int stop_resource(void *p) { assert(*(int *)p); *(int *)p = 0; return 0; }
int main(void) {
    mulib_taskpool_t *pool;
    mulib_initsys_t *supervisor;
    int resource = 0;
    size_t base, child;
    const char *argv[] = {"sleep", "30"};
    assert(!mulib_taskpool_create(&pool, 2, 2));
    assert(!mulib_initsys_create(&supervisor, 2, 1));
    assert(!mulib_initsys_add(supervisor, start_resource, stop_resource, &resource, &base));
    assert(!mulib_initsys_add_process(supervisor, "/bin/sleep", argv, 2, NULL, &child));
    assert(!mulib_initsys_depend(supervisor, base, child));
    assert(!mulib_initsys_start(supervisor, pool, NULL));
    assert(resource && mulib_initsys_live(supervisor, child) == 1);
    assert(!mulib_initsys_destroy(supervisor)); /* kills/reaps sleep, then stops resource */
    assert(!resource);
    mulib_taskpool_destroy(pool, 0); return 0;
}
