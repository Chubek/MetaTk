/**
 * @file mulib-prime.h
 * @brief Primality testing and next-prime search for 64-bit integers.
 * @defgroup mulib_prime MuLIB prime
 * @{
 * @details Deterministic trial division is appropriate for small to medium inputs but may be prohibitively slow near 64-bit limits. This module is not a cryptographic prime generator.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_PRIME before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_PRIME_H
#define MULIB_PRIME_H
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

/** @brief mulib prime is prime; see manual/mulib-prime.md for contracts and examples. */
int mulib_prime_is_prime(uint64_t number);
/** @brief mulib prime next; see manual/mulib-prime.md for contracts and examples. */
uint64_t mulib_prime_next(uint64_t number);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_PRIME_H */

#if defined(MULIB_IMPL_PRIME) && !defined(MULIB_PRIME_IMPLEMENTED)
#define MULIB_PRIME_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_prime_is_prime(uint64_t n){if(n<2)return 0;if(n%2==0)return n==2;if(n%3==0)return n==3;for(uint64_t i=5;i<=n/i;i+=6)if(n%i==0||n%(i+2)==0)return 0;return 1;}
uint64_t mulib_prime_next(uint64_t n){if(n<=2)return 2;if(!(n&1))n++;while(n){if(mulib_prime_is_prime(n))return n;if(n>UINT64_MAX-2)break;n+=2;}return 0;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
