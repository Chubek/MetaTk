#include "MetaVect.h"
#include <assert.h>
#include <string.h>
int main(void) {
    assert(strcmp(mv_version_string(), "0.1.0") == 0);
    assert(strcmp(mv_status_string(MV_STATUS_OK), "ok") == 0);
    assert(strcmp(mv_status_string((mv_status)999), "unknown status") == 0);
    return 0;
}
