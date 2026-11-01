/**
 * @file mulib-datalog.h
 * @brief Bounded finite positive Datalog over copied string constants.
 * Define MULIB_IMPL_DATALOG once. No module dependencies, negation or
 * functors.
 */
#ifndef MULIB_DATALOG_H
#define MULIB_DATALOG_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_datalog mulib_datalog_t;
  typedef enum mulib_datalog_status
  {
    MULIB_DATALOG_OK,
    MULIB_DATALOG_INVALID,
    MULIB_DATALOG_UNSAFE,
    MULIB_DATALOG_LIMIT,
    MULIB_DATALOG_MEMORY,
    MULIB_DATALOG_STOPPED
  } mulib_datalog_status_t;
  /** @brief Nonzero variable means a named variable; otherwise a string
   * constant. */
  typedef struct mulib_datalog_term
  {
    const char *name;
    int variable;
  } mulib_datalog_term_t;
  /** @brief Borrowed input atom. Names are case-sensitive NUL-terminated
   * strings. */
  typedef struct mulib_datalog_atom
  {
    const char *predicate;
    const mulib_datalog_term_t *terms;
    size_t arity;
  } mulib_datalog_atom_t;
  /** @brief All fields are hard limits, zero is zero. Arity<=8, body<=16,
   * variables<=32. */
  typedef struct mulib_datalog_limits
  {
    size_t facts, rules, symbols, symbol_bytes, arity, body, variables, steps,
        rounds;
  } mulib_datalog_limits_t;
  /** @brief Return defaults: 4096 facts, 256 rules, 4096 symbols, 64 KiB
   * strings, arity 8, body 16, variables 32, 1M work steps, 256 fixed-point
   * rounds. */
  mulib_datalog_limits_t mulib_datalog_default_limits (void);
  /** @brief Allocate a database; copies limits. NULL selects defaults. */
  mulib_datalog_t *mulib_datalog_create (const mulib_datalog_limits_t *limits,
                                         mulib_datalog_status_t *status);
  /** @brief Release database and all copied names. NULL is harmless. */
  void mulib_datalog_destroy (mulib_datalog_t *database);
  /** @brief Copy a ground fact. Duplicate facts succeed without consuming a
   * slot. */
  mulib_datalog_status_t mulib_datalog_fact (mulib_datalog_t *database,
                                             const mulib_datalog_atom_t *fact);
  /** @brief Copy a positive rule; all head variables must occur in the
   * nonempty body. */
  mulib_datalog_status_t mulib_datalog_rule (mulib_datalog_t *database,
                                             const mulib_datalog_atom_t *head,
                                             const mulib_datalog_atom_t *body,
                                             size_t body_count);
  /** @brief Compute least fixed point; on failure roll back facts derived by
   * this call. */
  mulib_datalog_status_t mulib_datalog_saturate (mulib_datalog_t *database);
  /** @brief Borrowed ground tuple callback. Nonzero stops; no database
   * mutation/reentry. */
  typedef int (*mulib_datalog_query_fn) (void *user, const char *const *tuple,
                                         size_t arity);
  /** @brief Match current facts (call saturate first); repeated variables
   * unify. Unknown predicates/constants yield zero matches, without interning
   * them. count includes the row that stopped the callback. A NULL callback
   * just counts.
   */
  mulib_datalog_status_t mulib_datalog_query (
      const mulib_datalog_t *database, const mulib_datalog_atom_t *pattern,
      mulib_datalog_query_fn callback, void *user, size_t *count);
  /** @brief Number of distinct stored ground facts, including derived facts.
   */
  size_t mulib_datalog_fact_count (const mulib_datalog_t *database);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_DATALOG) && !defined(MULIB_DATALOG_IMPLEMENTED)
