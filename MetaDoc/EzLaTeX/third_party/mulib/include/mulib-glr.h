/**
 * @file mulib-glr.h
 * @brief Bounded generalized LR(0) CFG parsing with a GSS and packed forest.
 *
 * Grammar compilation constructs the canonical LR(0) item automaton using
 * mulib-atn. Parsing retains every shift/reduce and reduce/reduce conflict,
 * saturating reductions to a fixed point at each token position. This is not
 * a deterministic-parser fallback. Epsilon productions, left recursion, and
 * cyclic ambiguity are supported within explicit limits. See the manual for
 * the deliberately simple, potentially expensive path-enumerating algorithm.
 *
 * Define MULIB_IMPL_GLR and explicitly provide implementations of ATN, GSS,
 * SPPF, VEC, and BITSET. No dependency switches are defined by this header.
 */
#ifndef MULIB_GLR_H
#define MULIB_GLR_H
#include "mulib-atn.h"
#include "mulib-gss.h"
#include "mulib-sppf.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

/** Missing symbol, production, or forest-root ID. */
#define MULIB_GLR_NONE SIZE_MAX
  /** SYNTAX means rejection, not resource failure. */
  typedef enum mulib_glr_status
  {
    MULIB_GLR_OK = 0,
    MULIB_GLR_SYNTAX = 1,
    MULIB_GLR_INVALID = -1,
    MULIB_GLR_NOMEM = -2,
    MULIB_GLR_LIMIT = -3,
    MULIB_GLR_UNCOMPILED = -4
  } mulib_glr_status_t;
  /** A terminal consumes one uint32_t token in its inclusive range. */
  typedef enum mulib_glr_symbol_kind
  {
    MULIB_GLR_NONTERMINAL = 0,
    MULIB_GLR_TERMINAL = 1
  } mulib_glr_symbol_kind_t;
  /** Copyable grammar symbol description; ranges apply only to terminals. */
  typedef struct mulib_glr_symbol
  {
    mulib_glr_symbol_kind_t kind;
    uint32_t lower, upper;
  } mulib_glr_symbol_t;
  /** Hard entry/work limits. Every zero field selects its documented default.
   */
  typedef struct mulib_glr_limits
  {
    size_t max_symbols, max_productions, max_rhs_symbols;
    size_t max_lr_states, max_lr_edges;
    size_t max_gss_nodes, max_gss_edges;
    size_t max_sppf_nodes, max_alternatives, max_children;
    size_t max_tokens, max_steps;
  } mulib_glr_limits_t;
  /** Owning grammar and compiled LR machine. */
  typedef struct mulib_glr mulib_glr_t;
  /**
   * Owning parse artifacts. root is NONE on rejection. position is the number
   * of tokens consumed before rejection, or the full length on success.
   * Partial forest/GSS artifacts are retained on SYNTAX, but not on negative
   * errors.
   */
  typedef struct mulib_glr_result
  {
    mulib_sppf_t *forest;
    mulib_gss_t *gss;
    size_t root, position;
  } mulib_glr_result_t;

  /** Fill limits with defaults; NULL is ignored. */
  void mulib_glr_limits_default (mulib_glr_limits_t *limits);
  /** Create an empty grammar. NULL limits selects defaults. */
  mulib_glr_t *mulib_glr_create (const mulib_glr_limits_t *limits);
  /** Destroy grammar and compiled machine, but not independent parse results.
   */
  void mulib_glr_destroy (mulib_glr_t *grammar);
  /** Add a nonterminal, returning its stable symbol ID. out_symbol is
   * required. */
  int mulib_glr_nonterminal (mulib_glr_t *grammar, size_t *out_symbol);
  /** Add a terminal; ranges may overlap and are not implicitly coalesced. */
  int mulib_glr_terminal (mulib_glr_t *grammar, uint32_t lower, uint32_t upper,
                          size_t *out_symbol);
  /**
   * Append a production with nonterminal lhs and already-defined RHS symbols.
   * NULL rhs is allowed for length zero (epsilon). Production IDs are distinct
   * even for identical rules. Successful grammar edits invalidate compilation.
   */
  int mulib_glr_production (mulib_glr_t *grammar, size_t lhs,
                            const size_t *rhs, size_t length,
                            size_t *out_production);
  /** Copy a symbol description. */
  int mulib_glr_symbol (const mulib_glr_t *grammar, size_t symbol,
                        mulib_glr_symbol_t *out);
  /**
   * Borrow a production's RHS. All three outputs are required. The RHS pointer
   * (NULL for epsilon) is invalidated by the next successful production
   * append.
   */
  int mulib_glr_rule (const mulib_glr_t *grammar, size_t production,
                      size_t *lhs, const size_t **rhs, size_t *length);
  /** Number of grammar symbols (zero for NULL). */
  size_t mulib_glr_symbol_count (const mulib_glr_t *grammar);
  /** Number of productions (zero for NULL). */
  size_t mulib_glr_production_count (const mulib_glr_t *grammar);
  /** Number of compiled LR states, zero when uncompiled. */
  size_t mulib_glr_state_count (const mulib_glr_t *grammar);
  /**
   * Compile a nonterminal start symbol. Transactional: an existing compilation
   * survives failure, provided no intervening grammar edit invalidated it.
   */
  int mulib_glr_compile (mulib_glr_t *grammar, size_t start_symbol);
  /** Initialize a fresh result; does not free existing artifacts. */
  void mulib_glr_result_init (mulib_glr_result_t *result);
  /** Free artifacts and reset result. NULL is allowed. */
  void mulib_glr_result_free (mulib_glr_result_t *result);
  /**
   * Parse all tokens with an immutable compiled grammar. out must have been
   * initialized and be empty. Negative errors leave it empty. NULL tokens is
   * allowed only for zero length. Results own their forest and GSS
   * independently of the grammar; symbol/production IDs refer to the grammar's
   * stable IDs.
   */
  int mulib_glr_parse (const mulib_glr_t *grammar, const uint32_t *tokens,
                       size_t length, mulib_glr_result_t *out);
  /** Static human-readable status text; never NULL. */
  const char *mulib_glr_status_string (int status);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_GLR_H */

