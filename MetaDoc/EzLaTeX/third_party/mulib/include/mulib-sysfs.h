/**
 * @file mulib-sysfs.h
 * @brief Bounded Linux-style text attributes over native files or MuLIB VFS.
 * @details Relative paths reject traversal before I/O. Native Linux stdio
 * follows symlinks: the root and tree must be trusted, not a sandbox. No late
 * feature-test macros or third-party dependencies. Define MULIB_IMPL_SYSFS and
 * separately MULIB_IMPL_VFS. See manual/mulib-sysfs.md.
 */
#ifndef MULIB_SYSFS_H
#define MULIB_SYSFS_H
#include "mulib-vfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_sysfs
  {
    mulib_vfs *vfs;
    char root[MULIB_VFS_PATH_CAP];
    size_t max_bytes;
  } mulib_sysfs;
  /** @brief Copy normalized absolute root and borrow vfs; NULL vfs selects
   * native Linux stdio (ENOTSUP elsewhere). max_bytes must be nonzero and <
   * SIZE_MAX.
   */
  mulib_vfs_result mulib_sysfs_init (mulib_sysfs *sys, mulib_vfs *vfs,
                                     const char *root, size_t max_bytes);
  /** @brief Validate a nonempty relative path: no empty, . or .. components,
   * leading/trailing slash, backslash, controls, or more than 4095 bytes.
   */
  mulib_vfs_result mulib_sysfs_validate (const char *relative);
  /** @brief Read complete text including any newline, with NUL padding.
   * Capacity includes padding; excess data gives ELIMIT, embedded NUL gives
   * EFORMAT. On failure count is zero and output[0] is NUL when a buffer is
   * provided.
   */
  mulib_vfs_result mulib_sysfs_read (const mulib_sysfs *sys,
                                     const char *relative, char *output,
                                     size_t capacity, size_t *length);
  /** @brief Write text once at offset zero to an existing attribute; no
   * implicit newline, creation or truncation. A short write gives EIO and may
   * change data.
   */
  mulib_vfs_result mulib_sysfs_write (const mulib_sysfs *sys,
                                      const char *relative, const char *data,
                                      size_t length);
  /** @brief Read a decimal integer, allowing surrounding ASCII whitespace;
   * output changes only on success. Typed reads have a 127-byte text limit.
   */
  mulib_vfs_result mulib_sysfs_read_i64 (const mulib_sysfs *sys,
                                         const char *relative, int64_t *value);
  /** @brief Read unsigned decimal without a sign; check range exactly. */
  mulib_vfs_result mulib_sysfs_read_u64 (const mulib_sysfs *sys,
                                         const char *relative,
                                         uint64_t *value);
  /** @brief Read exactly numeric 0 or 1 (surrounding whitespace allowed). */
  mulib_vfs_result mulib_sysfs_read_bool (const mulib_sysfs *sys,
                                          const char *relative, int *value);
  /** @brief Write canonical decimal followed by newline. */
  mulib_vfs_result mulib_sysfs_write_i64 (const mulib_sysfs *sys,
                                          const char *relative, int64_t value);
  /** @brief Write canonical unsigned decimal followed by newline. */
  mulib_vfs_result mulib_sysfs_write_u64 (const mulib_sysfs *sys,
                                          const char *relative,
                                          uint64_t value);
  /** @brief Write 0 or 1 followed by newline; other values are invalid. */
  mulib_vfs_result mulib_sysfs_write_bool (const mulib_sysfs *sys,
                                           const char *relative, int value);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SYSFS) && !defined(MULIB_SYSFS_IMPLEMENTED)
#define MULIB_SYSFS_IMPLEMENTED
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_vfs_result
  mulib_sysfs_validate (const char *relative)
  {
    size_t i = 0, start = 0;
    if (!relative || !relative[0])
      return MULIB_VFS_EINVAL;
    for (;;)
      {
        unsigned char c;
        if (i >= MULIB_VFS_PATH_CAP)
          return MULIB_VFS_ELIMIT;
        c = (unsigned char)relative[i];
        if (c && (c < 32 || c == 127 || c == '\\'))
          return MULIB_VFS_EINVAL;
        if (!c || c == '/')
          {
            size_t n = i - start;
            if (!n || (n == 1 && relative[start] == '.')
                || (n == 2 && relative[start] == '.'
                    && relative[start + 1] == '.'))
              return MULIB_VFS_EINVAL;
            if (!c)
              return MULIB_VFS_OK;
            start = i + 1;
          }
        ++i;
      }
  }
  mulib_vfs_result
  mulib_sysfs_init (mulib_sysfs *sys, mulib_vfs *vfs, const char *root,
                    size_t max_bytes)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    mulib_vfs_result r;
    if (!sys || !max_bytes || max_bytes == SIZE_MAX
        || (vfs && !vfs->max_mounts))
      return MULIB_VFS_EINVAL;
