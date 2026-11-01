/**
 * @file mulib-linker.h
 * @brief Single-image named symbols and checked ABS32/ABS64/PC32 relocations.
 * Define MULIB_IMPL_LINKER and MULIB_IMPL_EMITTER once, explicitly.
 */
#ifndef MULIB_LINKER_H
#define MULIB_LINKER_H
#include "mulib-emitter.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_linker mulib_linker_t;
  typedef enum mulib_linker_kind
  {
    MULIB_LINKER_ABS32,
    MULIB_LINKER_ABS64,
    MULIB_LINKER_PC32
  } mulib_linker_kind;
  typedef enum mulib_linker_result
  {
    MULIB_LINKER_OK = 0,
    MULIB_LINKER_EINVAL = -1,
    MULIB_LINKER_ENOMEM = -2,
    MULIB_LINKER_ELIMIT = -3,
    MULIB_LINKER_EDUPLICATE = -4,
    MULIB_LINKER_EUNDEFINED = -5,
    MULIB_LINKER_ERANGE = -6,
    MULIB_LINKER_EOVERLAP = -7
  } mulib_linker_result;
  /** @brief Allocate an owning, fixed-limit table; NULL means invalid
   * limits/OOM. */
  mulib_linker_t *mulib_linker_create (size_t max_symbols,
                                       size_t max_relocations);
  /** @brief Free all copied names and tables; NULL is harmless. */
  void mulib_linker_destroy (mulib_linker_t *linker);
  /** @brief Define an image-relative offset (absolute=0) or absolute address
   * (1). Names are copied, nonempty, at most 128 printable ASCII bytes. No
   * redefinition.
   */
  int mulib_linker_define (mulib_linker_t *linker, const char *name,
                           uint64_t value, int absolute);
  /** @brief Add a relocation, interning an undefined symbol if necessary.
   * Atomic on failure; overlapping relocation fields are rejected. Formula is
   * S+A for ABS and S+A-P for PC32, where P is the field's address, not its
   * end.
   */
  int mulib_linker_relocate (mulib_linker_t *linker, const char *name,
                             size_t offset, mulib_linker_kind kind,
                             int64_t addend);
  /** @brief Validate every relocation, then patch all or none in image byte
   * order. bad_relocation, if supplied, is SIZE_MAX on success/non-relocation
   * errors. Image-relative definitions may point exactly one byte past the
   * image.
   */
  int mulib_linker_apply (const mulib_linker_t *linker, mulib_emitter_t *image,
                          uint64_t base_address, size_t *bad_relocation);
  /** @brief Number of interned names (including unresolved ones). */
  size_t mulib_linker_symbol_count (const mulib_linker_t *linker);
  /** @brief Number of relocation fields. */
  size_t mulib_linker_relocation_count (const mulib_linker_t *linker);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_LINKER) && !defined(MULIB_LINKER_IMPLEMENTED)
