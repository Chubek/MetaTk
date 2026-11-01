/**
 * @file mulib-list.h
 * @brief Intrusive doubly linked circular list with sentinel.
 * @defgroup mulib_list MuLIB list
 * @{
 * @details Nodes belong to caller-defined structs. Initialize sentinel with
 * mulib_list_init, then link previously unlinked nodes. Never insert a node
 * already linked into a list.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_LIST before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_LIST_H
#define MULIB_LIST_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Intrusive doubly linked circular list with sentinel. */
  typedef struct mulib_list_node
  {
    struct mulib_list_node *prev, *next;
  } mulib_list_node_t;
  /** @brief mulib list init; see manual/mulib-list.md for contracts and
   * examples. */
  void mulib_list_init (mulib_list_node_t *root);
  /** @brief mulib list empty; see manual/mulib-list.md for contracts and
   * examples. */
  int mulib_list_empty (const mulib_list_node_t *root);
  /** @brief mulib list append; see manual/mulib-list.md for contracts and
   * examples. */
  void mulib_list_append (mulib_list_node_t *root, mulib_list_node_t *node);
  /** @brief mulib list prepend; see manual/mulib-list.md for contracts and
   * examples. */
  void mulib_list_prepend (mulib_list_node_t *root, mulib_list_node_t *node);
  /** @brief mulib list remove; see manual/mulib-list.md for contracts and
   * examples. */
  void mulib_list_remove (mulib_list_node_t *node);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_LIST_H */

#if defined(MULIB_IMPL_LIST) && !defined(MULIB_LIST_IMPLEMENTED)
#define MULIB_LIST_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_list_init (mulib_list_node_t *r)
  {
    if (r)
      r->next = r->prev = r;
  }
  int
  mulib_list_empty (const mulib_list_node_t *r)
  {
    return r && r->next == r;
  }
  void
  mulib_list_append (mulib_list_node_t *r, mulib_list_node_t *n)
  {
    if (!r || !n)
      return;
    n->next = r;
    n->prev = r->prev;
    r->prev->next = n;
    r->prev = n;
  }
  void
  mulib_list_prepend (mulib_list_node_t *r, mulib_list_node_t *n)
  {
    if (!r || !n)
      return;
    n->prev = r;
    n->next = r->next;
    r->next->prev = n;
    r->next = n;
  }
  void
  mulib_list_remove (mulib_list_node_t *n)
  {
    if (!n || !n->next || !n->prev)
      return;
    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->next = n->prev = NULL;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
