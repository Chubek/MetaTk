/**
 * @file mulib-utf8.h
 * @brief UTF-8 validation and single-code-point decoding.
 * @defgroup mulib_utf8 MuLIB utf8
 * @{
 * @details Rejects overlong encodings, surrogates, truncation, and scalar values above U+10FFFF. This module does not implement Unicode normalization, case folding, or grapheme segmentation.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_UTF8 before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_UTF8_H
#define MULIB_UTF8_H
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

/** @brief mulib utf8 decode; see manual/mulib-utf8.md for contracts and examples. */
int mulib_utf8_decode(const unsigned char *bytes,size_t length,uint32_t*codepoint,size_t*consumed);
/** @brief mulib utf8 validate; see manual/mulib-utf8.md for contracts and examples. */
int mulib_utf8_validate(const unsigned char*bytes,size_t length);
/** @brief mulib utf8 encode; see manual/mulib-utf8.md for contracts and examples. */
size_t mulib_utf8_encode(uint32_t codepoint,unsigned char*out,size_t capacity);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_UTF8_H */

#if defined(MULIB_IMPL_UTF8) && !defined(MULIB_UTF8_IMPLEMENTED)
#define MULIB_UTF8_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_utf8_decode(const unsigned char*s,size_t n,uint32_t*out,size_t*used){if(!s||!n||!out)return 0;uint32_t c=s[0];size_t len=1;if(c<0x80){}else if(c>=0xc2&&c<=0xdf){len=2;c&=0x1f;}else if(c>=0xe0&&c<=0xef){len=3;c&=0x0f;}else if(c>=0xf0&&c<=0xf4){len=4;c&=0x07;}else return 0;if(len>n)return 0;for(size_t i=1;i<len;i++){if((s[i]&0xc0)!=0x80)return 0;c=(c<<6)|(s[i]&63);}if((len==2&&c<128)||(len==3&&c<2048)||(len==4&&c<65536)||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return 0;*out=c;if(used)*used=len;return 1;}
int mulib_utf8_validate(const unsigned char*p,size_t n){if(!p&&n)return 0;while(n){uint32_t c;size_t used;if(!mulib_utf8_decode(p,n,&c,&used))return 0;p+=used;n-=used;}return 1;}
size_t mulib_utf8_encode(uint32_t c,unsigned char*out,size_t cap){if(!out||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return 0;size_t n=c<0x80?1:c<0x800?2:c<0x10000?3:4;if(cap<n)return 0;if(n==1)out[0]=(unsigned char)c;else{for(size_t i=n-1;i>0;i--){out[i]=(unsigned char)(0x80|(c&63));c>>=6;}out[0]=(unsigned char)((n==2?0xc0:n==3?0xe0:0xf0)|c);}return n;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