#if defined(MULIB_IMPL_GLR) && !defined(MULIB_GLR_IMPLEMENTED)
#define MULIB_GLR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_glr_production_
  {
    size_t lhs, offset, length, item;
  } mulib_glr_production_;
  typedef struct mulib_glr_transition_
  {
    size_t symbol, target;
  } mulib_glr_transition_;
  typedef struct mulib_glr_item_
  {
    size_t production, dot;
  } mulib_glr_item_;
  typedef struct mulib_glr_lr_state_
  {
    mulib_bitset_t items;
    mulib_vec_t transitions, reductions;
    int accepting;
  } mulib_glr_lr_state_;
  struct mulib_glr
  {
    mulib_glr_limits_t limits;
    mulib_vec_t symbols, productions, rhs, states;
    size_t item_count, longest_rhs, start;
    int compiled;
  };
  typedef struct mulib_glr_path_frame_
  {
    size_t node, next_edge;
  } mulib_glr_path_frame_;
  typedef struct mulib_glr_context_
  {
    const mulib_glr_t *grammar;
    mulib_glr_result_t *result;
    size_t position, work;
    int changed;
  } mulib_glr_context_;

  static int
  mulib_glr_tick_ (size_t *work, size_t limit)
  {
    if (*work >= limit)
      return 0;
    ++*work;
    return 1;
  }
  static const mulib_glr_symbol_t *
  mulib_glr_symbol_ (const mulib_glr_t *g, size_t i)
  {
    return (const mulib_glr_symbol_t *)(g->symbols.data
                                        + i * sizeof (mulib_glr_symbol_t));
  }
  static const mulib_glr_production_ *
  mulib_glr_production_at_ (const mulib_glr_t *g, size_t i)
  {
    return (
        const mulib_glr_production_ *)(g->productions.data
                                       + i * sizeof (mulib_glr_production_));
  }
  static const mulib_glr_lr_state_ *
  mulib_glr_state_ (const mulib_glr_t *g, size_t i)
  {
    return (const mulib_glr_lr_state_ *)(g->states.data
                                         + i * sizeof (mulib_glr_lr_state_));
  }
  static void
  mulib_glr_states_free_ (mulib_vec_t *states)
  {
    size_t i;
    for (i = 0; i < states->length; ++i)
      {
        mulib_glr_lr_state_ *s
            = (mulib_glr_lr_state_ *)mulib_vec_at (states, i);
        mulib_bitset_free (&s->items);
        mulib_vec_free (&s->transitions);
        mulib_vec_free (&s->reductions);
      }
    mulib_vec_free (states);
  }
  static void
  mulib_glr_invalidate_ (mulib_glr_t *g)
  {
    mulib_glr_states_free_ (&g->states);
    g->compiled = 0;
    g->start = MULIB_GLR_NONE;
  }

  void
  mulib_glr_limits_default (mulib_glr_limits_t *l)
  {
    if (!l)
      return;
    l->max_symbols = 1024;
    l->max_productions = 4096;
    l->max_rhs_symbols = 65536;
    l->max_lr_states = 1024;
    l->max_lr_edges = 65536;
    l->max_gss_nodes = 65536;
    l->max_gss_edges = 262144;
    l->max_sppf_nodes = 65536;
    l->max_alternatives = 262144;
    l->max_children = 1048576;
    l->max_tokens = 65536;
    l->max_steps = 10000000;
  }
  mulib_glr_t *
  mulib_glr_create (const mulib_glr_limits_t *limits)
  {
    mulib_glr_t *g = (mulib_glr_t *)malloc (sizeof (*g));
    mulib_glr_limits_t defaults;
    if (!g)
      return NULL;
    mulib_glr_limits_default (&defaults);
    g->limits = limits ? *limits : defaults;
#define MULIB_GLR_DEFAULT_(field)                                             \
  if (!g->limits.field)                                                       \
  g->limits.field = defaults.field
    MULIB_GLR_DEFAULT_ (max_symbols);
    MULIB_GLR_DEFAULT_ (max_productions);
    MULIB_GLR_DEFAULT_ (max_rhs_symbols);
    MULIB_GLR_DEFAULT_ (max_lr_states);
    MULIB_GLR_DEFAULT_ (max_lr_edges);
    MULIB_GLR_DEFAULT_ (max_gss_nodes);
    MULIB_GLR_DEFAULT_ (max_gss_edges);
    MULIB_GLR_DEFAULT_ (max_sppf_nodes);
    MULIB_GLR_DEFAULT_ (max_alternatives);
    MULIB_GLR_DEFAULT_ (max_children);
    MULIB_GLR_DEFAULT_ (max_tokens);
    MULIB_GLR_DEFAULT_ (max_steps);
#undef MULIB_GLR_DEFAULT_
    if (g->limits.max_symbols > UINT32_MAX)
      g->limits.max_symbols = UINT32_MAX;
    mulib_vec_init (&g->symbols, sizeof (mulib_glr_symbol_t));
    mulib_vec_init (&g->productions, sizeof (mulib_glr_production_));
    mulib_vec_init (&g->rhs, sizeof (size_t));
    mulib_vec_init (&g->states, sizeof (mulib_glr_lr_state_));
    g->item_count = 0;
    g->longest_rhs = 0;
    g->start = MULIB_GLR_NONE;
    g->compiled = 0;
    return g;
  }
  void
  mulib_glr_destroy (mulib_glr_t *g)
  {
    if (!g)
      return;
    mulib_glr_states_free_ (&g->states);
    mulib_vec_free (&g->symbols);
    mulib_vec_free (&g->productions);
    mulib_vec_free (&g->rhs);
    free (g);
  }
  static int
  mulib_glr_add_symbol_ (mulib_glr_t *g, mulib_glr_symbol_kind_t kind,
                         uint32_t lower, uint32_t upper, size_t *out)
  {
    mulib_glr_symbol_t s;
    if (!g || !out || lower > upper)
      return MULIB_GLR_INVALID;
    if (g->symbols.length >= g->limits.max_symbols
        || g->symbols.length >= SIZE_MAX / sizeof (s))
      return MULIB_GLR_LIMIT;
    s.kind = kind;
    s.lower = lower;
    s.upper = upper;
    if (!mulib_vec_push (&g->symbols, &s))
      return MULIB_GLR_NOMEM;
    *out = g->symbols.length - 1;
    mulib_glr_invalidate_ (g);
    return MULIB_GLR_OK;
  }
  int
  mulib_glr_nonterminal (mulib_glr_t *g, size_t *out)
  {
    return mulib_glr_add_symbol_ (g, MULIB_GLR_NONTERMINAL, 0, 0, out);
  }
  int
  mulib_glr_terminal (mulib_glr_t *g, uint32_t lower, uint32_t upper,
                      size_t *out)
  {
    return mulib_glr_add_symbol_ (g, MULIB_GLR_TERMINAL, lower, upper, out);
  }
  int
  mulib_glr_production (mulib_glr_t *g, size_t lhs, const size_t *rhs,
                        size_t length, size_t *out)
  {
    mulib_glr_production_ p;
    size_t *copy = NULL;
    size_t i, total;
    if (!g || !out || lhs >= g->symbols.length
        || mulib_glr_symbol_ (g, lhs)->kind != MULIB_GLR_NONTERMINAL
        || (length && !rhs))
      return MULIB_GLR_INVALID;
    if (length > g->limits.max_rhs_symbols - g->rhs.length
        || length > SIZE_MAX / sizeof (size_t) || length == SIZE_MAX
        || g->item_count > SIZE_MAX - length - 1
        || g->productions.length >= g->limits.max_productions
        || g->productions.length >= SIZE_MAX / sizeof (p))
      return MULIB_GLR_LIMIT;
    total = g->rhs.length + length;
    if (total < g->rhs.length || total > SIZE_MAX / sizeof (size_t))
      return MULIB_GLR_LIMIT;
    for (i = 0; i < length; ++i)
      if (rhs[i] >= g->symbols.length)
        return MULIB_GLR_INVALID;
    /* The input may be an RHS borrowed from this grammar. Copy before growth.
     */
    if (length)
      {
        copy = (size_t *)malloc (length * sizeof (size_t));
        if (!copy)
          return MULIB_GLR_NOMEM;
        memcpy (copy, rhs, length * sizeof (size_t));
      }
    /* Reserve RHS last: a failed append must not invalidate borrowed RHSs. */
    if (!mulib_vec_reserve (&g->productions, g->productions.length + 1)
        || !mulib_vec_reserve (&g->rhs, total))
      {
        free (copy);
        return MULIB_GLR_NOMEM;
      }
    p.lhs = lhs;
    p.offset = g->rhs.length;
    p.length = length;
    p.item = g->item_count;
    if (length)
      memcpy (g->rhs.data + p.offset * sizeof (size_t), copy,
              length * sizeof (size_t));
    free (copy);
    g->rhs.length = total;
    (void)mulib_vec_push (&g->productions, &p);
    g->item_count += length + 1;
    if (length > g->longest_rhs)
      g->longest_rhs = length;
    *out = g->productions.length - 1;
    mulib_glr_invalidate_ (g);
    return MULIB_GLR_OK;
  }
  int
  mulib_glr_symbol (const mulib_glr_t *g, size_t symbol,
                    mulib_glr_symbol_t *out)
  {
    if (!g || !out || symbol >= g->symbols.length)
      return MULIB_GLR_INVALID;
    *out = *mulib_glr_symbol_ (g, symbol);
    return MULIB_GLR_OK;
  }
  int
  mulib_glr_rule (const mulib_glr_t *g, size_t production, size_t *lhs,
                  const size_t **rhs, size_t *length)
  {
    const mulib_glr_production_ *p;
    if (!g || !lhs || !rhs || !length || production >= g->productions.length)
      return MULIB_GLR_INVALID;
    p = mulib_glr_production_at_ (g, production);
    *lhs = p->lhs;
    *length = p->length;
    *rhs = p->length ? (const size_t *)g->rhs.data + p->offset : NULL;
    return MULIB_GLR_OK;
  }
  size_t
  mulib_glr_symbol_count (const mulib_glr_t *g)
  {
    return g ? g->symbols.length : 0;
  }
  size_t
  mulib_glr_production_count (const mulib_glr_t *g)
  {
    return g ? g->productions.length : 0;
  }
  size_t
  mulib_glr_state_count (const mulib_glr_t *g)
  {
    return g && g->compiled ? g->states.length : 0;
  }

  static int
  mulib_glr_lr_add_ (const mulib_glr_t *g, mulib_vec_t *states,
                     const mulib_bitset_t *set, const mulib_glr_item_ *items,
                     size_t *work, size_t *out)
  {
    mulib_glr_lr_state_ state;
    size_t i;
    int status = MULIB_GLR_NOMEM;
    if (states->length >= g->limits.max_lr_states
        || states->length >= SIZE_MAX / sizeof (state))
      return MULIB_GLR_LIMIT;
    mulib_vec_init (&state.transitions, sizeof (mulib_glr_transition_));
    mulib_vec_init (&state.reductions, sizeof (size_t));
    state.accepting = 0;
    if (!mulib_bitset_init (&state.items, set->bits))
      return MULIB_GLR_NOMEM;
    memcpy (state.items.bytes, set->bytes, (set->bits + 7) / 8);
    for (i = 0; i < set->bits; ++i)
      {
        size_t production;
        if (!mulib_glr_tick_ (work, g->limits.max_steps))
          {
            status = MULIB_GLR_LIMIT;
            goto fail;
          }
        if (!mulib_bitset_test (set, i))
          continue;
        production = items[i].production;
        if (production == MULIB_GLR_NONE)
          continue;
        if (production == g->productions.length)
          {
            if (items[i].dot == 1)
              state.accepting = 1;
          }
        else if (items[i].dot
                 == mulib_glr_production_at_ (g, production)->length)
          {
            if (!mulib_vec_push (&state.reductions, &production))
              goto fail;
          }
      }
    if (!mulib_vec_push (states, &state))
      goto fail;
    *out = states->length - 1;
    return MULIB_GLR_OK;
  fail:
    mulib_bitset_free (&state.items);
    mulib_vec_free (&state.transitions);
    mulib_vec_free (&state.reductions);
    return status;
  }

  int
  mulib_glr_compile (mulib_glr_t *g, size_t start)
  {
    mulib_atn_t *atn = NULL;
    mulib_vec_t states, items;
    mulib_bitset_t scratch;
    size_t count, edges, i, j, k, id, work = 0, used, edge_count = 0;
    int status = MULIB_GLR_OK, scratch_ready = 0;
    if (!g || start >= g->symbols.length
        || mulib_glr_symbol_ (g, start)->kind != MULIB_GLR_NONTERMINAL)
      return MULIB_GLR_INVALID;
    if (g->symbols.length > SIZE_MAX - 2
        || g->item_count > SIZE_MAX - 2 - g->symbols.length
        || g->productions.length > SIZE_MAX - 3
        || g->rhs.length > (SIZE_MAX - 3 - g->productions.length) / 2)
      return MULIB_GLR_LIMIT;
    count = g->item_count + 2 + g->symbols.length;
    edges = 2 * g->rhs.length + g->productions.length + 3;
    if (count > SIZE_MAX - 7 || count > SIZE_MAX / sizeof (mulib_glr_item_))
      return MULIB_GLR_LIMIT;
    mulib_vec_init (&states, sizeof (mulib_glr_lr_state_));
    mulib_vec_init (&items, sizeof (mulib_glr_item_));
    atn = mulib_atn_create (count, edges);
    if (!atn || !mulib_vec_reserve (&items, count))
      {
        status = MULIB_GLR_NOMEM;
        goto done;
      }
    for (i = 0; i < count; ++i)
      {
        mulib_glr_item_ item;
        if (!mulib_glr_tick_ (&work, g->limits.max_steps))
          {
            status = MULIB_GLR_LIMIT;
            goto done;
          }
        status = mulib_atn_add_state (atn, 0, &id);
        if (status)
          goto done;
        item.production = MULIB_GLR_NONE;
        item.dot = 0;
        (void)mulib_vec_push (&items, &item);
      }
    for (i = 0; i < g->productions.length; ++i)
      {
        const mulib_glr_production_ *p = mulib_glr_production_at_ (g, i);
        for (j = 0; j <= p->length; ++j)
          {
            mulib_glr_item_ *item
                = (mulib_glr_item_ *)mulib_vec_at (&items, p->item + j);
            if (!mulib_glr_tick_ (&work, g->limits.max_steps))
              {
                status = MULIB_GLR_LIMIT;
                goto done;
              }
            item->production = i;
            item->dot = j;
            if (j < p->length)
              {
                size_t symbol = ((const size_t *)g->rhs.data)[p->offset + j];
                status = mulib_atn_add_range (
                    atn, p->item + j, p->item + j + 1, (uint32_t)symbol,
                    (uint32_t)symbol, &id);
                if (status)
                  goto done;
                if (mulib_glr_symbol_ (g, symbol)->kind
                    == MULIB_GLR_NONTERMINAL)
                  {
                    status = mulib_atn_add_epsilon (
                        atn, p->item + j, g->item_count + 2 + symbol, &id);
                    if (status)
                      goto done;
                  }
              }
          }
        status = mulib_atn_add_epsilon (atn, g->item_count + 2 + p->lhs,
                                        p->item, &id);
        if (status)
          goto done;
      }
    for (i = 0; i < 2; ++i)
      {
        mulib_glr_item_ *item
            = (mulib_glr_item_ *)mulib_vec_at (&items, g->item_count + i);
        item->production = g->productions.length;
        item->dot = i;
      }
    status = mulib_atn_add_range (atn, g->item_count, g->item_count + 1,
                                  (uint32_t)start, (uint32_t)start, &id);
    if (status)
      goto done;
    status = mulib_atn_add_epsilon (atn, g->item_count,
                                    g->item_count + 2 + start, &id);
    if (status)
      goto done;
    if (!mulib_bitset_init (&scratch, count))
      {
        status = MULIB_GLR_NOMEM;
        goto done;
      }
    scratch_ready = 1;
    (void)mulib_bitset_set (&scratch, g->item_count, 1);
    if (work >= g->limits.max_steps)
      {
        status = MULIB_GLR_LIMIT;
        goto done;
      }
    status
        = mulib_atn_closure (atn, &scratch, g->limits.max_steps - work, &used);
    work += used;
    if (status)
      goto done;
    status = mulib_glr_lr_add_ (
        g, &states, &scratch, (const mulib_glr_item_ *)items.data, &work, &id);
    if (status)
      goto done;
    for (i = 0; i < states.length; ++i)
      for (j = 0; j < g->symbols.length; ++j)
        {
          mulib_glr_lr_state_ *source;
          mulib_glr_transition_ transition;
          if (!mulib_glr_tick_ (&work, g->limits.max_steps))
            {
              status = MULIB_GLR_LIMIT;
              goto done;
            }
          source = (mulib_glr_lr_state_ *)mulib_vec_at (&states, i);
          if (work >= g->limits.max_steps)
            {
              status = MULIB_GLR_LIMIT;
              goto done;
            }
          status = mulib_atn_step (atn, &source->items, (uint32_t)j, &scratch,
                                   g->limits.max_steps - work, &used);
          work += used;
          if (status)
            goto done;
          if (!mulib_bitset_count (&scratch))
            continue;
          for (k = 0; k < states.length; ++k)
            {
              const mulib_glr_lr_state_ *candidate
                  = (const mulib_glr_lr_state_ *)(states.data
                                                  + k * sizeof (*candidate));
              if (!mulib_glr_tick_ (&work, g->limits.max_steps))
                {
                  status = MULIB_GLR_LIMIT;
                  goto done;
                }
              if (!memcmp (candidate->items.bytes, scratch.bytes,
                           (count + 7) / 8))
                break;
            }
          if (k == states.length)
            {
              status = mulib_glr_lr_add_ (g, &states, &scratch,
                                          (const mulib_glr_item_ *)items.data,
                                          &work, &k);
              if (status)
                goto done;
            }
          if (edge_count >= g->limits.max_lr_edges)
            {
              status = MULIB_GLR_LIMIT;
              goto done;
            }
          transition.symbol = j;
          transition.target = k;
          source = (mulib_glr_lr_state_ *)mulib_vec_at (&states, i);
          if (!mulib_vec_push (&source->transitions, &transition))
            {
              status = MULIB_GLR_NOMEM;
              goto done;
            }
          ++edge_count;
        }
    mulib_glr_invalidate_ (g);
    g->states = states;
    mulib_vec_init (&states, sizeof (mulib_glr_lr_state_));
    g->compiled = 1;
    g->start = start;
  done:
    if (scratch_ready)
      mulib_bitset_free (&scratch);
    mulib_atn_destroy (atn);
    mulib_vec_free (&items);
    mulib_glr_states_free_ (&states);
    return status;
  }

  void
  mulib_glr_result_init (mulib_glr_result_t *r)
  {
    if (r)
      {
        r->forest = NULL;
        r->gss = NULL;
        r->root = MULIB_GLR_NONE;
        r->position = 0;
      }
  }
  void
  mulib_glr_result_free (mulib_glr_result_t *r)
  {
    if (!r)
      return;
    mulib_sppf_destroy (r->forest);
    mulib_gss_destroy (r->gss);
    mulib_glr_result_init (r);
  }

  static int
  mulib_glr_reduce_ (mulib_glr_context_ *c, size_t predecessor,
                     size_t production, const size_t *children)
  {
    const mulib_glr_t *g = c->grammar;
    const mulib_glr_production_ *p = mulib_glr_production_at_ (g, production);
    const mulib_glr_lr_state_ *state;
    mulib_gss_node_t pred;
    size_t i, parent, alternative, top, edge, target = MULIB_GLR_NONE;
    int status, created;
    if (!mulib_glr_tick_ (&c->work, g->limits.max_steps))
      return MULIB_GLR_LIMIT;
    if (mulib_gss_node (c->result->gss, predecessor, &pred) != MULIB_GSS_OK
        || pred.state >= g->states.length)
      return MULIB_GLR_INVALID;
    state = mulib_glr_state_ (g, pred.state);
    for (i = 0; i < state->transitions.length; ++i)
      {
        const mulib_glr_transition_ *t
            = (const mulib_glr_transition_ *)(state->transitions.data
                                              + i * sizeof (*t));
        if (!mulib_glr_tick_ (&c->work, g->limits.max_steps))
          return MULIB_GLR_LIMIT;
        if (t->symbol == p->lhs)
          {
            target = t->target;
            break;
          }
      }
    /* A canonical LR path for this rule always has the corresponding goto. */
    if (target == MULIB_GLR_NONE)
      return MULIB_GLR_INVALID;
    status
        = mulib_sppf_intern (c->result->forest, MULIB_SPPF_NONTERMINAL, p->lhs,
                             pred.position, c->position, &parent, NULL);
    if (status)
      return status;
    status = mulib_sppf_pack (c->result->forest, parent, production, children,
                              p->length, &alternative, NULL);
    if (status)
      return status;
    status
        = mulib_gss_intern (c->result->gss, target, c->position, &top, NULL);
    if (status)
      return status;
    status = mulib_gss_link (c->result->gss, top, predecessor, parent, &edge,
                             &created);
    if (!status && created)
      c->changed = 1;
    return status;
  }

  static int
  mulib_glr_paths_ (mulib_glr_context_ *c, size_t head, size_t production,
                    mulib_glr_path_frame_ *frames, size_t *children)
  {
    const mulib_glr_t *g = c->grammar;
    const mulib_glr_production_ *p = mulib_glr_production_at_ (g, production);
    size_t depth = 0;
    mulib_gss_node_t node;
    int status;
    if (!p->length)
      return mulib_glr_reduce_ (c, head, production, NULL);
    if (mulib_gss_node (c->result->gss, head, &node) != MULIB_GSS_OK)
      return MULIB_GLR_INVALID;
    frames[0].node = head;
    frames[0].next_edge = node.first_edge;
    for (;;)
      {
        size_t expected;
        mulib_gss_edge_t edge;
        mulib_sppf_node_t label;
        if (!mulib_glr_tick_ (&c->work, g->limits.max_steps))
          return MULIB_GLR_LIMIT;
        if (depth == p->length)
          {
            status = mulib_glr_reduce_ (c, frames[depth].node, production,
                                        children);
            if (status)
              return status;
            --depth;
            continue;
          }
        if (frames[depth].next_edge == MULIB_GSS_NONE)
          {
            if (!depth)
              break;
            --depth;
            continue;
          }
        if (mulib_gss_edge (c->result->gss, frames[depth].next_edge, &edge)
            != MULIB_GSS_OK)
          return MULIB_GLR_INVALID;
        frames[depth].next_edge = edge.next;
        expected
            = ((const size_t *)g->rhs.data)[p->offset + p->length - depth - 1];
        if (mulib_sppf_node (c->result->forest, edge.label, &label)
            != MULIB_SPPF_OK)
          return MULIB_GLR_INVALID;
        if (label.symbol != expected)
          continue;
        children[p->length - depth - 1] = edge.label;
        ++depth;
        if (mulib_gss_node (c->result->gss, edge.predecessor, &node)
            != MULIB_GSS_OK)
          return MULIB_GLR_INVALID;
        frames[depth].node = edge.predecessor;
        frames[depth].next_edge = node.first_edge;
      }
    return MULIB_GLR_OK;
  }

  static int
  mulib_glr_saturate_ (mulib_glr_context_ *c, mulib_glr_path_frame_ *frames,
                       size_t *children)
  {
    const mulib_glr_t *g = c->grammar;
    size_t i, j;
    int status;
    do
      {
        c->changed = 0;
        for (i = 0; i < mulib_gss_node_count (c->result->gss); ++i)
          {
            mulib_gss_node_t node;
            const mulib_glr_lr_state_ *state;
            if (!mulib_glr_tick_ (&c->work, g->limits.max_steps))
              return MULIB_GLR_LIMIT;
            if (mulib_gss_node (c->result->gss, i, &node) != MULIB_GSS_OK
                || node.state >= g->states.length)
              return MULIB_GLR_INVALID;
            if (node.position != c->position)
              continue;
            state = mulib_glr_state_ (g, node.state);
            for (j = 0; j < state->reductions.length; ++j)
              {
                size_t production
                    = ((const size_t *)state->reductions.data)[j];
                if (!mulib_glr_tick_ (&c->work, g->limits.max_steps))
                  return MULIB_GLR_LIMIT;
                status = mulib_glr_paths_ (c, i, production, frames, children);
                if (status)
                  return status;
              }
          }
      }
    while (c->changed);
    return MULIB_GLR_OK;
  }

  static int
  mulib_glr_accept_ (mulib_glr_context_ *c)
  {
    const mulib_glr_t *g = c->grammar;
    size_t i, e;
    for (i = 0; i < mulib_gss_node_count (c->result->gss); ++i)
      {
        mulib_gss_node_t node;
        if (!mulib_glr_tick_ (&c->work, g->limits.max_steps))
          return MULIB_GLR_LIMIT;
        if (mulib_gss_node (c->result->gss, i, &node) != MULIB_GSS_OK
            || node.state >= g->states.length)
          return MULIB_GLR_INVALID;
        if (node.position != c->position
            || !mulib_glr_state_ (g, node.state)->accepting)
          continue;
        for (e = node.first_edge; e != MULIB_GSS_NONE;)
          {
            mulib_gss_edge_t edge;
            mulib_sppf_node_t label;
            if (!mulib_glr_tick_ (&c->work, g->limits.max_steps))
              return MULIB_GLR_LIMIT;
            if (mulib_gss_edge (c->result->gss, e, &edge) != MULIB_GSS_OK
                || mulib_sppf_node (c->result->forest, edge.label, &label)
                       != MULIB_SPPF_OK)
              return MULIB_GLR_INVALID;
            if (edge.predecessor == 0 && label.symbol == g->start
                && label.kind == MULIB_SPPF_NONTERMINAL && !label.begin
                && label.end == c->position)
              {
                c->result->root = edge.label;
                return MULIB_GLR_OK;
              }
            e = edge.next;
          }
      }
    return MULIB_GLR_SYNTAX;
  }

  static int
  mulib_glr_shift_ (mulib_glr_context_ *c, uint32_t token)
  {
    const mulib_glr_t *g = c->grammar;
    size_t i, j;
    int shifted = 0, status;
    for (i = 0; i < mulib_gss_node_count (c->result->gss); ++i)
      {
        mulib_gss_node_t node;
        const mulib_glr_lr_state_ *state;
        if (!mulib_glr_tick_ (&c->work, g->limits.max_steps))
          return MULIB_GLR_LIMIT;
        if (mulib_gss_node (c->result->gss, i, &node) != MULIB_GSS_OK
            || node.state >= g->states.length)
          return MULIB_GLR_INVALID;
        if (node.position != c->position)
          continue;
        state = mulib_glr_state_ (g, node.state);
        for (j = 0; j < state->transitions.length; ++j)
          {
            const mulib_glr_transition_ *t
                = (const mulib_glr_transition_ *)(state->transitions.data
                                                  + j * sizeof (*t));
            const mulib_glr_symbol_t *symbol
                = mulib_glr_symbol_ (g, t->symbol);
            size_t leaf, top, edge;
            if (!mulib_glr_tick_ (&c->work, g->limits.max_steps))
              return MULIB_GLR_LIMIT;
            if (symbol->kind != MULIB_GLR_TERMINAL || token < symbol->lower
                || token > symbol->upper)
              continue;
            status = mulib_sppf_intern (c->result->forest, MULIB_SPPF_TERMINAL,
                                        t->symbol, c->position,
                                        c->position + 1, &leaf, NULL);
            if (status)
              return status;
            status = mulib_gss_intern (c->result->gss, t->target,
                                       c->position + 1, &top, NULL);
            if (status)
              return status;
            status
                = mulib_gss_link (c->result->gss, top, i, leaf, &edge, NULL);
            if (status)
              return status;
            shifted = 1;
          }
      }
    return shifted ? MULIB_GLR_OK : MULIB_GLR_SYNTAX;
  }

  int
  mulib_glr_parse (const mulib_glr_t *g, const uint32_t *tokens, size_t length,
                   mulib_glr_result_t *out)
  {
    mulib_glr_result_t result;
    mulib_glr_context_ context;
    mulib_glr_path_frame_ *frames = NULL;
    size_t *children = NULL, id;
    int status;
    if (!g || !out || (length && !tokens) || out->forest || out->gss
        || out->root != MULIB_GLR_NONE)
      return MULIB_GLR_INVALID;
    if (!g->compiled)
      return MULIB_GLR_UNCOMPILED;
    if (length > g->limits.max_tokens || length == SIZE_MAX
        || g->longest_rhs >= SIZE_MAX / sizeof (*frames)
        || g->longest_rhs > SIZE_MAX / sizeof (*children))
      return MULIB_GLR_LIMIT;
    mulib_glr_result_init (&result);
    result.forest = mulib_sppf_create (g->limits.max_sppf_nodes,
                                       g->limits.max_alternatives,
                                       g->limits.max_children);
    result.gss
        = mulib_gss_create (g->limits.max_gss_nodes, g->limits.max_gss_edges);
    frames = (mulib_glr_path_frame_ *)malloc ((g->longest_rhs + 1)
                                              * sizeof (*frames));
    if (g->longest_rhs)
      children = (size_t *)malloc (g->longest_rhs * sizeof (*children));
    if (!result.forest || !result.gss || !frames
        || (g->longest_rhs && !children))
      {
        status = MULIB_GLR_NOMEM;
        goto done;
      }
    status = mulib_gss_intern (result.gss, 0, 0, &id, NULL);
    if (status)
      goto done;
    context.grammar = g;
    context.result = &result;
    context.position = 0;
    context.work = 0;
    context.changed = 0;
    for (;;)
      {
        status = mulib_glr_saturate_ (&context, frames, children);
        if (status)
          break;
        result.position = context.position;
        if (context.position == length)
          {
            status = mulib_glr_accept_ (&context);
            break;
          }
        status = mulib_glr_shift_ (&context, tokens[context.position]);
        if (status)
          break;
        ++context.position;
      }
    if (status == MULIB_GLR_OK || status == MULIB_GLR_SYNTAX)
      {
        *out = result;
        mulib_glr_result_init (&result);
      }
  done:
    free (frames);
    free (children);
    mulib_glr_result_free (&result);
    return status;
  }

  const char *
  mulib_glr_status_string (int status)
  {
    switch (status)
      {
      case MULIB_GLR_OK:
        return "accepted / success";
      case MULIB_GLR_SYNTAX:
        return "input does not match the grammar";
      case MULIB_GLR_INVALID:
        return "invalid argument or inconsistent object";
      case MULIB_GLR_NOMEM:
        return "allocation failed";
      case MULIB_GLR_LIMIT:
        return "resource or work limit exceeded";
      case MULIB_GLR_UNCOMPILED:
        return "grammar must be compiled after its last edit";
      default:
        return "unknown GLR status";
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
