/**
 * @file mulib-dnscache.h
 * @brief Fixed-capacity, caller-clock positive A/AAAA RRset cache.
 * Define MULIB_IMPL_DNSCACHE and, separately, MULIB_IMPL_DNS.
 */
#ifndef MULIB_DNSCACHE_H
#define MULIB_DNSCACHE_H
#include "mulib-dns.h"
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_DNSCACHE_MAX_ADDRESSES = 8
  };
  /** @brief Owned value: up to eight network-order addresses; IPv4 uses first
   * four bytes. */
  typedef struct mulib_dnscache_result
  {
    uint16_t type;
    uint32_t ttl;
    size_t count;
    unsigned char addresses[8][16];
  } mulib_dnscache_result_t;
  /** @brief Cache slot, initialized by init; caller must not modify live
   * slots. */
  typedef struct mulib_dnscache_entry
  {
    char name[256];
    mulib_dnscache_result_t result;
    uint64_t inserted;
    int occupied;
  } mulib_dnscache_entry_t;
  /** @brief Borrows the slots until reinitialization; no allocation or
   * synchronization. */
  typedef struct mulib_dnscache
  {
    mulib_dnscache_entry_t *entries;
    size_t capacity;
  } mulib_dnscache_t;
  /** @brief Initialize/clear caller-provided slots; NULL storage is valid for
   * zero capacity. */
  int mulib_dnscache_init (mulib_dnscache_t *cache,
                           mulib_dnscache_entry_t *storage, size_t capacity);
  /** @brief Copy a complete RRset; zero TTL removes the old key, without
   * storing it. */
  int mulib_dnscache_put (mulib_dnscache_t *cache, const char *name,
                          size_t length, const mulib_dnscache_result_t *result,
                          uint64_t now);
  /** @brief Copy a hit with remaining TTL; NOTFOUND leaves output unchanged.
   */
  int mulib_dnscache_get (mulib_dnscache_t *cache, const char *name,
                          size_t length, uint16_t type, uint64_t now,
                          mulib_dnscache_result_t *out);
  /** @brief Remove expired slots, including slots newer than a
   * backwards-moving clock. */
  size_t mulib_dnscache_expire (mulib_dnscache_t *cache, uint64_t now);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DNSCACHE) && !defined(MULIB_DNSCACHE_IMPLEMENTED)
#define MULIB_DNSCACHE_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_dnscache_valid_ (const mulib_dnscache_t *c)
  {
    return c && (!c->capacity || c->entries)
           && c->capacity <= SIZE_MAX / sizeof (*c->entries);
  }
  static int
  mulib_dnscache_live_ (const mulib_dnscache_entry_t *e, uint64_t now)
  {
    return e->occupied && now >= e->inserted
           && now - e->inserted < e->result.ttl;
  }
  int
  mulib_dnscache_init (mulib_dnscache_t *c, mulib_dnscache_entry_t *storage,
                       size_t cap)
  {
    if (!c || (cap && !storage) || cap > SIZE_MAX / sizeof (*storage))
      return MULIB_DNS_INVALID;
    if (cap)
      memset (storage, 0, cap * sizeof (*storage));
    c->entries = storage;
    c->capacity = cap;
    return MULIB_DNS_OK;
  }
  size_t
  mulib_dnscache_expire (mulib_dnscache_t *c, uint64_t now)
  {
    size_t count = 0;
    if (!mulib_dnscache_valid_ (c))
      return 0;
    for (size_t i = 0; i < c->capacity; ++i)
      if (c->entries[i].occupied
          && !mulib_dnscache_live_ (&c->entries[i], now))
        {
          c->entries[i].occupied = 0;
          ++count;
        }
    return count;
  }
  int
  mulib_dnscache_put (mulib_dnscache_t *c, const char *name, size_t n,
                      const mulib_dnscache_result_t *result, uint64_t now)
  {
    char key[256];
    mulib_dnscache_result_t copy;
    size_t slot = SIZE_MAX;
    uint64_t shortest = UINT64_MAX;
    int rc;
    if (!mulib_dnscache_valid_ (c) || !result || !result->count)
      return MULIB_DNS_INVALID;
    if (result->type != MULIB_DNS_A && result->type != MULIB_DNS_AAAA)
      return MULIB_DNS_UNSUPPORTED;
    if (result->count > 8)
      return MULIB_DNS_LIMIT;
    rc = mulib_dns_name_normalize (name, n, key);
    if (rc != MULIB_DNS_OK)
      return rc;
    copy = *result;
    for (size_t i = 0; i < c->capacity; ++i)
      {
        mulib_dnscache_entry_t *e = &c->entries[i];
        uint64_t remaining;
        if (e->occupied && e->result.type == copy.type
            && !strcmp (e->name, key))
          {
            slot = i;
            break;
          }
        remaining = mulib_dnscache_live_ (e, now)
                        ? e->result.ttl - (now - e->inserted)
                        : 0;
        if (slot == SIZE_MAX || remaining < shortest)
          {
            shortest = remaining;
            slot = i;
          }
      }
    if (slot == SIZE_MAX)
      return copy.ttl ? MULIB_DNS_LIMIT : MULIB_DNS_OK;
    if (!copy.ttl)
      {
        if (c->entries[slot].occupied
            && c->entries[slot].result.type == copy.type
            && !strcmp (c->entries[slot].name, key))
          c->entries[slot].occupied = 0;
        return MULIB_DNS_OK;
      }
    memcpy (c->entries[slot].name, key, strlen (key) + 1);
    c->entries[slot].result = copy;
    c->entries[slot].inserted = now;
    c->entries[slot].occupied = 1;
    return MULIB_DNS_OK;
  }
  int
  mulib_dnscache_get (mulib_dnscache_t *c, const char *name, size_t n,
                      uint16_t type, uint64_t now,
                      mulib_dnscache_result_t *out)
  {
    char key[256];
    int rc;
    if (!mulib_dnscache_valid_ (c) || !out)
      return MULIB_DNS_INVALID;
    if (type != MULIB_DNS_A && type != MULIB_DNS_AAAA)
      return MULIB_DNS_UNSUPPORTED;
    rc = mulib_dns_name_normalize (name, n, key);
    if (rc != MULIB_DNS_OK)
      return rc;
    for (size_t i = 0; i < c->capacity; ++i)
      {
        mulib_dnscache_entry_t *e = &c->entries[i];
        if (e->occupied && e->result.type == type && !strcmp (e->name, key))
          {
            if (!mulib_dnscache_live_ (e, now))
              {
                e->occupied = 0;
                return MULIB_DNS_NOTFOUND;
              }
            *out = e->result;
            out->ttl -= (uint32_t)(now - e->inserted);
            return MULIB_DNS_OK;
          }
      }
    return MULIB_DNS_NOTFOUND;
  }
#ifdef __cplusplus
}
#endif
#endif
