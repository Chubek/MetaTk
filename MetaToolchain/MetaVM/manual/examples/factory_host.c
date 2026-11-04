#include "MetaVM.h"
#include <inttypes.h>
#include <stdio.h>

extern mvm_status application_answer(mvm_module **, mvm_error *);

int main(void) {
    mvm_module *module = NULL;
    mvm_vm *vm = NULL;
    mvm_error error;
    mvm_value result = mvm_nil();
    mvm_status status = application_answer(&module, &error);
    int failed = 1;
    if (status != MVM_OK) goto report;
    vm = mvm_create(NULL, &error);
    if (!vm) { status = error.status; goto report; }
    status = mvm_run(vm, module, 0, NULL, 0, MVM_ENGINE_THREADED, &result, &error);
    if (status != MVM_OK) goto report;
    if (result.kind != MVM_INT || result.as.integer != 42) {
        fputs("Unexpected generated-module result\n", stderr);
        goto cleanup;
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
