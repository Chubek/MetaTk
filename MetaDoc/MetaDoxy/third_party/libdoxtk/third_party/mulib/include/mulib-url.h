/**
 * @file mulib-url.h
 * @brief URL component percent encoder and decoder.
 * @defgroup mulib_url MuLIB url
 * @{
 * @details Implements percent encoding of individual URL components using the unreserved character set. It does not parse full URLs and does not translate `+` to space.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_URL before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_URL_H
#define MULIB_URL_H
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

/** @brief mulib url encode; see manual/mulib-url.md for contracts and examples. */
size_t mulib_url_encode(const unsigned char *s,size_t n,char*out,size_t cap);
/** @brief mulib url decode; see manual/mulib-url.md for contracts and examples. */
int mulib_url_decode(const char*s,size_t n,unsigned char*out,size_t cap,size_t*written);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_URL_H */

#if defined(MULIB_IMPL_URL) && !defined(MULIB_URL_IMPLEMENTED)
#define MULIB_URL_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_url_encode(const unsigned char*s,size_t n,char*out,size_t cap){static const char d[]="0123456789ABCDEF";if((!s&&n)||!out)return 0;size_t j=0;for(size_t i=0;i<n;i++){unsigned char c=s[i];int safe=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~';size_t k=safe?1:3;if(j>cap||k>=cap-j)return 0;if(safe)out[j++]=(char)c;else{out[j++]='%';out[j++]=d[c>>4];out[j++]=d[c&15];}}if(j>=cap)return 0;out[j]=0;return j;}
static int mulib_url_digit_(char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;}
int mulib_url_decode(const char*s,size_t n,unsigned char*out,size_t cap,size_t*written){if(!s||(!out&&n))return 0;size_t j=0;for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)s[i];if(c=='%'){if(i+2>=n)return 0;int a=mulib_url_digit_(s[++i]),b=mulib_url_digit_(s[++i]);if(a<0||b<0)return 0;c=(unsigned char)((a<<4)|b);}if(j>=cap)return 0;out[j++]=c;}if(written)*written=j;return 1;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
