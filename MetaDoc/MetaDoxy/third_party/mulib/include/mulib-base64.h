/**
 * @file mulib-base64.h
 * @brief RFC 4648 padded Base64 encoder and strict decoder.
 * @defgroup mulib_base64 MuLIB base64
 * @{
 * @details Encoder requires a destination buffer including one final NUL. Decoder accepts padded standard alphabet only (not URL-safe alphabet or whitespace); errors do not guarantee that the output buffer was left untouched.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_BASE64 before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_BASE64_H
#define MULIB_BASE64_H
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

/** @brief mulib base64 encode; see manual/mulib-base64.md for contracts and examples. */
size_t mulib_base64_encode(const void*data,size_t n,char*out,size_t capacity);
/** @brief mulib base64 decode; see manual/mulib-base64.md for contracts and examples. */
int mulib_base64_decode(const char*data,size_t n,unsigned char*out,size_t capacity,size_t*written);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_BASE64_H */

#if defined(MULIB_IMPL_BASE64) && !defined(MULIB_BASE64_IMPLEMENTED)
#define MULIB_BASE64_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_base64_encode(const void*p,size_t n,char*out,size_t cap){static const char a[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";if((!p&&n)||n>(SIZE_MAX-1)/4*3)return 0;size_t need=((n+2)/3)*4;if(!out||cap<need+1)return 0;const unsigned char*s=p;size_t j=0;for(size_t i=0;i<n;i+=3){uint32_t x=(uint32_t)s[i]<<16;if(i+1<n)x|=(uint32_t)s[i+1]<<8;if(i+2<n)x|=s[i+2];out[j++]=a[(x>>18)&63];out[j++]=a[(x>>12)&63];out[j++]=i+1<n?a[(x>>6)&63]:'=';out[j++]=i+2<n?a[x&63]:'=';}out[j]=0;return j;}
static int mulib_base64_value_(char c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;}
int mulib_base64_decode(const char*s,size_t n,unsigned char*out,size_t cap,size_t*written){if(!s||n%4||(!out&&n))return 0;size_t pos=0;for(size_t i=0;i<n;i+=4){int v[4];for(int k=0;k<4;k++)v[k]=s[i+k]=='='?-2:mulib_base64_value_(s[i+k]);if(v[0]<0||v[1]<0||v[2]==-1||v[3]==-1||(v[2]==-2&&v[3]!=-2)||((v[2]<0||v[3]<0)&&i+4!=n))return 0;uint32_t x=(uint32_t)v[0]<<18|(uint32_t)v[1]<<12|(v[2]>=0?(uint32_t)v[2]<<6:0)|(v[3]>=0?(uint32_t)v[3]:0);size_t z=v[2]<0?1:(v[3]<0?2:3);if(pos>cap||z>cap-pos)return 0;out[pos++]=(unsigned char)(x>>16);if(z>1)out[pos++]=(unsigned char)(x>>8);if(z>2)out[pos++]=(unsigned char)x;if((z==1&&(v[1]&15))||(z==2&&(v[2]&3)))return 0;}if(written)*written=pos;return 1;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
