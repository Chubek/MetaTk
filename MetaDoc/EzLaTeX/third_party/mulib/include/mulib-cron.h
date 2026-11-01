/**
 * @file mulib-cron.h
 * @brief Bounded numeric five-field cron parser and civil-minute matcher.
 * Define MULIB_IMPL_CRON once. No threads, clocks or background scheduler.
 */
#ifndef MULIB_CRON_H
#define MULIB_CRON_H
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_CRON_MAX_SPEC 255
  typedef struct mulib_cron
  {
    uint64_t fields[5];
    int dom_any, dow_any;
  } mulib_cron_t;
  typedef int (*mulib_cron_fn) (void *context);
  enum
  {
    MULIB_CRON_OK = 0,
    MULIB_CRON_INVALID = -1
  };
  /** Parse minute hour day-of-month month day-of-week (0/7=Sunday).
   * Supports wildcards, numeric lists, inclusive ranges and range/wildcard
   * steps. On failure out is unchanged; error_offset is optional. */
  int mulib_cron_parse (mulib_cron_t *out, const char *spec,
                        size_t *error_offset);
  /** Return 1 match, 0 mismatch, -1 invalid schedule/civil fields. Seconds
   * ignored. Restricted day-of-month and day-of-week are ORed; literal * is
   * unrestricted. */
  int mulib_cron_match (const mulib_cron_t *schedule, const struct tm *civil);
  /** Match and invoke callback synchronously once per call, returning callback
   * code or zero if unmatched. Does not deduplicate repeated calls in the same
   * minute. */
  int mulib_cron_dispatch (const mulib_cron_t *schedule,
                           const struct tm *civil, mulib_cron_fn callback,
                           void *context, int *matched);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_CRON) && !defined(MULIB_CRON_IMPLEMENTED)
#define MULIB_CRON_IMPLEMENTED
static int
mulib_cron_number_ (const char *s, size_t end, size_t *pos, unsigned limit,
                    unsigned *out)
{
  unsigned value = 0;
  size_t start = *pos;
  while (*pos < end && s[*pos] >= '0' && s[*pos] <= '9')
    {
      unsigned digit = (unsigned)(s[*pos] - '0');
      if (digit > limit || value > (limit - digit) / 10)
        return 0;
      value = value * 10 + digit;
      ++*pos;
    }
  if (*pos == start)
    return 0;
  *out = value;
  return 1;
}
static int
mulib_cron_field_ (const char *s, size_t end, size_t *p, unsigned low,
                   unsigned high, int dow, uint64_t *mask)
{
  *mask = 0;
  for (;;)
    {
      unsigned first, last, step = 1, v;
      int span = 0;
      if (*p < end && s[*p] == '*')
        {
          ++*p;
          first = low;
          last = high;
          span = 1;
        }
      else
        {
          if (!mulib_cron_number_ (s, end, p, high, &first) || first < low)
            return 0;
          last = first;
          if (*p < end && s[*p] == '-')
            {
              ++*p;
              span = 1;
              if (!mulib_cron_number_ (s, end, p, high, &last) || last < first)
                return 0;
            }
        }
      if (*p < end && s[*p] == '/')
        {
          ++*p;
          if (!span || !mulib_cron_number_ (s, end, p, high - low + 1, &step)
              || !step)
            return 0;
        }
      for (v = first; v <= last; v += step)
        *mask |= UINT64_C (1) << (dow && v == 7 ? 0 : v);
      if (*p >= end || s[*p] != ',')
        return 1;
      ++*p;
    }
}
int
mulib_cron_parse (mulib_cron_t *out, const char *s, size_t *error)
{
  static const unsigned low[5] = { 0, 0, 1, 1, 0 },
                        high[5] = { 59, 23, 31, 12, 7 };
  mulib_cron_t parsed;
  size_t length = 0, p = 0, field;
  if (error)
    *error = 0;
  if (!out || !s)
    return MULIB_CRON_INVALID;
  while (length <= MULIB_CRON_MAX_SPEC && s[length])
    ++length;
  if (length > MULIB_CRON_MAX_SPEC)
    {
      if (error)
        *error = MULIB_CRON_MAX_SPEC;
      return MULIB_CRON_INVALID;
    }
  parsed.dom_any = parsed.dow_any = 0;
  for (field = 0; field < 5; ++field)
    {
      size_t start;
      while (p < length && (s[p] == ' ' || s[p] == '\t'))
        ++p;
      start = p;
      if (!mulib_cron_field_ (s, length, &p, low[field], high[field],
                              field == 4, &parsed.fields[field]))
        goto bad;
      if (field == 2)
        parsed.dom_any = p == start + 1 && s[start] == '*';
      if (field == 4)
        parsed.dow_any = p == start + 1 && s[start] == '*';
      if (field < 4 && (p == length || (s[p] != ' ' && s[p] != '\t')))
        goto bad;
    }
  while (p < length && (s[p] == ' ' || s[p] == '\t'))
    ++p;
  if (p != length)
    goto bad;
  *out = parsed;
  return MULIB_CRON_OK;
bad:
  if (error)
    *error = p;
  return MULIB_CRON_INVALID;
}
int
mulib_cron_match (const mulib_cron_t *s, const struct tm *t)
{
  static const unsigned char month_days[12]
      = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  static const uint64_t valid[5]
      = { (UINT64_C (1) << 60) - 1, (UINT64_C (1) << 24) - 1,
          (UINT64_C (1) << 32) - 2, (UINT64_C (1) << 13) - 2,
          (UINT64_C (1) << 7) - 1 };
  int dom, dow, days, year_mod;
  size_t i;
  if (!s || !t || t->tm_min < 0 || t->tm_min > 59 || t->tm_hour < 0
      || t->tm_hour > 23 || t->tm_mon < 0 || t->tm_mon > 11 || t->tm_wday < 0
      || t->tm_wday > 6 || (s->dom_any != 0 && s->dom_any != 1)
      || (s->dow_any != 0 && s->dow_any != 1))
    return MULIB_CRON_INVALID;
  for (i = 0; i < 5; ++i)
    if (!s->fields[i] || (s->fields[i] & ~valid[i]))
      return MULIB_CRON_INVALID;
  /* Reduction first also avoids overflow on hosts with a 64-bit int. */
  year_mod = (t->tm_year % 400 + 1900) % 400;
  days = month_days[t->tm_mon];
  if (t->tm_mon == 1 && year_mod % 4 == 0
      && (year_mod % 100 != 0 || year_mod == 0))
    ++days;
  if (t->tm_mday < 1 || t->tm_mday > days)
    return MULIB_CRON_INVALID;
  if (!(s->fields[0] & (UINT64_C (1) << t->tm_min))
      || !(s->fields[1] & (UINT64_C (1) << t->tm_hour))
      || !(s->fields[3] & (UINT64_C (1) << (t->tm_mon + 1))))
    return 0;
  dom = (s->fields[2] & (UINT64_C (1) << t->tm_mday)) != 0;
  dow = (s->fields[4] & (UINT64_C (1) << t->tm_wday)) != 0;
  return s->dom_any ? dow : (s->dow_any ? dom : (dom || dow));
}
int
mulib_cron_dispatch (const mulib_cron_t *s, const struct tm *t,
                     mulib_cron_fn fn, void *arg, int *matched)
{
  int rc;
  if (matched)
    *matched = 0;
  if (!fn)
    return MULIB_CRON_INVALID;
  rc = mulib_cron_match (s, t);
  if (rc < 0)
    return rc;
  if (matched)
    *matched = rc;
  return rc ? fn (arg) : 0;
}
#endif
