#include "MetaVM.h"
#include <inttypes.h>
#include <stdio.h>

static mvm_status increment(mvm_vm *vm, const mvm_value *arguments,
    size_t count, mvm_value *result, void *user, mvm_error *error) {
    int64_t answer;
    mvm_status status;
    (void)vm;
    if (count != 1 || !arguments || arguments[0].kind != MVM_INT || !result || !user) {
        mvm_error_clear(error);
        if (error) {
            error->status = MVM_TYPE;
            snprintf(error->message, sizeof error->message, "increment requires an integer and valid host context");
        }
        return MVM_TYPE;
    }
    status = mvm_integer_eval(MVM_OP_ADD, arguments[0].as.integer,
                              *(const int64_t *)user, &answer);
    if (status != MVM_OK) return status;
    *result = mvm_int(answer);
    return MVM_OK;
}

int main(void) {
    static const char source[] =
        ".import increment 1\n.function main 0 2\nconst r0 41\n"
        "host r1 increment r0\nret r1\n.end\n";
    int64_t delta = 1; // Borrowed callback context; kept alive through VM use.
    mvm_module *module = NULL;
    mvm_vm *vm = NULL;
    mvm_error error;
    mvm_value result = mvm_nil();
    mvm_status status = mvm_compile(source, sizeof source - 1, 0, &module, &error);
    int failed = 1;
    if (status != MVM_OK) goto report;
    vm = mvm_create(NULL, &error);
    if (!vm) { status = error.status; goto report; }
    status = mvm_register_native(vm, "increment", 1, increment, &delta, &error);
    if (status != MVM_OK) goto report;
    status = mvm_run(vm, module, 0, NULL, 0, MVM_ENGINE_BYTECODE, &result, &error);
    if (status != MVM_OK) goto report;
    if (result.kind != MVM_INT || result.as.integer != 42) {
        fputs("Unexpected host-call result\n", stderr);
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
