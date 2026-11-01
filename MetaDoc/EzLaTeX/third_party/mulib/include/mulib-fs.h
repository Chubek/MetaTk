/**
 * @file mulib-fs.h
 * @brief Whole-file read and write helpers with explicit byte length.
 * @defgroup mulib_fs MuLIB fs
 * @{
 * @details Reading allocates a NUL padding byte after the actual file contents
 * for convenience, but binary data may itself contain NULs. Writing truncates
 * existing destination files; writes are not atomic.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_FS before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_FS_H
#define MULIB_FS_H
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

  /** @brief mulib fs read; see manual/mulib-fs.md for contracts and examples.
   */
  unsigned char *mulib_fs_read (const char *path, size_t *length);
  /** @brief mulib fs write; see manual/mulib-fs.md for contracts and examples.
   */
  int mulib_fs_write (const char *path, const void *data, size_t length);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_FS_H */

#if defined(MULIB_IMPL_FS) && !defined(MULIB_FS_IMPLEMENTED)
#define MULIB_FS_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  unsigned char *
  mulib_fs_read (const char *path, size_t *length)
  {
    if (!path)
      return NULL;
    FILE *f = fopen (path, "rb");
    if (!f)
      return NULL;
    size_t cap = 4096, n = 0;
    unsigned char *b = malloc (cap + 1);
    if (!b)
      {
        fclose (f);
        return NULL;
      }
    for (;;)
      {
        if (n == cap)
          {
            if (cap > (SIZE_MAX - 1) / 2)
              {
                free (b);
                fclose (f);
                return NULL;
              }
            cap *= 2;
            void *p = realloc (b, cap + 1);
            if (!p)
              {
                free (b);
                fclose (f);
                return NULL;
              }
            b = p;
          }
        size_t got = fread (b + n, 1, cap - n, f);
        n += got;
        if (got == 0)
          {
            if (ferror (f))
              {
                free (b);
                fclose (f);
                return NULL;
              }
            break;
          }
      }
    if (fclose (f) != 0)
      {
        free (b);
        return NULL;
      }
    b[n] = 0;
    if (length)
      *length = n;
    return b;
  }
  int
  mulib_fs_write (const char *path, const void *p, size_t n)
  {
    if (!path || (!p && n))
      return 0;
    FILE *f = fopen (path, "wb");
    if (!f)
      return 0;
    size_t done = n ? fwrite (p, 1, n, f) : 0;
    int ok = done == n && fflush (f) == 0;
    return fclose (f) == 0 && ok;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
