#ifndef METAVM_EXECUTION_ENGINE_H
#define METAVM_EXECUTION_ENGINE_H
#include "Module.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mvm_engine { MVM_ENGINE_BYTECODE, MVM_ENGINE_THREADED } mvm_engine;
/* limits==NULL selects defaults; zero limits are invalid. VM operations require
 * caller synchronization. Independent VMs may execute shared immutable modules.
 * Failures return a status and optional diagnostic; never abort/exit/longjmp. */
mvm_vm *mvm_create(const mvm_limits *limits, mvm_error *);
void mvm_destroy(mvm_vm *); /* NULL accepted; releases all roots and objects. */
/* The result is set to nil on failure and rooted by the VM until the next run.
 * Host-held object arguments must be rooted across allocations. Reentrant run
 * on the same VM is rejected. All frames unwind on error; the VM is reusable.
 * max_steps is per invocation, shared by all calls; each instruction costs 1. */
mvm_status mvm_run(mvm_vm *, const mvm_module *, uint32_t function,
    const mvm_value *arguments, size_t count, mvm_engine,
    mvm_value *result, mvm_error *);
uint64_t mvm_steps(const mvm_vm *);
#ifdef __cplusplus
}
#endif
#endif