#if !defined(__linux__)
    if (!vfs)
      return MULIB_VFS_ENOTSUP;
#endif
    r = mulib_vfs_normalize (root, normalized, sizeof (normalized));
    if (r)
      return r;
    memcpy (sys->root, normalized, strlen (normalized) + 1);
    sys->vfs = vfs;
    sys->max_bytes = max_bytes;
    return MULIB_VFS_OK;
  }
  static mulib_vfs_result
  mulib_sysfs_path_ (const mulib_sysfs *sys, const char *relative, char *path)
  {
    size_t a, b, sep;
    mulib_vfs_result r;
    if (!sys || !sys->max_bytes || !sys->root[0])
      return MULIB_VFS_EINVAL;
    r = mulib_sysfs_validate (relative);
    if (r)
      return r;
    a = strlen (sys->root);
    b = strlen (relative);
    sep = a == 1 ? 0 : 1;
    if (a + b + sep >= MULIB_VFS_PATH_CAP)
      return MULIB_VFS_ELIMIT;
    memcpy (path, sys->root, a);
    if (sep)
      path[a++] = '/';
    memcpy (path + a, relative, b + 1);
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_sysfs_read (const mulib_sysfs *sys, const char *relative, char *output,
                    size_t capacity, size_t *length)
  {
    char path[MULIB_VFS_PATH_CAP], extra;
    size_t limit, got = 0, more = 0;
    mulib_vfs_result r;
    if (length)
      *length = 0;
    if (output && capacity)
      output[0] = 0;
    if (!output || !capacity || !length)
      return MULIB_VFS_EINVAL;
    r = mulib_sysfs_path_ (sys, relative, path);
    if (r)
      return r;
    limit = capacity - 1;
    if (limit > sys->max_bytes)
      limit = sys->max_bytes;
    if (sys->vfs)
      {
        r = mulib_vfs_read (sys->vfs, path, 0, output, limit, &got);
        if (!r && got == limit)
          r = mulib_vfs_read (sys->vfs, path, got, &extra, 1, &more);
      }
    else
      {
#if defined(__linux__)
        FILE *file = fopen (path, "rb");
        if (!file)
          return errno == ENOENT ? MULIB_VFS_ENOENT : MULIB_VFS_EIO;
        got = fread (output, 1, limit, file);
        if (got == limit)
          {
            int c = fgetc (file);
            more = c == EOF ? 0 : 1;
          }
        r = ferror (file) ? MULIB_VFS_EIO : MULIB_VFS_OK;
        if (fclose (file) && !r)
          r = MULIB_VFS_EIO;
#else
      r = MULIB_VFS_ENOTSUP;
#endif
      }
    if (!r && more)
      r = MULIB_VFS_ELIMIT;
    if (!r && memchr (output, 0, got))
      r = MULIB_VFS_EFORMAT;
    if (r)
      {
        output[0] = 0;
        return r;
      }
    output[got] = 0;
    *length = got;
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_sysfs_write (const mulib_sysfs *sys, const char *relative,
                     const char *data, size_t length)
  {
    char path[MULIB_VFS_PATH_CAP];
    size_t wrote = 0;
    mulib_vfs_result r = mulib_sysfs_path_ (sys, relative, path);
    if (r)
      return r;
    if (!data && length)
      return MULIB_VFS_EINVAL;
    if (length > sys->max_bytes)
      return MULIB_VFS_ELIMIT;
    if (length && memchr (data, 0, length))
      return MULIB_VFS_EFORMAT;
    if (sys->vfs)
      {
        mulib_vfs_stat info;
        r = mulib_vfs_getstat (sys->vfs, path, &info);
        if (!r && info.kind != MULIB_VFS_FILE)
          r = MULIB_VFS_EISDIR;
        if (!r)
          r = mulib_vfs_write (sys->vfs, path, 0, data, length, &wrote);
      }
    else
      {
#if defined(__linux__)
        FILE *file = fopen (path, "r+b");
        if (!file)
          return errno == ENOENT ? MULIB_VFS_ENOENT : MULIB_VFS_EIO;
        if (length)
          wrote = fwrite (data, 1, length, file);
        r = fflush (file) ? MULIB_VFS_EIO : MULIB_VFS_OK;
        if (fclose (file) && !r)
          r = MULIB_VFS_EIO;
#else
      r = MULIB_VFS_ENOTSUP;
#endif
      }
    if (!r && wrote != length)
      r = MULIB_VFS_EIO;
    return r;
  }
  static int
  mulib_sysfs_space_ (char c)
  {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v'
           || c == '\f';
  }
  static mulib_vfs_result
  mulib_sysfs_number_ (const char *text, int signed_value, uint64_t *magnitude,
                       int *negative)
  {
    const char *p = text;
    uint64_t n = 0, limit = UINT64_MAX;
    int digits = 0;
    *negative = 0;
    while (mulib_sysfs_space_ (*p))
      ++p;
    if (signed_value)
      {
        if (*p == '-' || *p == '+')
          {
            *negative = *p == '-';
            ++p;
          }
        limit = (uint64_t)INT64_MAX + (unsigned)*negative;
      }
    while (*p >= '0' && *p <= '9')
      {
        unsigned digit = (unsigned)(*p++ - '0');
        if (n > (limit - digit) / 10)
          return MULIB_VFS_ELIMIT;
        n = n * 10 + digit;
        digits = 1;
      }
    if (!digits)
      return MULIB_VFS_EFORMAT;
    while (mulib_sysfs_space_ (*p))
      ++p;
    if (*p)
      return MULIB_VFS_EFORMAT;
    *magnitude = n;
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_sysfs_read_i64 (const mulib_sysfs *sys, const char *relative,
                        int64_t *value)
  {
    char text[128];
    size_t n;
    uint64_t magnitude;
    int negative;
    mulib_vfs_result r;
    if (!value)
      return MULIB_VFS_EINVAL;
    r = mulib_sysfs_read (sys, relative, text, sizeof (text), &n);
    if (!r)
      r = mulib_sysfs_number_ (text, 1, &magnitude, &negative);
    if (!r)
      *value = negative ? (magnitude == (uint64_t)INT64_MAX + 1
                               ? INT64_MIN
                               : -(int64_t)magnitude)
                        : (int64_t)magnitude;
    return r;
  }
  mulib_vfs_result
  mulib_sysfs_read_u64 (const mulib_sysfs *sys, const char *relative,
                        uint64_t *value)
  {
    char text[128];
    size_t n;
    uint64_t magnitude;
    int negative;
    mulib_vfs_result r;
    if (!value)
      return MULIB_VFS_EINVAL;
    r = mulib_sysfs_read (sys, relative, text, sizeof (text), &n);
    if (!r)
      r = mulib_sysfs_number_ (text, 0, &magnitude, &negative);
    if (!r)
      *value = magnitude;
    return r;
  }
  mulib_vfs_result
  mulib_sysfs_read_bool (const mulib_sysfs *sys, const char *relative,
                         int *value)
  {
    uint64_t n;
    mulib_vfs_result r;
    if (!value)
      return MULIB_VFS_EINVAL;
    r = mulib_sysfs_read_u64 (sys, relative, &n);
    if (!r && n > 1)
      r = MULIB_VFS_EFORMAT;
    if (!r)
      *value = (int)n;
    return r;
  }
  mulib_vfs_result
  mulib_sysfs_write_i64 (const mulib_sysfs *sys, const char *relative,
                         int64_t value)
  {
    char text[32];
    int n = snprintf (text, sizeof (text), "%" PRId64 "\n", value);
    if (n < 0 || (size_t)n >= sizeof (text))
      return MULIB_VFS_EIO;
    return mulib_sysfs_write (sys, relative, text, (size_t)n);
  }
  mulib_vfs_result
  mulib_sysfs_write_u64 (const mulib_sysfs *sys, const char *relative,
                         uint64_t value)
  {
    char text[32];
    int n = snprintf (text, sizeof (text), "%" PRIu64 "\n", value);
    if (n < 0 || (size_t)n >= sizeof (text))
      return MULIB_VFS_EIO;
    return mulib_sysfs_write (sys, relative, text, (size_t)n);
  }
  mulib_vfs_result
  mulib_sysfs_write_bool (const mulib_sysfs *sys, const char *relative,
                          int value)
  {
    if (value != 0 && value != 1)
      return MULIB_VFS_EINVAL;
    return mulib_sysfs_write (sys, relative, value ? "1\n" : "0\n", 2);
  }
#ifdef __cplusplus
}
#endif
#endif
