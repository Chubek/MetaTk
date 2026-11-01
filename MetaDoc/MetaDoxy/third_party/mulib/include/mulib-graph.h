/**
 * @file mulib-graph.h
 * @brief Directed graph with dense adjacency matrix and breadth-first search.
 * @defgroup mulib_graph MuLIB graph
 * @{
 * @details Dense representation uses O(V²) space; BFS discovers each reachable vertex once, visiting adjacency rows in ascending vertex order.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_GRAPH before including this file.
 * Other translation units may include it without the implementation switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard library.
 * @}
 */
#ifndef MULIB_GRAPH_H
#define MULIB_GRAPH_H
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

/** @brief Directed graph with dense adjacency matrix and breadth-first search. */
typedef struct mulib_graph { unsigned char *edges; size_t n; } mulib_graph_t;
/** @brief mulib graph init; see manual/mulib-graph.md for contracts and examples. */
int mulib_graph_init(mulib_graph_t*g,size_t vertices);
/** @brief mulib graph edge; see manual/mulib-graph.md for contracts and examples. */
int mulib_graph_edge(mulib_graph_t*g,size_t src,size_t dst,int enabled);
/** @brief mulib graph bfs; see manual/mulib-graph.md for contracts and examples. */
int mulib_graph_bfs(const mulib_graph_t*g,size_t start,size_t *order,size_t capacity);
/** @brief mulib graph free; see manual/mulib-graph.md for contracts and examples. */
void mulib_graph_free(mulib_graph_t*g);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_GRAPH_H */

#if defined(MULIB_IMPL_GRAPH) && !defined(MULIB_GRAPH_IMPLEMENTED)
#define MULIB_GRAPH_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_graph_init(mulib_graph_t*g,size_t n){if(!g||(n&&n>SIZE_MAX/n))return 0;g->n=n;g->edges=calloc((n?n*n:1),1);return g->edges!=NULL;}
int mulib_graph_edge(mulib_graph_t*g,size_t a,size_t b,int on){if(!g||a>=g->n||b>=g->n)return 0;g->edges[a*g->n+b]=(unsigned char)!!on;return 1;}
int mulib_graph_bfs(const mulib_graph_t*g,size_t start,size_t*out,size_t cap){if(!g||start>=g->n||!out||cap<g->n||g->n>SIZE_MAX/sizeof(size_t))return -1;unsigned char*seen=calloc(g->n,1);size_t*q=malloc(g->n*sizeof(size_t));if(!seen||!q){free(q);free(seen);return -1;}size_t head=0,tail=0;q[tail++]=start;seen[start]=1;while(head<tail){size_t u=q[head++];out[head-1]=u;for(size_t v=0;v<g->n;v++)if(g->edges[u*g->n+v]&&!seen[v]){seen[v]=1;q[tail++]=v;}}free(q);free(seen);return tail>INT_MAX?-1:(int)tail;}
void mulib_graph_free(mulib_graph_t*g){if(g){free(g->edges);memset(g,0,sizeof(*g));}}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
