/**
 * @file mulib-arena.h
 * @brief Bump allocator with expandable allocation blocks.
 * @defgroup mulib_arena MuLIB arena
 * @{
 * @details Pointer stability is guaranteed until reset. Individual allocations cannot be freed. Allocation aligns to max_align_t and cannot hold over-aligned types.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ARENA before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_ARENA_H
#define MULIB_ARENA_H
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

/** @brief Bump allocator with expandable allocation blocks. */
typedef struct mulib_arena_block { struct mulib_arena_block *next; size_t used,capacity; max_align_t align; unsigned char data[1]; } mulib_arena_block_t;
/** @brief Bump allocator with expandable allocation blocks. */
typedef struct mulib_arena { mulib_arena_block_t *head; size_t block_size; } mulib_arena_t;
/** @brief mulib arena init; see manual/mulib-arena.md for contracts and examples. */
void mulib_arena_init(mulib_arena_t*a,size_t block_size);
/** @brief mulib arena alloc; see manual/mulib-arena.md for contracts and examples. */
void *mulib_arena_alloc(mulib_arena_t*a,size_t size);
/** @brief mulib arena reset; see manual/mulib-arena.md for contracts and examples. */
void mulib_arena_reset(mulib_arena_t*a);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ARENA_H */

#if defined(MULIB_IMPL_ARENA) && !defined(MULIB_ARENA_IMPLEMENTED)
#define MULIB_ARENA_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_arena_init(mulib_arena_t*a,size_t n){if(a){a->head=NULL;a->block_size=n?n:4096;}}
void *mulib_arena_alloc(mulib_arena_t*a,size_t n){if(!a)return NULL;size_t align=sizeof(max_align_t);if(n>SIZE_MAX-(align-1))return NULL;n=(n+align-1)/align*align;if(!n)n=align;mulib_arena_block_t*b=a->head;if(!b||n>b->capacity-b->used){size_t cap=a->block_size;if(cap<n)cap=n;if(cap>SIZE_MAX-sizeof(*b))return NULL;b=malloc(sizeof(*b)+cap);if(!b)return NULL;b->next=a->head;b->used=0;b->capacity=cap;a->head=b;}void*p=b->data+b->used;b->used+=n;return p;}
void mulib_arena_reset(mulib_arena_t*a){if(!a)return;mulib_arena_block_t*b=a->head;while(b){mulib_arena_block_t*n=b->next;free(b);b=n;}a->head=NULL;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
