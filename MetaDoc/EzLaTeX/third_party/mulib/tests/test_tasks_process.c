#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/wait.h>
#define MULIB_IMPL_TASKPOOL
#define MULIB_IMPL_TASKDAG
#define MULIB_IMPL_PARALLEL
#define MULIB_IMPL_PIPELINE
#define MULIB_IMPL_PROCESS
#define MULIB_IMPL_CRON
#define MULIB_IMPL_INITSYS
#include "mulib-taskpool.h"
#include "mulib-taskdag.h"
#include "mulib-parallel.h"
#include "mulib-pipeline.h"
#include "mulib-process.h"
#include "mulib-cron.h"
#include "mulib-initsys.h"

typedef struct test_gate {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    size_t entered;
    int open;
} test_gate_t;
static void test_gate_init(test_gate_t *g) {
    assert(!pthread_mutex_init(&g->mutex, NULL));
    assert(!pthread_cond_init(&g->changed, NULL));
    g->entered = 0; g->open = 0;
}
static void test_gate_job(void *arg) {
    test_gate_t *g = (test_gate_t *)arg;
    assert(!pthread_mutex_lock(&g->mutex));
    ++g->entered;
    assert(!pthread_cond_broadcast(&g->changed));
    while (!g->open) assert(!pthread_cond_wait(&g->changed, &g->mutex));
    assert(!pthread_mutex_unlock(&g->mutex));
}
static void test_gate_entered(test_gate_t *g, size_t count) {
    assert(!pthread_mutex_lock(&g->mutex));
    while (g->entered < count) assert(!pthread_cond_wait(&g->changed, &g->mutex));
    assert(!pthread_mutex_unlock(&g->mutex));
}
static void test_gate_open(test_gate_t *g) {
    assert(!pthread_mutex_lock(&g->mutex));
    g->open = 1;
    assert(!pthread_cond_broadcast(&g->changed));
    assert(!pthread_mutex_unlock(&g->mutex));
}
static void test_gate_destroy(test_gate_t *g) {
    assert(!pthread_cond_destroy(&g->changed));
    assert(!pthread_mutex_destroy(&g->mutex));
}
static void test_increment(void *arg) { (void)atomic_fetch_add((atomic_int *)arg, 1); }
static int test_reentry_iteration(size_t i, void *arg) {
    (void)i; (void)arg; assert(!"same-pool nested iteration executed"); return 0;
}
static void test_pool_reentry(void *arg) {
    mulib_taskpool_t *p = (mulib_taskpool_t *)arg;
    mulib_taskdag_t *dag;
    assert(mulib_taskpool_wait(p) == MULIB_TASK_INVALID);
    assert(mulib_taskpool_shutdown(p, 1) == MULIB_TASK_INVALID);
    mulib_taskpool_destroy(p, 1); /* must not free the currently executing pool */
    assert(mulib_parallel_for(p, 0, 4, 1, test_reentry_iteration, NULL, NULL, NULL) == MULIB_TASK_INVALID);
    assert(!mulib_taskdag_create(&dag, 1, 0));
    assert(mulib_taskdag_run(dag, p, NULL) == MULIB_TASK_INVALID);
    mulib_taskdag_destroy(dag);
}
static void test_pool_cancel_other(void *arg) {
    assert(!mulib_taskpool_shutdown((mulib_taskpool_t *)arg, 1));
}
static void test_pool(void) {
    mulib_taskpool_t *p = NULL;
    atomic_int called;
    test_gate_t gate;
    atomic_init(&called, 0);
    assert(mulib_taskpool_create(NULL, 1, 1) == MULIB_TASK_INVALID);
    assert(mulib_taskpool_create(&p, 0, 1) == MULIB_TASK_INVALID);
    assert(mulib_taskpool_create(&p, 1, SIZE_MAX) == MULIB_TASK_INVALID);
    assert(!mulib_taskpool_create(&p, 1, 1));
    assert(!mulib_taskpool_submit(p, test_pool_reentry, p, NULL));
    assert(!mulib_taskpool_wait(p));
    test_gate_init(&gate);
    assert(!mulib_taskpool_submit(p, test_gate_job, &gate, NULL));
    test_gate_entered(&gate, 1);
    assert(!mulib_taskpool_submit(p, test_increment, &called, test_increment));
    assert(mulib_taskpool_submit(p, test_increment, &called, NULL) == MULIB_TASK_FULL);
    assert(!mulib_taskpool_shutdown(p, 1));
    assert(atomic_load(&called) == 1); /* cancelled, not executed */
    assert(mulib_taskpool_submit(p, test_increment, &called, NULL) == MULIB_TASK_CLOSED);
    test_gate_open(&gate);
    assert(!mulib_taskpool_wait(p));
    mulib_taskpool_destroy(p, 1);
    test_gate_destroy(&gate);

    assert(!mulib_taskpool_create(&p, 2, 2));
    test_gate_init(&gate);
    assert(!mulib_taskpool_submit(p, test_gate_job, &gate, NULL));
    assert(!mulib_taskpool_submit(p, test_gate_job, &gate, NULL));
    test_gate_entered(&gate, 2); /* impossible for a sequential fake pool */
    test_gate_open(&gate);
    assert(!mulib_taskpool_wait(p));
    test_gate_destroy(&gate);
    assert(!mulib_taskpool_submit(p, test_increment, &called, NULL));
    mulib_taskpool_destroy(p, 0); /* drains */
    assert(atomic_load(&called) == 2);
    assert(!mulib_taskpool_create(&p, 1, 1));
    test_gate_init(&gate);
    assert(!mulib_taskpool_submit(p, test_gate_job, &gate, NULL));
    test_gate_entered(&gate, 1);
    assert(!mulib_taskpool_submit(p, test_pool_reentry, p, test_pool_reentry));
    assert(!mulib_taskpool_shutdown(p, 1)); /* same guard on controller cleanup */
    test_gate_open(&gate);
    mulib_taskpool_destroy(p, 1);
    test_gate_destroy(&gate);
    {
        mulib_taskpool_t *other;
        assert(!mulib_taskpool_create(&p, 1, 1));
        assert(!mulib_taskpool_create(&other, 1, 1));
        test_gate_init(&gate);
        assert(!mulib_taskpool_submit(other, test_gate_job, &gate, NULL));
        test_gate_entered(&gate, 1);
        assert(!mulib_taskpool_submit(other, test_pool_reentry, p, test_pool_reentry));
        assert(!mulib_taskpool_submit(p, test_pool_cancel_other, other, NULL));
        assert(!mulib_taskpool_wait(p)); /* cleanup's frame must retain outer p */
        test_gate_open(&gate);
        mulib_taskpool_destroy(other, 1);
        mulib_taskpool_destroy(p, 1);
        test_gate_destroy(&gate);
    }
    mulib_taskpool_destroy(NULL, 1);
    assert(mulib_taskpool_wait(NULL) == MULIB_TASK_INVALID);
}

