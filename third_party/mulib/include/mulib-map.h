/**
 * @file mulib-map.h
 * @brief String-keyed dictionary with owning keys and borrowed values.
 * @defgroup mulib_map MuLIB map
 * @{
 * @details This is a compact linear-search dictionary, not a hash map. Use for modest configuration tables, not large high-throughput indexes.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MAP before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_MAP_H
#define MULIB_MAP_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <inttypes.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief String-keyed dictionary with owning keys and borrowed values. */
typedef struct mulib_map_entry { char *key; void *value; } mulib_map_entry_t;
/** @brief String-keyed dictionary with owning keys and borrowed values. */
typedef struct mulib_map { mulib_map_entry_t *items; size_t length,capacity; } mulib_map_t;
/** @brief mulib map init; see manual/mulib-map.md for contracts and examples. */
void mulib_map_init(mulib_map_t*m);
/** @brief mulib map set; see manual/mulib-map.md for contracts and examples. */
int mulib_map_set(mulib_map_t*m,const char*key,void*value);
/** @brief mulib map get; see manual/mulib-map.md for contracts and examples. */
int mulib_map_get(const mulib_map_t*m,const char*key,void**out);
/** @brief mulib map remove; see manual/mulib-map.md for contracts and examples. */
int mulib_map_remove(mulib_map_t*m,const char*key);
/** @brief mulib map free; see manual/mulib-map.md for contracts and examples. */
void mulib_map_free(mulib_map_t*m);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MAP_H */

#if defined(MULIB_IMPL_MAP) && !defined(MULIB_MAP_IMPLEMENTED)
#define MULIB_MAP_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_map_init(mulib_map_t*m){if(m)memset(m,0,sizeof(*m));}
int mulib_map_set(mulib_map_t*m,const char*k,void*v){if(!m||!k)return 0;for(size_t i=0;i<m->length;i++)if(!strcmp(k,m->items[i].key)){m->items[i].value=v;return 1;}if(m->length==m->capacity){size_t n=m->capacity?m->capacity*2:8;if(n<m->capacity||n>SIZE_MAX/sizeof(*m->items))return 0;void*p=realloc(m->items,n*sizeof(*m->items));if(!p)return 0;m->items=p;m->capacity=n;}size_t len=strlen(k);if(len==SIZE_MAX)return 0;char*dup=malloc(len+1);if(!dup)return 0;memcpy(dup,k,len+1);m->items[m->length++]=(mulib_map_entry_t){dup,v};return 1;}
int mulib_map_get(const mulib_map_t*m,const char*k,void**out){if(!m||!k)return 0;for(size_t i=0;i<m->length;i++)if(!strcmp(k,m->items[i].key)){if(out)*out=m->items[i].value;return 1;}return 0;}
int mulib_map_remove(mulib_map_t*m,const char*k){if(!m||!k)return 0;for(size_t i=0;i<m->length;i++)if(!strcmp(k,m->items[i].key)){free(m->items[i].key);memmove(m->items+i,m->items+i+1,(m->length-i-1)*sizeof(*m->items));m->length--;return 1;}return 0;}
void mulib_map_free(mulib_map_t*m){if(m){for(size_t i=0;i<m->length;i++)free(m->items[i].key);free(m->items);memset(m,0,sizeof(*m));}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
