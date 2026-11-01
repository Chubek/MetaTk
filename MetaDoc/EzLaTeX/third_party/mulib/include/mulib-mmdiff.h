/**
 * @file mulib-mmdiff.h
 * @brief Binary memory diff: bounded hash-indexed COPY/INSERT generation.
 * Define MULIB_IMPL_MMDIFF, MULIB_IMPL_VEC, MULIB_IMPL_BUFFER in one TU.
 * This module concerns byte buffers only, not an external memtkx format/API.
 * Patches own an exact source snapshot and literals; no wire format is
 * defined.
 */
#ifndef MULIB_MMDIFF_H
#define MULIB_MMDIFF_H
#include "mulib-buffer.h"
#include "mulib-vec.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Error codes, without errno semantics. */
  typedef enum mulib_mmdiff_status
  {
    MULIB_MMDIFF_OK = 0,
    MULIB_MMDIFF_INVALID,
    MULIB_MMDIFF_LIMIT,
    MULIB_MMDIFF_NOMEM
  } mulib_mmdiff_status_t;
  /** @brief COPY addresses the original source, INSERT the patch literal
   * buffer. */
  typedef enum mulib_mmdiff_kind
  {
    MULIB_MMDIFF_COPY = 1,
    MULIB_MMDIFF_INSERT = 2
  } mulib_mmdiff_kind_t;
  /** @brief A nonempty binary span; COPYs may reorder/repeat source bytes. */
  typedef struct mulib_mmdiff_command
  {
    mulib_mmdiff_kind_t kind;
    size_t offset, length;
  } mulib_mmdiff_command_t;
  /** @brief Owning in-memory patch; initialize/free and do not shallow-copy.
   */
  typedef struct mulib_mmdiff_patch
  {
    mulib_vec_t commands;
    mulib_buffer_t source, inserts;
    size_t target_length;
  } mulib_mmdiff_patch_t;
  /** @brief Hard input/index/work limits; zero is a real limit, not unlimited.
   * Work counts hash-window byte reads, rolling steps, candidate visits/byte
   * compares. NULL selects 8-byte matches, 16 MiB inputs, 1,000,000 index
   * entries, and 64,000,000 work units. min_match must be positive. */
  typedef struct mulib_mmdiff_limits
  {
    size_t min_match, max_bytes, max_index_entries, max_work;
  } mulib_mmdiff_limits_t;
  /** @brief Initialize an uninitialized patch. */
  void mulib_mmdiff_init (mulib_mmdiff_patch_t *patch);
  /** @brief Release/reset; NULL is accepted. */
  void mulib_mmdiff_free (mulib_mmdiff_patch_t *patch);
  /** @brief Greedily choose longest verified source match at each target
   * position. Uses a rolling-hash index of every source window; not an
   * optimal/minimal diff. On error initialized out is unchanged. NULL bytes
   * require zero length. */
  mulib_mmdiff_status_t mulib_mmdiff_make (const void *old_data,
                                           size_t old_length,
                                           const void *new_data,
                                           size_t new_length,
                                           const mulib_mmdiff_limits_t *limits,
                                           mulib_mmdiff_patch_t *out);
  /** @brief Validate exact base and every command, then transactionally
   * replace initialized out. out must not be a patch member; base may alias
   * out->data. No in-place source writes or string terminator; max_output is a
   * hard cap. */
  mulib_mmdiff_status_t mulib_mmdiff_apply (const mulib_mmdiff_patch_t *patch,
                                            const void *base,
                                            size_t base_length,
                                            size_t max_output,
                                            mulib_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MMDIFF_H */

#if defined(MULIB_IMPL_MMDIFF) && !defined(MULIB_MMDIFF_IMPLEMENTED)
#define MULIB_MMDIFF_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  void
  mulib_mmdiff_init (mulib_mmdiff_patch_t *p)
  {
    if (p)
      {
        mulib_vec_init (&p->commands, sizeof (mulib_mmdiff_command_t));
        mulib_buffer_init (&p->source);
        mulib_buffer_init (&p->inserts);
        p->target_length = 0;
      }
  }
  void
  mulib_mmdiff_free (mulib_mmdiff_patch_t *p)
  {
    if (p)
      {
        mulib_vec_free (&p->commands);
        mulib_buffer_free (&p->source);
        mulib_buffer_free (&p->inserts);
        mulib_mmdiff_init (p);
      }
  }
  static int
  mulib_mmdiff_command_ (mulib_mmdiff_patch_t *p, mulib_mmdiff_kind_t kind,
                         size_t off, size_t len)
  {
    mulib_mmdiff_command_t c, *last;
    if (!len)
      return 1;
    last = p->commands.length ? (mulib_mmdiff_command_t *)mulib_vec_at (
                                    &p->commands, p->commands.length - 1)
                              : NULL;
    if (last && last->kind == kind && last->length <= SIZE_MAX - last->offset
        && last->offset + last->length == off
        && len <= SIZE_MAX - last->length)
      {
        last->length += len;
        return 1;
      }
    c.kind = kind;
    c.offset = off;
    c.length = len;
    return mulib_vec_push (&p->commands, &c);
  }
  static int
  mulib_mmdiff_work_ (size_t *remaining, size_t amount)
  {
    if (amount > *remaining)
      return 0;
    *remaining -= amount;
    return 1;
  }
  static size_t
  mulib_mmdiff_slot_ (uint64_t hash, size_t mask)
  {
    return (size_t)(hash ^ (hash >> 32)) & mask;
  }
  mulib_mmdiff_status_t
  mulib_mmdiff_make (const void *old_data, size_t an, const void *new_data,
                     size_t bn, const mulib_mmdiff_limits_t *limits,
                     mulib_mmdiff_patch_t *out)
  {
    mulib_mmdiff_limits_t defaults
        = { 8, 16u * 1024u * 1024u, 1000000u, 64000000u };
    const unsigned char *a = (const unsigned char *)old_data,
                        *b = (const unsigned char *)new_data;
    mulib_mmdiff_patch_t p;
    size_t *heads = NULL, *next = NULL, entries = 0, slots = 1, remaining,
           pos = 0;
    size_t window;
    uint64_t power = 1;
    mulib_mmdiff_status_t status = MULIB_MMDIFF_OK;
    if (!out || (!a && an) || (!b && bn))
      return MULIB_MMDIFF_INVALID;
    if (!limits)
      limits = &defaults;
    if (!limits->min_match)
      return MULIB_MMDIFF_INVALID;
    if (an > limits->max_bytes || bn > limits->max_bytes)
      return MULIB_MMDIFF_LIMIT;
    remaining = limits->max_work;
    window = limits->min_match;
    mulib_mmdiff_init (&p);
    p.target_length = bn;
    if (!mulib_buffer_append (&p.source, a, an))
      {
        status = MULIB_MMDIFF_NOMEM;
        goto done;
      }
    if (an >= window && bn >= window)
      {
        uint64_t hash = 0;
        entries = an - window + 1;
        if (entries > limits->max_index_entries
            || entries > SIZE_MAX / sizeof (*next) || entries > SIZE_MAX / 2)
          {
            status = MULIB_MMDIFF_LIMIT;
            goto done;
          }
        while (slots < entries * 2)
          {
            if (slots > SIZE_MAX / 2)
              {
                status = MULIB_MMDIFF_LIMIT;
                goto done;
              }
            slots *= 2;
          }
        if (slots > SIZE_MAX / sizeof (*heads))
          {
            status = MULIB_MMDIFF_LIMIT;
            goto done;
          }
        heads = (size_t *)malloc (slots * sizeof (*heads));
        next = (size_t *)malloc (entries * sizeof (*next));
        if (!heads || !next)
          {
            status = MULIB_MMDIFF_NOMEM;
            goto done;
          }
        for (size_t i = 0; i < slots; ++i)
          heads[i] = SIZE_MAX;
        if (!mulib_mmdiff_work_ (&remaining, window))
          {
            status = MULIB_MMDIFF_LIMIT;
            goto done;
          }
        for (size_t i = 0; i < window; ++i)
          {
            hash = hash * UINT64_C (257) + a[i];
            if (i)
              power *= UINT64_C (257);
          }
        for (size_t i = 0; i < entries; ++i)
          {
            size_t slot = mulib_mmdiff_slot_ (hash, slots - 1);
            next[i] = heads[slot];
            heads[slot] = i;
            if (i + 1 < entries)
              {
                if (!mulib_mmdiff_work_ (&remaining, 1))
                  {
                    status = MULIB_MMDIFF_LIMIT;
                    goto done;
                  }
                hash = (hash - (uint64_t)a[i] * power) * UINT64_C (257)
                       + a[i + window];
              }
          }
        /* Favor early offsets: identical/repetitive bases then match
         * immediately. */
        for (size_t i = 0; i < slots; ++i)
          {
            size_t current = heads[i], previous = SIZE_MAX;
            while (current != SIZE_MAX)
              {
                size_t following = next[current];
                next[current] = previous;
                previous = current;
                current = following;
              }
            heads[i] = previous;
          }
      }
    while (pos < bn)
      {
        size_t best = 0, best_offset = 0;
        if (entries && window <= bn - pos)
          {
            uint64_t hash = 0;
            size_t candidate;
            if (!mulib_mmdiff_work_ (&remaining, window))
              {
                status = MULIB_MMDIFF_LIMIT;
                goto done;
              }
            for (size_t i = 0; i < window; ++i)
              hash = hash * UINT64_C (257) + b[pos + i];
            candidate = heads[mulib_mmdiff_slot_ (hash, slots - 1)];
            while (candidate != SIZE_MAX)
              {
                size_t len = 0, max = an - candidate;
                if (!mulib_mmdiff_work_ (&remaining, 1))
                  {
                    status = MULIB_MMDIFF_LIMIT;
                    goto done;
                  }
                if (max > bn - pos)
                  max = bn - pos;
                if (max <= best)
                  {
                    candidate = next[candidate];
                    continue;
                  }
                /* Even a hash-bucket collision must compare actual bytes. */
                while (len < max)
                  {
                    if (!mulib_mmdiff_work_ (&remaining, 1))
                      {
                        status = MULIB_MMDIFF_LIMIT;
                        goto done;
                      }
                    if (a[candidate + len] != b[pos + len])
                      break;
                    ++len;
                  }
                if (len >= window && len > best)
                  {
                    best = len;
                    best_offset = candidate;
                  }
                if (best == bn - pos || best == an)
                  break;
                candidate = next[candidate];
              }
          }
        if (best)
          {
            if (!mulib_mmdiff_command_ (&p, MULIB_MMDIFF_COPY, best_offset,
                                        best))
              {
                status = MULIB_MMDIFF_NOMEM;
                goto done;
              }
            pos += best;
          }
        else
          {
            size_t off = p.inserts.length;
            if (!mulib_buffer_append (&p.inserts, b + pos, 1)
                || !mulib_mmdiff_command_ (&p, MULIB_MMDIFF_INSERT, off, 1))
              {
                status = MULIB_MMDIFF_NOMEM;
                goto done;
              }
            ++pos;
          }
      }
    mulib_mmdiff_free (out);
    *out = p;
    mulib_mmdiff_init (&p);
  done:
    free (heads);
    free (next);
    mulib_mmdiff_free (&p);
    return status;
  }
  static int
  mulib_mmdiff_buffer_valid_ (const mulib_buffer_t *b)
  {
    return b->length <= b->capacity && (!b->length || b->data);
  }
  mulib_mmdiff_status_t
  mulib_mmdiff_apply (const mulib_mmdiff_patch_t *p, const void *base,
                      size_t n, size_t cap, mulib_buffer_t *out)
  {
    size_t produced = 0;
    mulib_buffer_t result;
    if (!p || !out || out == &p->source || out == &p->inserts || (!base && n)
        || !mulib_mmdiff_buffer_valid_ (&p->source)
        || !mulib_mmdiff_buffer_valid_ (&p->inserts)
        || p->commands.stride != sizeof (mulib_mmdiff_command_t)
        || p->commands.length > p->commands.capacity
        || p->commands.length > SIZE_MAX / sizeof (mulib_mmdiff_command_t)
        || (p->commands.length && !p->commands.data) || p->source.length != n
        || (n && memcmp (p->source.data, base, n)))
      return MULIB_MMDIFF_INVALID;
    if (p->target_length > cap)
      return MULIB_MMDIFF_LIMIT;
    for (size_t i = 0; i < p->commands.length; ++i)
      {
        const mulib_mmdiff_command_t *c
            = (const mulib_mmdiff_command_t *)(p->commands.data
                                               + i * p->commands.stride);
        size_t available;
        if (c->kind == MULIB_MMDIFF_COPY)
          available = n;
        else if (c->kind == MULIB_MMDIFF_INSERT)
          available = p->inserts.length;
        else
          return MULIB_MMDIFF_INVALID;
        if (!c->length || c->offset > available
            || c->length > available - c->offset
            || c->length > SIZE_MAX - produced)
          return MULIB_MMDIFF_INVALID;
        produced += c->length;
      }
    if (produced != p->target_length)
      return MULIB_MMDIFF_INVALID;
    mulib_buffer_init (&result);
    for (size_t i = 0; i < p->commands.length; ++i)
      {
        const mulib_mmdiff_command_t *c
            = (const mulib_mmdiff_command_t *)(p->commands.data
                                               + i * p->commands.stride);
        const unsigned char *bytes
            = c->kind == MULIB_MMDIFF_COPY ? p->source.data : p->inserts.data;
        if (!mulib_buffer_append (&result, bytes + c->offset, c->length))
          {
            mulib_buffer_free (&result);
            return MULIB_MMDIFF_NOMEM;
          }
      }
    mulib_buffer_free (out);
    *out = result;
    return MULIB_MMDIFF_OK;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
