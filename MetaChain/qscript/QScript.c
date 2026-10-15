#include "QScript.h"

const char *qs_version(void) { return "1.0.0"; }
qs_status qs_eval(qs_context *context,const char *source,size_t length,int64_t *result) {
    qs_program *program;
    qs_status status;
    if (result) *result=0;
    if (!context) return QS_INVALID_ARGUMENT;
    program=qs_compile(context,source,length);
    if (!program) return QS_ERROR;
    status=qs_run(context,program,result);
    qs_program_destroy(program);
    return status;
}
