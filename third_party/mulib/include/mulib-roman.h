/**
 * @file mulib-roman.h
 * @brief Roman numeral formatting and a simple validity-checked parser.
 * @defgroup mulib_roman MuLIB roman
 * @{
 * @details Supports canonical Roman numerals 1..3999. Parser validates canonical spelling by formatting the computed value and comparing strings.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ROMAN before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_ROMAN_H
#define MULIB_ROMAN_H
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

/** @brief mulib roman format; see manual/mulib-roman.md for contracts and examples. */
int mulib_roman_format(unsigned value,char*out,size_t cap);
/** @brief mulib roman parse; see manual/mulib-roman.md for contracts and examples. */
unsigned mulib_roman_parse(const char*text);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ROMAN_H */

#if defined(MULIB_IMPL_ROMAN) && !defined(MULIB_ROMAN_IMPLEMENTED)
#define MULIB_ROMAN_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_roman_format(unsigned n,char*out,size_t cap){static const unsigned val[]={1000,900,500,400,100,90,50,40,10,9,5,4,1};static const char*sym[]={"M","CM","D","CD","C","XC","L","XL","X","IX","V","IV","I"};if(!out||!n||n>3999)return 0;size_t j=0;for(size_t i=0;i<13;i++)while(n>=val[i]){size_t z=strlen(sym[i]);if(z>=cap-j)return 0;memcpy(out+j,sym[i],z);j+=z;n-=val[i];}out[j]=0;return 1;}
unsigned mulib_roman_parse(const char*s){if(!s||!*s)return 0;int res=0;for(size_t i=0;s[i];i++){char c=s[i],d=s[i+1];unsigned v=c=='I'?1:c=='V'?5:c=='X'?10:c=='L'?50:c=='C'?100:c=='D'?500:c=='M'?1000:0;unsigned w=d=='I'?1:d=='V'?5:d=='X'?10:d=='L'?50:d=='C'?100:d=='D'?500:d=='M'?1000:0;if(!v||res>3999)return 0;res+=v<w?-(int)v:(int)v;}char canon[32];if(!mulib_roman_format(res,canon,sizeof(canon))||strcmp(canon,s))return 0;return res;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
