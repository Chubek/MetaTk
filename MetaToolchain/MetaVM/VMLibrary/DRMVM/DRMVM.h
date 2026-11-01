#ifndef METAVM_LIBRARY_DRMVM_H
#define METAVM_LIBRARY_DRMVM_H
#include "MetaVM.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Portable Machinery example, not a native guest loader. */
mvm_status DRMVM_example_module(mvm_module **,mvm_error *);
#ifdef __cplusplus
}
#endif
#endif