typedef struct test_dag_context { int calls, result; } test_dag_context_t;
static int test_dag_call(void *arg) {
    test_dag_context_t *c = (test_dag_context_t *)arg;
    ++c->calls; return c->result;
}
static void test_dag(mulib_taskpool_t *p) {
    mulib_taskdag_t *d;
    test_dag_context_t c[4] = {{0,0},{0,42},{0,0},{0,0}};
    size_t id[4], order[4], i;
    int state, result;
    mulib_task_cancel_t *cancel = mulib_task_cancel_create();
    assert(cancel);
    assert(!mulib_taskdag_create(&d, 4, 4));
    assert(!mulib_taskdag_run(d, p, NULL));
    for (i = 0; i < 4; ++i) assert(!mulib_taskdag_add(d, test_dag_call, &c[i], &id[i]));
    assert(mulib_taskdag_add(d, test_dag_call, &c[0], &i) == MULIB_TASK_FULL);
    assert(!mulib_taskdag_depend(d, id[0], id[1]));
    assert(!mulib_taskdag_depend(d, id[0], id[1]));
    assert(!mulib_taskdag_depend(d, id[1], id[3]));
    assert(!mulib_taskdag_order(d, order, 4));
    assert(mulib_taskdag_run(d, p, NULL) == MULIB_TASK_FAILED);
    assert(c[0].calls == 1 && c[1].calls == 1 && c[2].calls == 1 && c[3].calls == 0);
    assert(!mulib_taskdag_result(d, id[1], &state, &result));
    assert(state == MULIB_TASKDAG_FAILED && result == 42);
    assert(!mulib_taskdag_result(d, id[3], &state, &result));
    assert(state == MULIB_TASKDAG_BLOCKED && result == 0);
    c[1].result = 0;
    assert(!mulib_taskdag_run(d, p, NULL));
    assert(c[3].calls == 1);
    mulib_task_cancel_request(cancel);
    assert(mulib_taskdag_run(d, p, cancel) == MULIB_TASK_CANCELLED);
    assert(c[0].calls == 2);
    assert(!mulib_taskdag_depend(d, id[3], id[0]));
    assert(mulib_taskdag_order(d, order, 4) == MULIB_TASK_CYCLE);
    assert(mulib_taskdag_run(d, p, NULL) == MULIB_TASK_CYCLE);
    assert(c[0].calls == 2);
    assert(mulib_taskdag_result(d, 4, &state, NULL) == MULIB_TASK_INVALID);
    mulib_taskdag_destroy(d);
    mulib_task_cancel_destroy(cancel);
    assert(!mulib_taskdag_create(&d, 1, 1));
    assert(!mulib_taskdag_add(d, test_dag_call, &c[0], &id[0]));
    assert(!mulib_taskdag_depend(d, 0, 0));
    assert(mulib_taskdag_run(d, p, NULL) == MULIB_TASK_CYCLE);
    mulib_taskdag_destroy(d);
}

