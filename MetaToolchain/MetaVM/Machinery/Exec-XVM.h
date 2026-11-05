#ifndef METAVM_EXEC_XVM_H
#define METAVM_EXEC_XVM_H
#include "ExecutionEngine.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mvm_xvm_native_mode {
    MVM_XVM_NATIVE_AUTO, MVM_XVM_NATIVE_OFF, MVM_XVM_NATIVE_REQUIRED
} mvm_xvm_native_mode;
typedef struct mvm_xvm_options {
    size_t max_plan_bytes, max_native_bytes;
    mvm_xvm_native_mode native_mode;
} mvm_xvm_options;
typedef struct mvm_xvm_statistics {
    size_t instructions, blocks, plan_bytes, native_bytes;
    uint64_t executed_blocks;
    int native;
} mvm_xvm_statistics;
/* Pure defaults: 64 MiB plan, 64 MiB native code, auto native/portable XVM.
 * No module mutation or persistent borrowed instruction cache is performed. */
mvm_xvm_options mvm_xvm_default_options(void);
/* Copies options. Requires an idle VM and positive plan/native ceilings.
 * Failure preserves the previous options. Independent of guest fuel/heap limits. */
mvm_status mvm_xvm_configure(mvm_vm *, const mvm_xvm_options *, mvm_error *);
/* Last started invocation's counters; zeroed for other engines. NULL returns zeroes.
 * Like every VM accessor, requires caller synchronization. */
mvm_xvm_statistics mvm_xvm_stats(const mvm_vm *);
mvm_status mvm_run_xvm(mvm_vm *, const mvm_module *, uint32_t,
    const mvm_value *, size_t, mvm_value *, mvm_error *);
mvm_status mvm_run_default(mvm_vm *, const mvm_module *, uint32_t,
    const mvm_value *, size_t, mvm_value *, mvm_error *);
#ifdef __cplusplus
}
#endif
#endif
