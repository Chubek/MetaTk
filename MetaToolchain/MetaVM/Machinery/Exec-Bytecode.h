#ifndef METAVM_EXEC_BYTECODE_H
#define METAVM_EXEC_BYTECODE_H
#include "ExecutionEngine.h"
#ifdef __cplusplus
extern "C" {
#endif
mvm_status mvm_run_bytecode(mvm_vm *, const mvm_module *, uint32_t,
    const mvm_value *, size_t, mvm_value *, mvm_error *);
#ifdef __cplusplus
}
#endif
#endif
