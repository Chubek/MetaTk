/**
 * @file mulib-geometry.h
 * @brief Basic planar point geometry including distances and orientation.
 * @defgroup mulib_geometry MuLIB geometry
 * @{
 * @details The polygon area is signed: counterclockwise polygons have positive area. Geometric computations use double precision and do not provide adaptive exact predicates.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_GEOMETRY before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_GEOMETRY_H
#define MULIB_GEOMETRY_H
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

/** @brief Basic planar point geometry including distances and orientation. */
typedef struct mulib_geometry_point { double x,y; } mulib_geometry_point_t;
/** @brief mulib geometry distance; see manual/mulib-geometry.md for contracts and examples. */
double mulib_geometry_distance(mulib_geometry_point_t a,mulib_geometry_point_t b);
/** @brief mulib geometry cross; see manual/mulib-geometry.md for contracts and examples. */
double mulib_geometry_cross(mulib_geometry_point_t a,mulib_geometry_point_t b,mulib_geometry_point_t c);
/** @brief mulib geometry polygon area; see manual/mulib-geometry.md for contracts and examples. */
double mulib_geometry_polygon_area(const mulib_geometry_point_t*points,size_t n);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_GEOMETRY_H */

#if defined(MULIB_IMPL_GEOMETRY) && !defined(MULIB_GEOMETRY_IMPLEMENTED)
#define MULIB_GEOMETRY_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
double mulib_geometry_distance(mulib_geometry_point_t a,mulib_geometry_point_t b){return hypot(a.x-b.x,a.y-b.y);}
double mulib_geometry_cross(mulib_geometry_point_t a,mulib_geometry_point_t b,mulib_geometry_point_t c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
double mulib_geometry_polygon_area(const mulib_geometry_point_t*p,size_t n){if(!p||n<3)return 0;double acc=0;for(size_t i=0;i<n;i++){size_t j=(i+1)%n;acc+=p[i].x*p[j].y-p[j].x*p[i].y;}return acc/2;}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
