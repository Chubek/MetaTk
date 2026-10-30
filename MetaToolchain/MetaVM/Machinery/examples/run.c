#include "MetaVM.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    mvm_module *module = NULL;
    mvm_vm *vm;
    mvm_error error;
    mvm_status status;
    mvm_value result;
    uint32_t entry;
    mvm_engine engine = MVM_ENGINE_BYTECODE;
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--threaded"))) {
        fprintf(stderr, "usage: %s MODULE [--threaded]\n", argv[0]);
        return 2;
    }
    if (argc == 3) engine = MVM_ENGINE_THREADED;
    status = mvm_load_file(argv[1], 0, &module, &error);
    if (status != MVM_OK) {
        fprintf(stderr, "%s:%zu: %s\n", argv[1], error.line, error.message);
        return 1;
    }
    status = mvm_module_find(module, "main", &entry);
    if (status != MVM_OK) {
        fprintf(stderr, "module has no main function\n");
        mvm_module_destroy(module);
        return 1;
    }
    vm = mvm_create(NULL, &error);
    if (!vm) {
        fprintf(stderr, "%s\n", error.message);
        mvm_module_destroy(module);
        return 1;
    }
    status = mvm_run(vm, module, entry, NULL, 0, engine, &result, &error);
    if (status != MVM_OK) fprintf(stderr, "function %zu instruction %zu: %s\n",
        error.function, error.instruction, error.message);
    else {
        switch (result.kind) {
        case MVM_NIL: puts("nil"); break;
        case MVM_BOOL: puts(result.as.boolean ? "true" : "false"); break;
        case MVM_INT: printf("%" PRId64 "\n", result.as.integer); break;
        case MVM_OBJECT: {
            mvm_object_kind kind;
            size_t length;
            if (mvm_object_type(vm, result, &kind) == MVM_OK &&
                mvm_object_length(vm, result, &length) == MVM_OK)
                printf("%s[%zu]\n", mvm_object_describe(kind)->name, length);
            break;
        }
        }
    }
    mvm_destroy(vm); mvm_module_destroy(module);
    return status == MVM_OK ? 0 : 1;
}
