#ifndef METAVM_LIBRARY_CONCURRENTML_H
#define METAVM_LIBRARY_CONCURRENTML_H
#include "MetaVM.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Portable Machinery example, not a native guest loader. */
mvm_status ConcurrentML_example_module(mvm_module **,mvm_error *);
#ifdef __cplusplus
}
#endif
#endif
