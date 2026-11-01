#include <assert.h>
#define MULIB_IMPL_CRON
#include "mulib-cron.h"
static int tick(void *p) { ++*(int *)p; return 0; }
int main(void) {
    mulib_cron_t cron;
    struct tm monday = {0};
    int count = 0, matched;
    monday.tm_year = 126; monday.tm_mon = 9; monday.tm_mday = 26;
    monday.tm_wday = 1; monday.tm_hour = 9; monday.tm_min = 30;
    assert(!mulib_cron_parse(&cron, "*/15 9-17 * * 1-5", NULL));
    assert(!mulib_cron_dispatch(&cron, &monday, tick, &count, &matched));
    assert(matched && count == 1);
    monday.tm_min = 31;
    assert(!mulib_cron_match(&cron, &monday));
    assert(mulib_cron_parse(&cron, "*/0 * * * *", NULL) == MULIB_CRON_INVALID);
    return 0;
}
