/**
 * @file mulib-jit.h
 * @brief Owning one-way RW-to-RX native code memory, Linux AMD64 SysV only.
 * Define MULIB_IMPL_JIT and MULIB_IMPL_DYNCALL explicitly in one C11 TU.
 */
#ifndef MULIB_JIT_H
#define MULIB_JIT_H
#include "mulib-dyncall.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_jit mulib_jit_t;
  typedef enum mulib_jit_result
  {
    MULIB_JIT_OK = 0,
    MULIB_JIT_EINVAL = -1,
    MULIB_JIT_ENOTSUP = -2,
    MULIB_JIT_ENOMEM = -3,
    MULIB_JIT_EOS = -4,
    MULIB_JIT_ESTATE = -5,
    MULIB_JIT_ERANGE = -6
  } mulib_jit_result;
  /** @brief Whether this build implements native execution, not OS permission.
   */
  int mulib_jit_supported (void);
  /** @brief Allocate zeroed RW, non-executable memory; *output must initially
   * be NULL. Capacity must be nonzero. Output unchanged on failure. No feature
   * macros needed.
   */
  int mulib_jit_create (mulib_jit_t **output, size_t capacity);
  /** @brief Copy into the RW allocation. Sealed objects reject all writes. */
  int mulib_jit_write (mulib_jit_t *jit, size_t offset, const void *bytes,
                       size_t length);
  /** @brief Irreversibly make nonempty code RX, never RWX; idempotent on
   * success. */
  int mulib_jit_seal (mulib_jit_t *jit);
  /** @brief Run sealed, trusted int64_t(void) SysV code at offset zero.
   * Not a sandbox: caller must supply valid ABI-conforming host machine code.
   * No executable pointer escapes; allocation remains live throughout the
   * call.
   */
  int mulib_jit_run_i64_0 (const mulib_jit_t *jit, int64_t *result);
  /** @brief Unmap and free; on OS failure retain ownership. NULL is harmless.
   */
  int mulib_jit_destroy (mulib_jit_t *jit);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_JIT) && !defined(MULIB_JIT_IMPLEMENTED)
#define MULIB_JIT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#if defined(__linux__) && defined(__x86_64__) && !defined(__ILP32__)
#define MULIB_JIT_NATIVE_ 1
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  struct mulib_jit
  {
    void *mapping, *memory;
    size_t capacity, mapped, size;
    int sealed;
  };
  int
  mulib_jit_supported (void)
  {
#ifdef MULIB_JIT_NATIVE_
    return sizeof (void *) == sizeof (mulib_dyncall_i64_0_fn);
#else
  return 0;
#endif
  }
  int
  mulib_jit_create (mulib_jit_t **out, size_t cap)
  {
    if (!out || *out || !cap)
      return MULIB_JIT_EINVAL;
#ifdef MULIB_JIT_NATIVE_
    {
      long page = sysconf (_SC_PAGESIZE);
      size_t mapped, remainder;
      mulib_jit_t *j;
      int fd;
      void *memory;
      if (!mulib_jit_supported ())
        return MULIB_JIT_ENOTSUP;
      if (page <= 0 || (uintmax_t)page > SIZE_MAX)
        return MULIB_JIT_EOS;
      /* A readable zero prefix permits native tooling (e.g. Clang's indirect
       * function sanitizer) to inspect bytes preceding an uninstrumented
       * entry. */
      if (cap > SIZE_MAX - 16)
        return MULIB_JIT_ERANGE;
      mapped = cap + 16;
      remainder = mapped % (size_t)page;
      if (remainder && mapped > SIZE_MAX - ((size_t)page - remainder))
        return MULIB_JIT_ERANGE;
      mapped += remainder ? (size_t)page - remainder : 0;
      j = (mulib_jit_t *)calloc (1, sizeof (*j));
      if (!j)
        return MULIB_JIT_ENOMEM;
      /* /dev/zero avoids MAP_ANONYMOUS and late feature-test macro changes. */
      fd = open ("/dev/zero", O_RDWR);
      if (fd < 0)
        {
          free (j);
          return MULIB_JIT_EOS;
        }
      memory = mmap (NULL, mapped, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
      (void)close (fd);
      if (memory == MAP_FAILED)
        {
          free (j);
          return MULIB_JIT_EOS;
        }
      j->mapping = memory;
      j->memory = (unsigned char *)memory + 16;
      j->capacity = cap;
      j->mapped = mapped;
      *out = j;
      return 0;
    }
#else
  return MULIB_JIT_ENOTSUP;
#endif
  }
  int
  mulib_jit_write (mulib_jit_t *j, size_t at, const void *bytes, size_t n)
  {
    if (!j || (!bytes && n))
      return MULIB_JIT_EINVAL;
    if (j->sealed)
      return MULIB_JIT_ESTATE;
    if (at > j->capacity || n > j->capacity - at)
      return MULIB_JIT_ERANGE;
    if (n)
      {
        memcpy ((unsigned char *)j->memory + at, bytes, n);
        if (at + n > j->size)
          j->size = at + n;
      }
    return 0;
  }
  int
  mulib_jit_seal (mulib_jit_t *j)
  {
    if (!j)
      return MULIB_JIT_EINVAL;
    if (j->sealed)
      return 0;
    if (!j->size)
      return MULIB_JIT_ESTATE;
#ifdef MULIB_JIT_NATIVE_
    /* AMD64 instruction/data caches are coherent. */
    if (mprotect (j->mapping, j->mapped, PROT_READ | PROT_EXEC) != 0)
      return MULIB_JIT_EOS;
    j->sealed = 1;
    return 0;
#else
  return MULIB_JIT_ENOTSUP;
#endif
  }
  int
  mulib_jit_run_i64_0 (const mulib_jit_t *j, int64_t *out)
  {
    if (!j || !out)
      return MULIB_JIT_EINVAL;
    if (!j->sealed)
      return MULIB_JIT_ESTATE;
#ifdef MULIB_JIT_NATIVE_
    {
      mulib_dyncall_i64_0_fn entry;
      if (sizeof (entry) != sizeof (j->memory))
        return MULIB_JIT_ENOTSUP;
      /* Linux AMD64 ABI extension, not an ISO C object/function pointer cast.
       */
      memcpy (&entry, &j->memory, sizeof (entry));
      return mulib_dyncall_i64_0 (entry, out);
    }
#else
  return MULIB_JIT_ENOTSUP;
#endif
  }
  int
  mulib_jit_destroy (mulib_jit_t *j)
  {
    if (!j)
      return 0;
#ifdef MULIB_JIT_NATIVE_
    if (munmap (j->mapping, j->mapped) != 0)
      return MULIB_JIT_EOS;
    free (j);
    return 0;
#else
  return MULIB_JIT_ENOTSUP;
#endif
  }
#ifdef __cplusplus
}
#endif
#endif
