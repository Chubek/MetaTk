#ifndef METAVM_LOADER_H
#define METAVM_LOADER_H
#include "Module.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Detect MVM binary magic; otherwise compile assembly. Caller owns output. */
mvm_status mvm_load_file(const char *path, size_t max_bytes, mvm_module **, mvm_error *);
mvm_status mvm_save_file(const char *path, const mvm_module *, mvm_error *);
#ifdef __cplusplus
}
#endif
#endif