static int test_square(size_t i, const void *in, void *out, void *arg) {
    const int *x = (const int *)in;
    (void)i; (void)arg;
    *(int *)out = *x * *x; return 0;
}
static int test_for_fill(size_t i, void *arg) { ((size_t *)arg)[i] = i + 1; return 0; }
static int test_for_fail(size_t i, void *arg) { (void)arg; return i == 0 ? 123 : 0; }
static int test_for_cancel(size_t i, void *arg) {
    (void)i; mulib_task_cancel_request((mulib_task_cancel_t *)arg); return 0;
}
static int test_for_boundary(size_t i, void *arg) { *(size_t *)arg = i; return 0; }
static void test_parallel(mulib_taskpool_t *p) {
    int input[32], output[32], error;
    size_t filled[32] = {0}, i;
    mulib_task_cancel_t *cancel = mulib_task_cancel_create();
    assert(cancel);
    for (i = 0; i < 32; ++i) input[i] = (int)i;
    assert(!mulib_parallel_map(p, input, sizeof(int), output, sizeof(int), 32, 3, test_square, NULL, NULL, &error));
    assert(!error);
    for (i = 0; i < 32; ++i) assert(output[i] == (int)(i * i));
    assert(!mulib_parallel_map(p, input, sizeof(int), input, sizeof(int), 32, 7, test_square, NULL, NULL, NULL));
    assert(input[5] == 25);
    assert(mulib_parallel_map(p, input, sizeof(int), input + 1, sizeof(int), 2, 1, test_square, NULL, NULL, NULL) == MULIB_TASK_INVALID);
    assert(mulib_parallel_map(p, input, sizeof(int), output, sizeof(int), SIZE_MAX, 1, test_square, NULL, NULL, NULL) == MULIB_TASK_INVALID);
    assert(!mulib_parallel_map(p, NULL, 1, NULL, 1, 0, 1, test_square, NULL, NULL, NULL));
    assert(!mulib_parallel_for(p, 0, 32, 5, test_for_fill, filled, NULL, NULL));
    for (i = 0; i < 32; ++i) assert(filled[i] == i + 1);
    assert(mulib_parallel_for(p, 2, 1, 1, test_for_fill, filled, NULL, NULL) == MULIB_TASK_INVALID);
    assert(!mulib_parallel_for(p, SIZE_MAX - 1, SIZE_MAX, SIZE_MAX, test_for_boundary, &i, NULL, NULL));
    assert(i == SIZE_MAX - 1);
    assert(mulib_parallel_for(p, 0, 32, 1, test_for_fail, NULL, NULL, &error) == MULIB_TASK_FAILED);
    assert(error == 123);
    assert(mulib_parallel_for(p, 0, 32, 1, test_for_cancel, cancel, cancel, NULL) == MULIB_TASK_CANCELLED);
    assert(mulib_parallel_for(p, 0, 0, 1, test_for_fill, filled, cancel, NULL) == MULIB_TASK_CANCELLED);
    mulib_task_cancel_destroy(cancel);
}

