#include "MetaVM.h"
#include <inttypes.h>
#include <stdio.h>

int main(void) {
    static const char source[] =
        ".function main 0 3\nconst r0 40\nconst r1 2\n"
        "add r2 r0 r1\nret r2\n.end\n";
    mvm_module *module = NULL;
    mvm_vm *vm = NULL;
    mvm_error error;
    mvm_value result = mvm_nil();
    mvm_status status;
    int failed = 1;
    int engine;
    status = mvm_compile(source, sizeof source - 1, 0, &module, &error);
    if (status != MVM_OK) goto report;
    vm = mvm_create(NULL, &error);
    if (!vm) { status = error.status; goto report; }
    for (engine = MVM_ENGINE_BYTECODE; engine <= MVM_ENGINE_THREADED; ++engine) {
        status = mvm_run(vm, module, 0, NULL, 0, (mvm_engine)engine, &result, &error);
        if (status != MVM_OK) goto report;
        if (result.kind != MVM_INT || result.as.integer != 42) {
            fputs("Unexpected result\n", stderr);
            goto cleanup;
        }
    }
    printf("%" PRId64 "\n", result.as.integer);
    failed = 0;
    goto cleanup;
report:
    fprintf(stderr, "%s: %s\n", mvm_status_string(status), error.message);
cleanup:
    mvm_destroy(vm);
    mvm_module_destroy(module);
    return failed;
}
