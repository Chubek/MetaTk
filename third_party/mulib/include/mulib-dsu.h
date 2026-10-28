/**
 * @file mulib-dsu.h
 * @brief Union-find disjoint-set forest with path compression and union by rank.
 * @defgroup mulib_dsu MuLIB dsu
 * @{
 * @details Find returns SIZE_MAX for out-of-range indices, which cannot be a valid root in an allocated DSU.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_DSU before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_DSU_H
#define MULIB_DSU_H
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

/** @brief Union-find disjoint-set forest with path compression and union by rank. */
typedef struct mulib_dsu { size_t *parent; unsigned char *rank; size_t count; } mulib_dsu_t;
/** @brief mulib dsu init; see manual/mulib-dsu.md for contracts and examples. */
int mulib_dsu_init(mulib_dsu_t*d,size_t n);
/** @brief mulib dsu find; see manual/mulib-dsu.md for contracts and examples. */
size_t mulib_dsu_find(mulib_dsu_t*d,size_t i);
/** @brief mulib dsu union; see manual/mulib-dsu.md for contracts and examples. */
int mulib_dsu_union(mulib_dsu_t*d,size_t a,size_t b);
/** @brief mulib dsu free; see manual/mulib-dsu.md for contracts and examples. */
void mulib_dsu_free(mulib_dsu_t*d);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DSU_H */

#if defined(MULIB_IMPL_DSU) && !defined(MULIB_DSU_IMPLEMENTED)
#define MULIB_DSU_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_dsu_init(mulib_dsu_t*d,size_t n){if(!d||n>SIZE_MAX/sizeof(size_t))return 0;memset(d,0,sizeof(*d));d->parent=malloc((n?n:1)*sizeof(size_t));d->rank=calloc(n?n:1,1);if(!d->parent||!d->rank){mulib_dsu_free(d);return 0;}d->count=n;for(size_t i=0;i<n;i++)d->parent[i]=i;return 1;}
size_t mulib_dsu_find(mulib_dsu_t*d,size_t i){if(!d||i>=d->count)return SIZE_MAX;size_t x=i;while(d->parent[x]!=x)x=d->parent[x];while(i!=x){size_t p=d->parent[i];d->parent[i]=x;i=p;}return x;}
int mulib_dsu_union(mulib_dsu_t*d,size_t a,size_t b){a=mulib_dsu_find(d,a);b=mulib_dsu_find(d,b);if(a==SIZE_MAX||b==SIZE_MAX)return 0;if(a==b)return 1;if(d->rank[a]<d->rank[b]){size_t t=a;a=b;b=t;}d->parent[b]=a;if(d->rank[a]==d->rank[b])d->rank[a]++;return 1;}
void mulib_dsu_free(mulib_dsu_t*d){if(d){free(d->parent);free(d->rank);memset(d,0,sizeof(*d));}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