typedef struct test_release_context { size_t count; pthread_t owner; } test_release_context_t;
static int test_pipeline_replace(mulib_pipeline_item_t *item, void *arg) {
    int *replacement = (int *)malloc(sizeof(int));
    (void)arg;
    if (!replacement) return 17;
    *replacement = *(int *)item->data + 1;
    free(item->data); item->data = replacement; item->size = sizeof(int);
    return 0;
}
static int test_pipeline_check(mulib_pipeline_item_t *item, void *arg) {
    (void)arg; assert(item->size == sizeof(int));
    assert(*(int *)item->data > 0); return 0;
}
static int test_pipeline_fail(mulib_pipeline_item_t *item, void *arg) {
    (void)item; (void)arg; return 77;
}
static void test_pipeline_release(mulib_pipeline_item_t *item, void *arg) {
    test_release_context_t *c = (test_release_context_t *)arg;
    assert(pthread_equal(c->owner, pthread_self()));
    ++c->count; free(item->data);
}
static void test_pipeline_items(mulib_pipeline_item_t *items, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        items[i].data = malloc(sizeof(int)); assert(items[i].data);
        *(int *)items[i].data = (int)i; items[i].size = sizeof(int);
    }
}
static void test_pipeline(mulib_taskpool_t *p) {
    mulib_taskpool_t *closed;
    mulib_pipeline_stage_t stages[2] = {{test_pipeline_replace,NULL},{test_pipeline_check,NULL}};
    mulib_pipeline_item_t items[16];
    test_release_context_t release = {0, 0};
    mulib_task_cancel_t *cancel = mulib_task_cancel_create();
    size_t complete, i;
    int error;
    assert(cancel); release.owner = pthread_self();
    assert(mulib_pipeline_run(p, stages, SIZE_MAX, items, 0, test_pipeline_release, &release, NULL, NULL, NULL) == MULIB_TASK_INVALID);
    assert(mulib_pipeline_run(p, stages, 2, items, SIZE_MAX, test_pipeline_release, &release, NULL, NULL, NULL) == MULIB_TASK_INVALID);
    assert(!release.count);
    test_pipeline_items(items, 16);
    assert(!mulib_pipeline_run(p, stages, 2, items, 16, test_pipeline_release, &release, NULL, &complete, &error));
    assert(complete == 16 && release.count == 16 && !error);
    for (i = 0; i < 16; ++i) assert(!items[i].data && !items[i].size);
    test_pipeline_items(items, 16);
    stages[1].fn = test_pipeline_fail;
    assert(mulib_pipeline_run(p, stages, 2, items, 16, test_pipeline_release, &release, NULL, &complete, &error) == MULIB_TASK_FAILED);
    assert(!complete && error == 77 && release.count == 32);
    test_pipeline_items(items, 16);
    mulib_task_cancel_request(cancel);
    assert(mulib_pipeline_run(p, stages, 2, items, 16, test_pipeline_release, &release, cancel, &complete, &error) == MULIB_TASK_CANCELLED);
    assert(!complete && release.count == 48);
    assert(!mulib_pipeline_run(p, NULL, 0, NULL, 0, test_pipeline_release, &release, NULL, &complete, NULL));
    items[0].data = NULL; items[0].size = 1;
    assert(mulib_pipeline_run(p, stages, 2, items, 1, test_pipeline_release, &release, NULL, NULL, NULL) == MULIB_TASK_INVALID);
    assert(release.count == 48); /* validation failure did not consume */
    assert(!mulib_taskpool_create(&closed, 1, 1));
    assert(!mulib_taskpool_shutdown(closed, 0));
    test_pipeline_items(items, 16);
    assert(mulib_pipeline_run(closed, stages, 2, items, 16, test_pipeline_release, &release, NULL, &complete, &error) == MULIB_TASK_CLOSED);
    assert(!complete && !error && release.count == 64);
    mulib_taskpool_destroy(closed, 0);
    mulib_task_cancel_destroy(cancel);
}

