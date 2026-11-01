/**
 * @file mulib-mmfile.h
 * @brief Shared, whole-file POSIX memory mappings; explicit unsupported
 * fallback. Define MULIB_IMPL_MMFILE in one translation unit. No dependency
 * switches.
 */
#ifndef MULIB_MMFILE_H
#define MULIB_MMFILE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief An owning mapping. Initialize with MULIB_MMFILE_INIT, never copy
   * it. */
  typedef struct mulib_mmfile
  {
    void *data;
    size_t size;
    int writable, active;
  } mulib_mmfile_t;
#define MULIB_MMFILE_INIT { NULL, 0, 0, 0 }
  /** @brief Results; EIO preserves the failing OS call's errno. */
  enum mulib_mmfile_result
  {
    MULIB_MMFILE_OK = 0,
    MULIB_MMFILE_EINVAL = -1,
    MULIB_MMFILE_EIO = -2,
    MULIB_MMFILE_EOVERFLOW = -3,
    MULIB_MMFILE_ENOTSUP = -4,
    MULIB_MMFILE_ESTATE = -5
  };
  /** @brief Map an existing regular file. writable is 0 or 1; no resizing
   * occurs. Empty files succeed with NULL data. Failure leaves an initialized
   * output alone. Read-only data must not be written. The file must not be
   * truncated while mapped.
   */
  int mulib_mmfile_open (mulib_mmfile_t *mapping, const char *path,
                         int writable);
  /** @brief Synchronously flush a writable mapping. Empty/read-only mappings
   * are no-ops. */
  int mulib_mmfile_flush (mulib_mmfile_t *mapping);
  /** @brief Unmap and reset; idempotent. Failure retains the mapping for
   * retry. Closing does not implicitly perform a synchronous flush.
   */
  int mulib_mmfile_close (mulib_mmfile_t *mapping);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MMFILE_H */

#if defined(MULIB_IMPL_MMFILE) && !defined(MULIB_MMFILE_IMPLEMENTED)
#define MULIB_MMFILE_IMPLEMENTED
#include <errno.h>
#include <stdint.h>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define MULIB_MMFILE_POSIX_ 1
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_mmfile_open (mulib_mmfile_t *m, const char *path, int writable)
  {
    if (!m || !path || (writable != 0 && writable != 1))
      return MULIB_MMFILE_EINVAL;
    if (m->active || m->data || m->size || m->writable)
      return MULIB_MMFILE_ESTATE;
#ifdef MULIB_MMFILE_POSIX_
    {
      int fd = open (path, writable ? O_RDWR : O_RDONLY);
      struct stat st;
      void *data = NULL;
      size_t size;
      int saved;
      if (fd < 0)
        return MULIB_MMFILE_EIO;
      if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0 || fstat (fd, &st) < 0)
        {
          saved = errno;
          (void)close (fd);
          errno = saved;
          return MULIB_MMFILE_EIO;
        }
      if (!S_ISREG (st.st_mode) || st.st_size < 0)
        {
          (void)close (fd);
          return MULIB_MMFILE_EINVAL;
        }
      if ((uintmax_t)st.st_size > (uintmax_t)SIZE_MAX)
        {
          (void)close (fd);
          return MULIB_MMFILE_EOVERFLOW;
        }
      size = (size_t)st.st_size;
      if (size)
        {
          data = mmap (NULL, size, PROT_READ | (writable ? PROT_WRITE : 0),
                       MAP_SHARED, fd, 0);
          if (data == MAP_FAILED)
            {
              saved = errno;
              (void)close (fd);
              errno = saved;
              return MULIB_MMFILE_EIO;
            }
        }
      if (close (fd) < 0)
        {
          saved = errno;
          if (size)
            (void)munmap (data, size);
          errno = saved;
          return MULIB_MMFILE_EIO;
        }
      m->data = data;
      m->size = size;
      m->writable = writable;
      m->active = 1;
      return MULIB_MMFILE_OK;
    }
#else
  return MULIB_MMFILE_ENOTSUP;
#endif
  }
  int
  mulib_mmfile_flush (mulib_mmfile_t *m)
  {
    if (!m || (!m->active && (m->data || m->size || m->writable)))
      return MULIB_MMFILE_EINVAL;
#ifdef MULIB_MMFILE_POSIX_
    if (m->size && m->writable && msync (m->data, m->size, MS_SYNC) < 0)
      return MULIB_MMFILE_EIO;
    return MULIB_MMFILE_OK;
#else
  return MULIB_MMFILE_ENOTSUP;
#endif
  }
  int
  mulib_mmfile_close (mulib_mmfile_t *m)
  {
    if (!m || (!m->active && (m->data || m->size || m->writable)))
      return MULIB_MMFILE_EINVAL;
#ifdef MULIB_MMFILE_POSIX_
    if (m->size && munmap (m->data, m->size) < 0)
      return MULIB_MMFILE_EIO;
    m->data = NULL;
    m->size = 0;
    m->writable = 0;
    m->active = 0;
    return MULIB_MMFILE_OK;
#else
  return MULIB_MMFILE_ENOTSUP;
#endif
  }
#ifdef __cplusplus
}
#endif
#undef MULIB_MMFILE_POSIX_
#endif /* implementation */
