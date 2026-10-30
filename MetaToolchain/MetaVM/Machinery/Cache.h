#ifndef METAVM_CACHE_H
#define METAVM_CACHE_H
#include "Module.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mvm_cache mvm_cache;
/* Bounded LRU cache of independently owned verified module snapshots. Caller
 * synchronizes access. get returns a borrowed view until put/clear/destroy. */
mvm_cache *mvm_cache_create(size_t capacity);
void mvm_cache_destroy(mvm_cache *);
void mvm_cache_clear(mvm_cache *);
mvm_status mvm_cache_put(mvm_cache *, const char *key, const mvm_module *, mvm_error *);
const mvm_module *mvm_cache_get(mvm_cache *, const char *key);
#ifdef __cplusplus
}
#endif
#endif