static size_t test_read_all(int fd, char *buffer, size_t capacity) {
    size_t used = 0;
    for (;;) {
        ssize_t n = read(fd, buffer + used, capacity - used);
        if (n < 0 && errno == EINTR) continue;
        assert(n >= 0);
        if (!n) break;
        used += (size_t)n; assert(used < capacity);
    }
    return used;
}
static void test_process(void) {
    mulib_process_t producer, consumer;
    mulib_process_status_t status;
    const char *echo[] = {"echo", "literal;$(no-shell)"};
    const char *cat[] = {"cat"}, *failure[] = {"false"}, *sleep_args[] = {"sleep", "30"};
    mulib_process_io_t io = {-1,-1,-1};
    int first[2], second[2], raw;
    char output[128];
    size_t n;
    long pid;
    pid_t checker = fork();
    assert(checker >= 0);
    if (!checker) {
        int pair[2];
        (void)close(0); (void)close(1); (void)close(2);
        if (mulib_process_pipe(pair) || pair[0] < 3 || pair[1] < 3 ||
            !(fcntl(pair[0], F_GETFD) & FD_CLOEXEC) ||
            !(fcntl(pair[1], F_GETFD) & FD_CLOEXEC)) _exit(1);
        (void)close(pair[0]); (void)close(pair[1]); _exit(0);
    }
    assert(waitpid(checker, &raw, 0) == checker && WIFEXITED(raw) && !WEXITSTATUS(raw));
    mulib_process_init(&producer); mulib_process_init(&consumer);
    assert(!mulib_process_pipe(first) && !mulib_process_pipe(second));
    assert(first[0] >= 3 && first[1] >= 3);
    assert((fcntl(first[0], F_GETFD) & FD_CLOEXEC) && (fcntl(first[1], F_GETFD) & FD_CLOEXEC));
    io.stdin_fd = first[0]; io.stdout_fd = second[1];
    assert(!mulib_process_spawn(&consumer, "/bin/cat", cat, 1, &io));
    io.stdin_fd = -1; io.stdout_fd = first[1];
    assert(!mulib_process_spawn(&producer, "/bin/echo", echo, 2, &io));
    assert(!mulib_process_close(first[0]) && !mulib_process_close(first[1]));
    assert(!mulib_process_close(second[1]));
    n = test_read_all(second[0], output, sizeof(output));
    assert(n == strlen(echo[1]) + 1 && !memcmp(output, echo[1], n - 1) && output[n - 1] == '\n');
    assert(!mulib_process_close(second[0]));
    assert(!mulib_process_wait(&producer, &status, 0) && status.exited && !status.exit_code);
    assert(!mulib_process_wait(&consumer, &status, 0) && status.exited && !status.exit_code);
    assert(mulib_process_wait(&consumer, &status, 0) == MULIB_PROCESS_INVALID);
    assert(mulib_process_spawn(&producer, "/mulib/no/such/executable", cat, 1, NULL) == MULIB_PROCESS_SYSTEM);
    assert(!producer.pid);
    assert(mulib_process_spawn(&producer, "/bin/cat", cat, 0, NULL) == MULIB_PROCESS_INVALID);
    assert(!mulib_process_spawn(&producer, "/bin/false", failure, 1, NULL));
    assert(!mulib_process_wait(&producer, &status, 0) && status.exit_code != 0);
    assert(!mulib_process_spawn(&producer, "/bin/sleep", sleep_args, 2, NULL));
    pid = producer.pid;
    assert(mulib_process_wait(&producer, &status, 1) == MULIB_PROCESS_RUNNING);
    assert(!mulib_process_terminate(&producer, &status) && !producer.pid && status.term_signal == SIGKILL);
    assert(waitpid((pid_t)pid, &raw, WNOHANG) == -1 && errno == ECHILD);
    assert(!mulib_process_spawn(&producer, "/bin/true", failure, 1, NULL));
    pid = producer.pid;
    assert(waitpid((pid_t)pid, &raw, 0) == (pid_t)pid);
    status.exit_code = 91;
    assert(mulib_process_terminate(&producer, &status) == MULIB_PROCESS_SYSTEM && errno == ECHILD);
    assert(!producer.pid && status.exit_code == 91);
}

