#include "qscript/QScript.h"
#include <string.h>
int qscript_c_smoke(void) {
    const char *source="return 6 * 7;";
    int64_t value=0;
    qs_context *c=qs_create();
    int ok=c&&qs_eval(c,source,strlen(source),&value)==QS_OK&&value==42;
    qs_destroy(c);
    return ok;
}
