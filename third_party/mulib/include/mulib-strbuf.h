/**
 * @file mulib-strbuf.h
 * @brief Dynamically growing NUL-terminated text builder.
 * @defgroup mulib_strbuf MuLIB strbuf
 * @{
 * @details Successful append always maintains a trailing NUL. For empty newly initialized builders data is NULL until the first append. Appending from within the same buffer may be unsafe if growth reallocates it.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_STRBUF before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_STRBUF_H
#define MULIB_STRBUF_H
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

/** @brief Dynamically growing NUL-terminated text builder. */
typedef struct mulib_strbuf { char *data; size_t length,capacity; } mulib_strbuf_t;
/** @brief mulib strbuf init; see manual/mulib-strbuf.md for contracts and examples. */
void mulib_strbuf_init(mulib_strbuf_t*s);
/** @brief mulib strbuf append n; see manual/mulib-strbuf.md for contracts and examples. */
int mulib_strbuf_append_n(mulib_strbuf_t*s,const char*text,size_t n);
/** @brief mulib strbuf append; see manual/mulib-strbuf.md for contracts and examples. */
int mulib_strbuf_append(mulib_strbuf_t*s,const char*text);
/** @brief mulib strbuf putc; see manual/mulib-strbuf.md for contracts and examples. */
int mulib_strbuf_putc(mulib_strbuf_t*s,char c);
/** @brief mulib strbuf free; see manual/mulib-strbuf.md for contracts and examples. */
void mulib_strbuf_free(mulib_strbuf_t*s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_STRBUF_H */

#if defined(MULIB_IMPL_STRBUF) && !defined(MULIB_STRBUF_IMPLEMENTED)
#define MULIB_STRBUF_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
void mulib_strbuf_init(mulib_strbuf_t*s){if(s)memset(s,0,sizeof(*s));}
int mulib_strbuf_append_n(mulib_strbuf_t*s,const char*t,size_t n){if(!s||(!t&&n)||n>SIZE_MAX-s->length-1)return 0;size_t need=s->length+n+1;if(need>s->capacity){size_t cap=s->capacity?s->capacity:32;while(cap<need){if(cap>SIZE_MAX/2){cap=need;break;}cap*=2;}char*p=realloc(s->data,cap);if(!p)return 0;s->data=p;s->capacity=cap;}if(n)memmove(s->data+s->length,t,n);s->length+=n;s->data[s->length]='\0';return 1;}
int mulib_strbuf_append(mulib_strbuf_t*s,const char*t){return t?mulib_strbuf_append_n(s,t,strlen(t)):0;}
int mulib_strbuf_putc(mulib_strbuf_t*s,char c){return mulib_strbuf_append_n(s,&c,1);}
void mulib_strbuf_free(mulib_strbuf_t*s){if(s){free(s->data);memset(s,0,sizeof(*s));}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