typedef struct test_cron_context { mulib_taskpool_t *pool; atomic_int *calls; } test_cron_context_t;
static int test_cron_enqueue(void *arg) {
    test_cron_context_t *c = (test_cron_context_t *)arg;
    return mulib_taskpool_submit(c->pool, test_increment, c->calls, NULL);
}
static void test_cron(mulib_taskpool_t *p) {
    static const char *bad[] = {"", "* * * *", "* * * * * *", "60 * * * *", "* 24 * * *", "* * 0 * *", "* * * 13 *", "* * * * 8", "*/0 * * * *", "1/2 * * * *", "2-1 * * * *", "*, * * * *", "* * * * MON", "* * * * *\n", "999999999999999999 * * * *", "* * * * 1-7/9"};
    mulib_cron_t s, saved;
    struct tm t = {0};
    size_t i, offset;
    atomic_int calls;
    int matched;
    char huge[MULIB_CRON_MAX_SPEC + 2];
    test_cron_context_t c;
    atomic_init(&calls, 0); c.pool = p; c.calls = &calls;
    t.tm_year = 124; t.tm_mon = 1; t.tm_mday = 29; t.tm_wday = 4; t.tm_hour = 12; t.tm_min = 30;
    assert(!mulib_cron_parse(&s, "*/15 9-17 * 1,2,12 1-5", &offset));
    assert(mulib_cron_match(&s, &t) == 1);
    assert(!mulib_cron_dispatch(&s, &t, test_cron_enqueue, &c, &matched) && matched);
    assert(!mulib_taskpool_wait(p) && atomic_load(&calls) == 1);
    t.tm_min = 31; assert(!mulib_cron_match(&s, &t));
    assert(!mulib_cron_dispatch(&s, &t, test_cron_enqueue, &c, &matched) && !matched);
    t.tm_min = 30;
    saved = s;
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        assert(mulib_cron_parse(&s, bad[i], &offset) == MULIB_CRON_INVALID);
        assert(!memcmp(s.fields, saved.fields, sizeof(s.fields)) && s.dom_any == saved.dom_any && s.dow_any == saved.dow_any);
    }
    memset(huge, ' ', sizeof(huge)); huge[sizeof(huge) - 1] = '\0';
    assert(mulib_cron_parse(&s, huge, &offset) == MULIB_CRON_INVALID);
    assert(!mulib_cron_parse(&s, "0 0 1 * 0,7", NULL));
    t.tm_hour = t.tm_min = 0; t.tm_mday = 2; t.tm_wday = 0;
    assert(mulib_cron_match(&s, &t) == 1); /* restricted DOM OR DOW */
    t.tm_mday = 1; t.tm_wday = 1; assert(mulib_cron_match(&s, &t) == 1);
    t.tm_mday = 2; assert(!mulib_cron_match(&s, &t));
    assert(!mulib_cron_parse(&s, "* * * * *", NULL));
    t.tm_year = 123; t.tm_mday = 29;
    assert(mulib_cron_match(&s, &t) == MULIB_CRON_INVALID);
    t.tm_year = 124; assert(mulib_cron_match(&s, &t) == 1);
    t.tm_year = 0; assert(mulib_cron_match(&s, &t) == MULIB_CRON_INVALID); /* 1900 */
    t.tm_year = 100; assert(mulib_cron_match(&s, &t) == 1); /* 2000 */
    t.tm_mon = 12; assert(mulib_cron_match(&s, &t) == MULIB_CRON_INVALID);
    assert(mulib_cron_match(NULL, &t) == MULIB_CRON_INVALID);
}

