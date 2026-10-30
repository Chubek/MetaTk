#ifndef METAVM_MEMORY_H
#define METAVM_MEMORY_H
#include "System.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mvm_memory_stats { size_t objects, heap_bytes, roots; } mvm_memory_stats;
/* Heap bytes include object headers and payloads, not modules or host roots. */
mvm_memory_stats mvm_memory_usage(const mvm_vm *);
#ifdef __cplusplus
}
#endif
#endif