#define MULIB_DATALOG_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_datalog_atom_
  {
    size_t predicate, arity, terms[8];
    unsigned variables;
  } mulib_datalog_atom_;
  typedef struct mulib_datalog_rule_
  {
    mulib_datalog_atom_ head, body[16];
    size_t count;
  } mulib_datalog_rule_;
  struct mulib_datalog
  {
    mulib_datalog_limits_t limits;
    mulib_datalog_atom_ *facts;
    mulib_datalog_rule_ *rules;
    char **symbols;
    size_t fact_count, rule_count, symbol_count, symbol_bytes;
  };
  mulib_datalog_limits_t
  mulib_datalog_default_limits (void)
  {
    mulib_datalog_limits_t l
        = { 4096, 256, 4096, 65536, 8, 16, 32, 1000000, 256 };
    return l;
  }
  mulib_datalog_t *
  mulib_datalog_create (const mulib_datalog_limits_t *requested,
                        mulib_datalog_status_t *status)
  {
    mulib_datalog_limits_t l
        = requested ? *requested : mulib_datalog_default_limits ();
    mulib_datalog_t *d = NULL;
    mulib_datalog_status_t rc = MULIB_DATALOG_LIMIT;
    if (l.arity > 8 || l.body > 16 || l.variables > 32
        || l.facts > SIZE_MAX / sizeof (mulib_datalog_atom_)
        || l.rules > SIZE_MAX / sizeof (mulib_datalog_rule_)
        || l.symbols > SIZE_MAX / sizeof (char *))
      goto done;
    rc = MULIB_DATALOG_MEMORY;
    d = (mulib_datalog_t *)calloc (1, sizeof *d);
    if (!d)
      goto done;
    d->limits = l;
    if (l.facts)
      d->facts = (mulib_datalog_atom_ *)malloc (l.facts * sizeof *d->facts);
    if (l.rules)
      d->rules = (mulib_datalog_rule_ *)malloc (l.rules * sizeof *d->rules);
    if (l.symbols)
      d->symbols = (char **)malloc (l.symbols * sizeof *d->symbols);
    if ((l.facts && !d->facts) || (l.rules && !d->rules)
        || (l.symbols && !d->symbols))
      {
        mulib_datalog_destroy (d);
        d = NULL;
        goto done;
      }
    rc = MULIB_DATALOG_OK;
  done:
    if (status)
      *status = rc;
    return d;
  }
  void
  mulib_datalog_destroy (mulib_datalog_t *d)
  {
    size_t i;
    if (!d)
      return;
    for (i = 0; i < d->symbol_count; ++i)
      free (d->symbols[i]);
    free (d->symbols);
    free (d->facts);
    free (d->rules);
    free (d);
  }
  static size_t
  mulib_datalog_find_ (const mulib_datalog_t *d, const char *name)
  {
    size_t i;
    for (i = 0; i < d->symbol_count; ++i)
      if (!strcmp (d->symbols[i], name))
        return i;
    return SIZE_MAX;
  }
  static mulib_datalog_status_t
  mulib_datalog_intern_ (mulib_datalog_t *d, const char *s, size_t *id)
  {
    size_t n;
    char *p;
    *id = mulib_datalog_find_ (d, s);
    if (*id != SIZE_MAX)
      return MULIB_DATALOG_OK;
    n = strlen (s);
    if (d->symbol_count == d->limits.symbols || n == SIZE_MAX
        || n + 1 > d->limits.symbol_bytes - d->symbol_bytes)
      return MULIB_DATALOG_LIMIT;
    p = (char *)malloc (n + 1);
    if (!p)
      return MULIB_DATALOG_MEMORY;
    memcpy (p, s, n + 1);
    *id = d->symbol_count;
    d->symbols[d->symbol_count++] = p;
    d->symbol_bytes += n + 1;
    return MULIB_DATALOG_OK;
  }
  static int
  mulib_datalog_input_ (const mulib_datalog_t *d,
                        const mulib_datalog_atom_t *a)
  {
    size_t i;
    if (!a || !a->predicate || !*a->predicate || (a->arity && !a->terms))
      return 0;
    if (a->arity > d->limits.arity)
      return 0;
    for (i = 0; i < a->arity; ++i)
      if (!a->terms[i].name || (a->terms[i].variable && !*a->terms[i].name))
        return 0;
    return 1;
  }
  static mulib_datalog_status_t
  mulib_datalog_atom_copy_ (mulib_datalog_t *d, const mulib_datalog_atom_t *a,
                            mulib_datalog_atom_ *out, const char **vars,
                            size_t *var_count)
  {
    size_t i, j;
    mulib_datalog_status_t rc;
    memset (out, 0, sizeof *out);
    out->arity = a->arity;
    rc = mulib_datalog_intern_ (d, a->predicate, &out->predicate);
    if (rc)
      return rc;
    for (i = 0; i < a->arity; ++i)
      {
        if (a->terms[i].variable)
          {
            if (!vars)
              return MULIB_DATALOG_INVALID;
            for (j = 0; j < *var_count; ++j)
              if (!strcmp (vars[j], a->terms[i].name))
                break;
            if (j == *var_count)
              {
                if (j == d->limits.variables)
                  return MULIB_DATALOG_LIMIT;
                vars[(*var_count)++] = a->terms[i].name;
              }
            out->variables |= 1u << i;
            out->terms[i] = j;
          }
        else
          {
            rc = mulib_datalog_intern_ (d, a->terms[i].name, &out->terms[i]);
            if (rc)
              return rc;
          }
      }
    return MULIB_DATALOG_OK;
  }
  static void
  mulib_datalog_rollback_symbols_ (mulib_datalog_t *d, size_t count)
  {
    while (d->symbol_count > count)
      {
        char *s = d->symbols[--d->symbol_count];
        d->symbol_bytes -= strlen (s) + 1;
        free (s);
      }
  }
  static int
  mulib_datalog_equal_ (const mulib_datalog_atom_ *a,
                        const mulib_datalog_atom_ *b)
  {
    return a->predicate == b->predicate && a->arity == b->arity
           && !memcmp (a->terms, b->terms, a->arity * sizeof *a->terms);
  }
  mulib_datalog_status_t
  mulib_datalog_fact (mulib_datalog_t *d, const mulib_datalog_atom_t *a)
  {
    mulib_datalog_atom_ f;
    mulib_datalog_status_t rc;
    size_t i, old;
    if (!d || !mulib_datalog_input_ (d, a))
      return MULIB_DATALOG_INVALID;
    for (i = 0; i < a->arity; ++i)
      if (a->terms[i].variable)
        return MULIB_DATALOG_INVALID;
    old = d->symbol_count;
    rc = mulib_datalog_atom_copy_ (d, a, &f, NULL, NULL);
    if (!rc)
      {
        for (i = 0; i < d->fact_count; ++i)
          if (mulib_datalog_equal_ (&f, &d->facts[i]))
            return MULIB_DATALOG_OK;
        if (d->fact_count == d->limits.facts)
          rc = MULIB_DATALOG_LIMIT;
        else
          d->facts[d->fact_count++] = f;
      }
    if (rc)
      mulib_datalog_rollback_symbols_ (d, old);
    return rc;
  }
  mulib_datalog_status_t
  mulib_datalog_rule (mulib_datalog_t *d, const mulib_datalog_atom_t *head,
                      const mulib_datalog_atom_t *body, size_t count)
  {
    mulib_datalog_rule_ r;
    const char *vars[32];
    size_t n = 0, body_vars, old, i;
    mulib_datalog_status_t rc = MULIB_DATALOG_OK;
    if (!d || !count || !body || !mulib_datalog_input_ (d, head))
      return MULIB_DATALOG_INVALID;
    if (count > d->limits.body || d->rule_count == d->limits.rules)
      return MULIB_DATALOG_LIMIT;
    for (i = 0; i < count; ++i)
      if (!mulib_datalog_input_ (d, &body[i]))
        return MULIB_DATALOG_INVALID;
    old = d->symbol_count;
    memset (&r, 0, sizeof r);
    r.count = count;
    for (i = 0; i < count; ++i)
      {
        rc = mulib_datalog_atom_copy_ (d, &body[i], &r.body[i], vars, &n);
        if (rc)
          goto fail;
      }
    body_vars = n;
    rc = mulib_datalog_atom_copy_ (d, head, &r.head, vars, &n);
    if (rc)
      goto fail;
    if (n != body_vars)
      {
        rc = MULIB_DATALOG_UNSAFE;
        goto fail;
      }
    d->rules[d->rule_count++] = r;
    return MULIB_DATALOG_OK;
  fail:
    mulib_datalog_rollback_symbols_ (d, old);
    return rc;
  }
  static int
  mulib_datalog_unify_ (const mulib_datalog_atom_ *p,
                        const mulib_datalog_atom_ *f, size_t bind[32])
  {
    size_t i;
    if (p->predicate != f->predicate || p->arity != f->arity)
      return 0;
    for (i = 0; i < p->arity; ++i)
      {
        size_t t = p->terms[i];
        if (p->variables & (1u << i))
          {
            if (bind[t] == SIZE_MAX)
              bind[t] = f->terms[i];
            else if (bind[t] != f->terms[i])
              return 0;
          }
        else if (t != f->terms[i])
          return 0;
      }
    return 1;
  }
  static mulib_datalog_status_t
  mulib_datalog_join_ (mulib_datalog_t *d, const mulib_datalog_rule_ *r,
                       size_t level, size_t snapshot,
                       const size_t bindings[32], size_t *steps)
  {
    size_t i, j, bind[32];
    mulib_datalog_status_t rc;
    if (level == r->count)
      {
        mulib_datalog_atom_ f = r->head;
        for (j = 0; j < f.arity; ++j)
          if (f.variables & (1u << j))
            f.terms[j] = bindings[f.terms[j]];
        f.variables = 0;
        for (i = 0; i < d->fact_count; ++i)
          {
            if (*steps == d->limits.steps)
              return MULIB_DATALOG_LIMIT;
            ++*steps;
            if (mulib_datalog_equal_ (&f, &d->facts[i]))
              return MULIB_DATALOG_OK;
          }
        if (d->fact_count == d->limits.facts)
          return MULIB_DATALOG_LIMIT;
        d->facts[d->fact_count++] = f;
        return MULIB_DATALOG_OK;
      }
    for (i = 0; i < snapshot; ++i)
      {
        if (*steps == d->limits.steps)
          return MULIB_DATALOG_LIMIT;
        ++*steps;
        memcpy (bind, bindings, sizeof bind);
        if (!mulib_datalog_unify_ (&r->body[level], &d->facts[i], bind))
          continue;
        rc = mulib_datalog_join_ (d, r, level + 1, snapshot, bind, steps);
        if (rc)
          return rc;
      }
    return MULIB_DATALOG_OK;
  }
  mulib_datalog_status_t
  mulib_datalog_saturate (mulib_datalog_t *d)
  {
    size_t old, snapshot, round, i, bind[32], steps = 0;
    mulib_datalog_status_t rc = MULIB_DATALOG_LIMIT;
    if (!d)
      return MULIB_DATALOG_INVALID;
    if (!d->rule_count)
      return MULIB_DATALOG_OK;
    old = d->fact_count;
    for (i = 0; i < 32; ++i)
      bind[i] = SIZE_MAX;
    for (round = 0; round < d->limits.rounds; ++round)
      {
        snapshot = d->fact_count;
        for (i = 0; i < d->rule_count; ++i)
          {
            if (steps == d->limits.steps)
              {
                rc = MULIB_DATALOG_LIMIT;
                goto fail;
              }
            ++steps;
            rc = mulib_datalog_join_ (d, &d->rules[i], 0, snapshot, bind,
                                      &steps);
            if (rc)
              goto fail;
          }
        if (snapshot == d->fact_count)
          return MULIB_DATALOG_OK;
      }
    rc = MULIB_DATALOG_LIMIT;
  fail:
    d->fact_count = old;
    return rc;
  }
  mulib_datalog_status_t
  mulib_datalog_query (const mulib_datalog_t *d, const mulib_datalog_atom_t *a,
                       mulib_datalog_query_fn callback, void *user,
                       size_t *count)
  {
    mulib_datalog_atom_ p;
    const char *vars[32], *tuple[8];
    size_t i, j, n = 0, rows = 0, bind[32];
    if (count)
      *count = 0;
    if (!d || !mulib_datalog_input_ (d, a))
      return MULIB_DATALOG_INVALID;
    memset (&p, 0, sizeof p);
    p.arity = a->arity;
    p.predicate = mulib_datalog_find_ (d, a->predicate);
    for (i = 0; i < a->arity; ++i)
      {
        if (a->terms[i].variable)
          {
            for (j = 0; j < n; ++j)
              if (!strcmp (vars[j], a->terms[i].name))
                break;
            if (j == n)
              {
                if (n == d->limits.variables)
                  return MULIB_DATALOG_LIMIT;
                vars[n++] = a->terms[i].name;
              }
            p.variables |= 1u << i;
            p.terms[i] = j;
          }
        else
          p.terms[i] = mulib_datalog_find_ (d, a->terms[i].name);
      }
    for (i = 0; i < d->fact_count; ++i)
      {
        if (i == d->limits.steps)
          return MULIB_DATALOG_LIMIT;
        for (j = 0; j < 32; ++j)
          bind[j] = SIZE_MAX;
        if (!mulib_datalog_unify_ (&p, &d->facts[i], bind))
          continue;
        ++rows;
        if (count)
          *count = rows;
        for (j = 0; j < p.arity; ++j)
          tuple[j] = d->symbols[d->facts[i].terms[j]];
        if (callback && callback (user, tuple, p.arity))
          return MULIB_DATALOG_STOPPED;
      }
    return MULIB_DATALOG_OK;
  }
  size_t
  mulib_datalog_fact_count (const mulib_datalog_t *d)
  {
    return d ? d->fact_count : 0;
  }
#ifdef __cplusplus
}
#endif
#endif