typedef struct test_service {
    int *events;
    size_t *used;
    int id, fail_start, fail_stop;
} test_service_t;
static int test_service_start(void *arg) {
    test_service_t *s = (test_service_t *)arg;
    s->events[(*s->used)++] = s->id;
    return s->fail_start;
}
static int test_service_stop(void *arg) {
    test_service_t *s = (test_service_t *)arg;
    s->events[(*s->used)++] = -s->id;
    if (s->fail_stop) { s->fail_stop = 0; return 1; }
    return 0;
}
static void test_initsys(mulib_taskpool_t *p) {
    mulib_initsys_t *s;
    int events[32], raw;
    size_t used = 0, a, b, exited, attempt;
    const char *sleep_args[] = {"sleep", "30"}, *true_args[] = {"true"};
    test_service_t first = {events,&used,1,0,0}, second = {events,&used,2,0,0};
    mulib_process_status_t status;
    struct timespec delay = {0,1000000};
    mulib_task_cancel_t *cancel = mulib_task_cancel_create();
    assert(cancel);
    assert(!mulib_initsys_create(&s, 2, 2));
    assert(!mulib_initsys_add(s, test_service_start, test_service_stop, &first, &a));
    assert(!mulib_initsys_add(s, test_service_start, test_service_stop, &second, &b));
    assert(!mulib_initsys_depend(s, a, b));
    mulib_task_cancel_request(cancel);
    assert(mulib_initsys_start(s, p, cancel) == MULIB_TASK_CANCELLED && !used);
    mulib_task_cancel_destroy(cancel);
    assert(!mulib_initsys_start(s, p, NULL));
    assert(used == 2 && events[0] == 1 && events[1] == 2);
    assert(mulib_initsys_live(s, a) == 1 && mulib_initsys_live(s, b) == 1);
    assert(mulib_initsys_depend(s, b, a) == MULIB_TASK_INVALID);
    assert(!mulib_initsys_stop(s));
    assert(used == 4 && events[2] == -2 && events[3] == -1);
    second.fail_start = 5;
    assert(mulib_initsys_start(s, p, NULL) == MULIB_TASK_FAILED);
    assert(used == 7 && events[4] == 1 && events[5] == 2 && events[6] == -1);
    assert(!mulib_initsys_live(s, a));
    second.fail_start = 0; first.fail_stop = 1;
    assert(!mulib_initsys_start(s, p, NULL));
    assert(mulib_initsys_destroy(s) == MULIB_TASK_FAILED);
    assert(mulib_initsys_live(s, a) == 1);
    assert(!mulib_initsys_destroy(s)); /* retry retained object */
    used = 0;
    assert(!mulib_initsys_create(&s, 2, 2));
    assert(!mulib_initsys_add(s, test_service_start, test_service_stop, &first, &a));
    assert(!mulib_initsys_add(s, test_service_start, test_service_stop, &second, &b));
    assert(!mulib_initsys_depend(s, a, b) && !mulib_initsys_depend(s, b, a));
    assert(mulib_initsys_start(s, p, NULL) == MULIB_TASK_CYCLE && !used);
    assert(!mulib_initsys_destroy(s));
    assert(!mulib_initsys_create(&s, 2, 1));
    assert(!mulib_initsys_add(s, test_service_start, test_service_stop, &first, &a));
    assert(!mulib_initsys_add_process(s, "/bin/sleep", sleep_args, 2, NULL, &b));
    assert(!mulib_initsys_depend(s, a, b));
    assert(!mulib_initsys_start(s, p, NULL));
    assert(!mulib_initsys_check(s, NULL, NULL));
    assert(!mulib_initsys_destroy(s));
    assert(waitpid(-1, &raw, WNOHANG) == -1 && errno == ECHILD);
    assert(!mulib_initsys_create(&s, 1, 0));
    assert(!mulib_initsys_add_process(s, "/bin/true", true_args, 1, NULL, &a));
    assert(!mulib_initsys_start(s, p, NULL));
    for (attempt = 0; attempt < 1000; ++attempt) {
        int rc = mulib_initsys_check(s, &exited, &status);
        if (rc == MULIB_TASK_FAILED) break;
        assert(rc == MULIB_TASK_OK); (void)nanosleep(&delay, NULL);
    }
    assert(attempt < 1000 && exited == a && status.exited && !status.exit_code);
    assert(!mulib_initsys_destroy(s));
    assert(!mulib_initsys_create(&s, 2, 1));
    assert(!mulib_initsys_add(s, test_service_start, test_service_stop, &first, &a));
    assert(!mulib_initsys_add_process(s, "/mulib/no/such/executable", true_args, 1, NULL, &b));
    assert(!mulib_initsys_depend(s, a, b));
    assert(mulib_initsys_start(s, p, NULL) == MULIB_TASK_FAILED);
    assert(!mulib_initsys_live(s, a) && !mulib_initsys_live(s, b));
    assert(!mulib_initsys_destroy(s));
    assert(!mulib_initsys_create(&s, 1, 0));
    assert(!mulib_initsys_add_process(s, "/bin/true", true_args, 1, NULL, &a));
    assert(!mulib_initsys_start(s, p, NULL));
    assert(waitpid(-1, &raw, 0) > 0); /* competing reaper: report, but allow cleanup */
    assert(mulib_initsys_check(s, NULL, NULL) == MULIB_TASK_SYSTEM);
    assert(!mulib_initsys_live(s, a) && !mulib_initsys_destroy(s));
    assert(!mulib_initsys_create(&s, 1, 0));
    assert(!mulib_initsys_add_process(s, "/bin/true", true_args, 1, NULL, &a));
    assert(!mulib_initsys_start(s, p, NULL));
    assert(waitpid(-1, &raw, 0) > 0);
    assert(mulib_initsys_destroy(s) == MULIB_TASK_FAILED);
    assert(!mulib_initsys_live(s, a) && !mulib_initsys_destroy(s));
}

int main(void) {
    mulib_taskpool_t *pool;
    (void)alarm(20); /* deterministic upper bound even if a concurrency regression hangs */
    test_pool();
    assert(!mulib_taskpool_create(&pool, 3, 2));
    test_dag(pool); test_parallel(pool); test_pipeline(pool);
    test_process(); test_cron(pool); test_initsys(pool);
    mulib_taskpool_destroy(pool, 0);
    (void)alarm(0);
    return 0;
}