#define MULIB_LINKER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_linker_symbol_
  {
    char *name;
    uint64_t value;
    int defined, absolute;
  } mulib_linker_symbol_;
  typedef struct mulib_linker_reloc_
  {
    size_t symbol, offset;
    mulib_linker_kind kind;
    int64_t addend;
  } mulib_linker_reloc_;
  struct mulib_linker
  {
    mulib_linker_symbol_ *symbols;
    mulib_linker_reloc_ *relocations;
    size_t symbol_count, relocation_count, max_symbols, max_relocations;
  };
  static int
  mulib_linker_name_ (const char *name, size_t *length)
  {
    size_t n = 0;
    if (!name)
      return 0;
    while (name[n])
      {
        unsigned char c = (unsigned char)name[n];
        if (n == 128 || c < 33 || c > 126)
          return 0;
        ++n;
      }
    *length = n;
    return n != 0;
  }
  static size_t
  mulib_linker_find_ (const mulib_linker_t *l, const char *name)
  {
    size_t i;
    for (i = 0; i < l->symbol_count; ++i)
      if (!strcmp (l->symbols[i].name, name))
        return i;
    return SIZE_MAX;
  }
  static int
  mulib_linker_intern_ (mulib_linker_t *l, const char *name, size_t n,
                        size_t *index)
  {
    char *copy;
    *index = mulib_linker_find_ (l, name);
    if (*index != SIZE_MAX)
      return 0;
    if (l->symbol_count == l->max_symbols)
      return MULIB_LINKER_ELIMIT;
    copy = (char *)malloc (n + 1);
    if (!copy)
      return MULIB_LINKER_ENOMEM;
    memcpy (copy, name, n + 1);
    *index = l->symbol_count++;
    l->symbols[*index].name = copy;
    return 0;
  }
  mulib_linker_t *
  mulib_linker_create (size_t ns, size_t nr)
  {
    mulib_linker_t *l;
    if (ns > SIZE_MAX / sizeof (mulib_linker_symbol_)
        || nr > SIZE_MAX / sizeof (mulib_linker_reloc_))
      return NULL;
    l = (mulib_linker_t *)calloc (1, sizeof (*l));
    if (!l)
      return NULL;
    if (ns)
      l->symbols = (mulib_linker_symbol_ *)calloc (ns, sizeof (*l->symbols));
    if (nr)
      l->relocations
          = (mulib_linker_reloc_ *)calloc (nr, sizeof (*l->relocations));
    if ((ns && !l->symbols) || (nr && !l->relocations))
      {
        mulib_linker_destroy (l);
        return NULL;
      }
    l->max_symbols = ns;
    l->max_relocations = nr;
    return l;
  }
  void
  mulib_linker_destroy (mulib_linker_t *l)
  {
    size_t i;
    if (!l)
      return;
    for (i = 0; i < l->symbol_count; ++i)
      free (l->symbols[i].name);
    free (l->symbols);
    free (l->relocations);
    free (l);
  }
  int
  mulib_linker_define (mulib_linker_t *l, const char *name, uint64_t value,
                       int absolute)
  {
    size_t n, i;
    int result;
    if (!l || !mulib_linker_name_ (name, &n)
        || (absolute != 0 && absolute != 1))
      return MULIB_LINKER_EINVAL;
    result = mulib_linker_intern_ (l, name, n, &i);
    if (result)
      return result;
    if (l->symbols[i].defined)
      return MULIB_LINKER_EDUPLICATE;
    l->symbols[i].value = value;
    l->symbols[i].defined = 1;
    l->symbols[i].absolute = absolute;
    return 0;
  }
  static unsigned
  mulib_linker_width_ (mulib_linker_kind kind)
  {
    return kind == MULIB_LINKER_ABS64 ? 8U : 4U;
  }
  int
  mulib_linker_relocate (mulib_linker_t *l, const char *name, size_t at,
                         mulib_linker_kind kind, int64_t addend)
  {
    size_t n, i, symbol;
    unsigned width;
    int result;
    if (!l || !mulib_linker_name_ (name, &n) || kind < MULIB_LINKER_ABS32
        || kind > MULIB_LINKER_PC32)
      return MULIB_LINKER_EINVAL;
    width = mulib_linker_width_ (kind);
    if (at > SIZE_MAX - width)
      return MULIB_LINKER_ERANGE;
    if (l->relocation_count == l->max_relocations)
      return MULIB_LINKER_ELIMIT;
    for (i = 0; i < l->relocation_count; ++i)
      {
        const mulib_linker_reloc_ *r = l->relocations + i;
        if (at < r->offset + mulib_linker_width_ (r->kind)
            && r->offset < at + width)
          return MULIB_LINKER_EOVERLAP;
      }
    result = mulib_linker_intern_ (l, name, n, &symbol);
    if (result)
      return result;
    l->relocations[l->relocation_count].symbol = symbol;
    l->relocations[l->relocation_count].offset = at;
    l->relocations[l->relocation_count].kind = kind;
    l->relocations[l->relocation_count++].addend = addend;
    return 0;
  }
  static int
  mulib_linker_add_ (uint64_t a, int64_t b, uint64_t *out)
  {
    uint64_t magnitude;
    if (b >= 0)
      {
        if (a > UINT64_MAX - (uint64_t)b)
          return 0;
        *out = a + (uint64_t)b;
      }
    else
      {
        magnitude = (uint64_t)(-(b + 1)) + 1;
        if (a < magnitude)
          return 0;
        *out = a - magnitude;
      }
    return 1;
  }
  static int
  mulib_linker_value_ (const mulib_linker_t *l, const mulib_emitter_t *e,
                       uint64_t base, size_t i, uint64_t *value)
  {
    const mulib_linker_reloc_ *r = l->relocations + i;
    const mulib_linker_symbol_ *s = l->symbols + r->symbol;
    uint64_t address, place, delta, magnitude;
    int negative;
    unsigned width = mulib_linker_width_ (r->kind);
    if (r->offset > e->size || width > e->size - r->offset)
      return MULIB_LINKER_ERANGE;
    if (!s->defined)
      return MULIB_LINKER_EUNDEFINED;
    address = s->value;
    if (!s->absolute)
      {
        if (s->value > e->size || s->value > UINT64_MAX - base)
          return MULIB_LINKER_ERANGE;
        address += base;
      }
    if (r->kind == MULIB_LINKER_PC32)
      {
        if (r->offset > UINT64_MAX - base)
          return MULIB_LINKER_ERANGE;
        place = base + r->offset;
        negative = address < place;
        delta = negative ? place - address : address - place;
        magnitude = r->addend < 0 ? (uint64_t)(-(r->addend + 1)) + 1
                                  : (uint64_t)r->addend;
        if (negative == (r->addend < 0))
          {
            if (delta > UINT64_MAX - magnitude)
              return MULIB_LINKER_ERANGE;
            delta += magnitude;
          }
        else if (delta >= magnitude)
          {
            delta -= magnitude;
          }
        else
          {
            delta = magnitude - delta;
            negative = !negative;
          }
        if (delta > (negative ? UINT64_C (2147483648) : (uint64_t)INT32_MAX))
          return MULIB_LINKER_ERANGE;
        *value
            = negative ? (UINT64_C (0x100000000) - delta) & UINT32_MAX : delta;
      }
    else
      {
        if (!mulib_linker_add_ (address, r->addend, &address))
          return MULIB_LINKER_ERANGE;
        if (r->kind == MULIB_LINKER_ABS32 && address > UINT32_MAX)
          return MULIB_LINKER_ERANGE;
        *value = address;
      }
    return 0;
  }
  int
  mulib_linker_apply (const mulib_linker_t *l, mulib_emitter_t *e,
                      uint64_t base, size_t *bad)
  {
    size_t i;
    uint64_t value;
    int result;
    if (bad)
      *bad = SIZE_MAX;
    if (!l || !e || e->size > e->capacity || (!e->data && e->capacity)
        || (e->order != MULIB_EMITTER_LITTLE && e->order != MULIB_EMITTER_BIG))
      return MULIB_LINKER_EINVAL;
    if (e->size > UINT64_MAX - base)
      return MULIB_LINKER_ERANGE;
    for (i = 0; i < l->relocation_count; ++i)
      {
        result = mulib_linker_value_ (l, e, base, i, &value);
        if (result)
          {
            if (bad)
              *bad = i;
            return result;
          }
      }
    for (i = 0; i < l->relocation_count; ++i)
      {
        (void)mulib_linker_value_ (l, e, base, i, &value);
        (void)mulib_emitter_patch_uint (
            e, l->relocations[i].offset, value,
            mulib_linker_width_ (l->relocations[i].kind));
      }
    return 0;
  }
  size_t
  mulib_linker_symbol_count (const mulib_linker_t *l)
  {
    return l ? l->symbol_count : 0;
  }
  size_t
  mulib_linker_relocation_count (const mulib_linker_t *l)
  {
    return l ? l->relocation_count : 0;
  }
#ifdef __cplusplus
}
#endif
#endif
