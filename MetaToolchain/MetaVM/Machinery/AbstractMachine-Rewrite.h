#ifndef METAVM_ABSTRACT_MACHINE_REWRITE_H
#define METAVM_ABSTRACT_MACHINE_REWRITE_H
#include "Module.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Replace a single instruction, verify whole module, roll back on failure. */
mvm_status mvm_module_replace(mvm_module *, uint32_t function, size_t pc,
    mvm_instruction replacement, mvm_error *);
#ifdef __cplusplus
}
#endif
#endif
