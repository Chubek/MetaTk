#ifndef METAVM_METAIL_REWRITE_H
#define METAVM_METAIL_REWRITE_H
#include "MetaIL-ISel.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Fold a selected pure scalar operation using the VM's exact semantics. */
mvm_status mvm_fold_integer_operation(const char *name, int64_t, int64_t, int64_t *);
#ifdef __cplusplus
}
#endif
#endif
