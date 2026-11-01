/**
 * @file mulib-csv.h
 * @brief Streaming CSV row parser for quoted comma-separated fields.
 * @defgroup mulib_csv MuLIB csv
 * @{
 * @details Parses escaped double quotes (`""`) and quoted delimiters/newlines. Row fields are owned temporarily by the parser and are valid only for the duration of a callback invocation. This is a lightweight dialect, not a dialect autodetector.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_CSV before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_CSV_H
#define MULIB_CSV_H
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

/** @brief Streaming CSV row parser for quoted comma-separated fields. */
typedef int (*mulib_csv_row_callback)(void*user,const char *const*fields,size_t count);
/** @brief mulib csv parse; see manual/mulib-csv.md for contracts and examples. */
int mulib_csv_parse(const char*source,mulib_csv_row_callback callback,void*user);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSV_H */

#if defined(MULIB_IMPL_CSV) && !defined(MULIB_CSV_IMPLEMENTED)
#define MULIB_CSV_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_csv_parse(const char*s,mulib_csv_row_callback cb,void*user){if(!s||!cb)return 0;size_t cap=8,n=0;char**fields=malloc(cap*sizeof(char*));if(!fields)return 0;int ok=1;while(*s){n=0;int done=0;while(!done){size_t bc=64,len=0;char*b=malloc(bc);if(!b){ok=0;goto end;}int quoted=*s=='"';if(quoted)s++;int closed=!quoted;while(*s){if(quoted){if(*s=='"'){if(s[1]=='"')s++;else{closed=1;s++;break;}}}else if(*s==','||*s=='\n'||*s=='\r')break;if(len+1>=bc){size_t nc=bc*2;char*p=realloc(b,nc);if(!p){free(b);ok=0;goto end;}b=p;bc=nc;}b[len++]=*s++;}if(!closed){free(b);ok=0;goto end;}b[len]=0;if(n==cap){size_t nc=cap*2;char**p=realloc(fields,nc*sizeof(char*));if(!p){free(b);ok=0;goto end;}fields=p;cap=nc;}fields[n++]=b;if(*s==','){s++;continue;}if(*s=='\r')s++;if(*s=='\n')s++;done=1;}if(!cb(user,(const char*const*)fields,n)){ok=0;goto end;}for(size_t i=0;i<n;i++)free(fields[i]);n=0;}end:for(size_t i=0;i<n;i++)free(fields[i]);free(fields);return ok;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
