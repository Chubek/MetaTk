#include "Internal.h"

typedef struct cache_entry { char *key; mvm_module *module; } cache_entry;
struct mvm_cache { cache_entry *entries; size_t count, capacity; };
mvm_cache *mvm_cache_create(size_t capacity) {
    mvm_cache *cache;
    if (!capacity || capacity > SIZE_MAX / sizeof(cache_entry)) return NULL;
    cache = calloc(1, sizeof(*cache));
    if (!cache) return NULL;
    cache->entries = calloc(capacity, sizeof(cache_entry));
    if (!cache->entries) { free(cache); return NULL; }
    cache->capacity = capacity;
    return cache;
}
void mvm_cache_clear(mvm_cache *cache) {
    size_t i;
    if (!cache) return;
    for (i = 0; i < cache->count; ++i) {
        free(cache->entries[i].key);
        mvm_module_destroy(cache->entries[i].module);
    }
    cache->count = 0;
}
void mvm_cache_destroy(mvm_cache *cache) {
    if (!cache) return;
    mvm_cache_clear(cache); free(cache->entries); free(cache);
}
static void touch(mvm_cache *cache, size_t i) {
    cache_entry entry = cache->entries[i];
    if (i) memmove(cache->entries + 1, cache->entries, i * sizeof(entry));
    cache->entries[0] = entry;
}
const mvm_module *mvm_cache_get(mvm_cache *cache, const char *key) {
    size_t i;
    if (!cache || !key) return NULL;
    for (i = 0; i < cache->count; ++i) {
        if (!strcmp(key, cache->entries[i].key)) {
            touch(cache, i); return cache->entries[0].module;
        }
    }
    return NULL;
}
mvm_status mvm_cache_put(mvm_cache *cache, const char *key, const mvm_module *m, mvm_error *e) {
    mvm_module *copy;
    char *name;
    size_t i;
    mvm_status s;
    mvm_error_clear(e);
    if (!cache || !key) return mvm_fail(e, MVM_INVALID, "invalid cache arguments");
    s = mvm_module_clone(m, &copy, e);
    if (s != MVM_OK) return s;
    name = mvm_copy_string(key);
    if (!name) { mvm_module_destroy(copy); return mvm_fail(e, MVM_OOM, "cache key allocation failed"); }
    for (i = 0; i < cache->count; ++i) if (!strcmp(key, cache->entries[i].key)) break;
    if (i == cache->count && cache->count < cache->capacity) ++cache->count;
    else {
        if (i == cache->count) i = cache->count - 1;
        free(cache->entries[i].key);
        mvm_module_destroy(cache->entries[i].module);
    }
    cache->entries[i].key = name; cache->entries[i].module = copy;
    touch(cache, i);
    return MVM_OK;
}
