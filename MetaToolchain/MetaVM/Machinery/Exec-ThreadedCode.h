#ifndef METAVM_EXEC_THREADED_CODE_H
#define METAVM_EXEC_THREADED_CODE_H
#include "ExecutionEngine.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Portable indirect threading: predecoded C function pointers, iterative
 * dispatch, no computed goto, executable allocation, or native JIT dependency. */
mvm_status mvm_run_threaded(mvm_vm *, const mvm_module *, uint32_t,
    const mvm_value *, size_t, mvm_value *, mvm_error *);
#ifdef __cplusplus
}
#endif
#endif
