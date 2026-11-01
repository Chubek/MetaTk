#ifndef METAVM_LIBRARY_P4_H
#define METAVM_LIBRARY_P4_H
#include "MetaVM.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Portable Machinery example, not a native guest loader. */
mvm_status P4_example_module(mvm_module **,mvm_error *);
#ifdef __cplusplus
}
#endif
#endif
