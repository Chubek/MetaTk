/**
 * @file mulib-semver.h
 * @brief Semantic version triplet parsing and numeric precedence comparison.
 * @defgroup mulib_semver MuLIB semver
 * @{
 * @details Implements `MAJOR.MINOR.PATCH` only. Prerelease tags and build metadata from the complete SemVer 2.0 specification are not supported.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SEMVER before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_SEMVER_H
#define MULIB_SEMVER_H
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

/** @brief Semantic version triplet parsing and numeric precedence comparison. */
typedef struct mulib_semver { unsigned major,minor,patch; } mulib_semver_t;
/** @brief mulib semver parse; see manual/mulib-semver.md for contracts and examples. */
int mulib_semver_parse(const char*source,mulib_semver_t*out);
/** @brief mulib semver compare; see manual/mulib-semver.md for contracts and examples. */
int mulib_semver_compare(mulib_semver_t a,mulib_semver_t b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SEMVER_H */

#if defined(MULIB_IMPL_SEMVER) && !defined(MULIB_SEMVER_IMPLEMENTED)
#define MULIB_SEMVER_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_semver_parse(const char*s,mulib_semver_t*out){if(!s||!out)return 0;unsigned a,b,c;int n=0;if(sscanf(s,"%u.%u.%u%n",&a,&b,&c,&n)!=3||!n||s[n])return 0;*out=(mulib_semver_t){a,b,c};return 1;}
int mulib_semver_compare(mulib_semver_t a,mulib_semver_t b){if(a.major!=b.major)return a.major<b.major?-1:1;if(a.minor!=b.minor)return a.minor<b.minor?-1:1;return a.patch==b.patch?0:a.patch<b.patch?-1:1;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
