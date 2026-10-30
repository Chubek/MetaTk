#ifndef METAVM_GC_H
#define METAVM_GC_H
#include "Value.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Non-moving mark/sweep, including cycles. Frames, last successful result and
 * explicit roots are traced. Roots survive run failures. No recursive tracing. */
mvm_status mvm_pin(mvm_vm *, mvm_value, mvm_root **output);
mvm_value mvm_root_value(const mvm_root *);
void mvm_unpin(mvm_root *); /* NULL accepted; root invalid after this call. */
size_t mvm_collect(mvm_vm *); /* Number of objects reclaimed; NULL => 0. */
#ifdef __cplusplus
}
#endif
#endif
