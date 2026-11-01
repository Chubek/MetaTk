/**
 * @file mulib-port.h
 * @brief Platform identification and checked size arithmetic for C11 modules.
 * Define MULIB_IMPL_PORT once. No operating-system headers or feature-test
 * macros are exposed. Platform macros are always defined to either 0 or 1.
 */
#ifndef MULIB_PORT_H
#define MULIB_PORT_H
#include <stddef.h>
#if defined(_WIN32)
#define MULIB_PORT_WINDOWS 1
#define MULIB_PORT_POSIX 0
#else
#define MULIB_PORT_WINDOWS 0
#if defined(__unix__) || defined(__APPLE__)
#define MULIB_PORT_POSIX 1
#else
#define MULIB_PORT_POSIX 0
#endif
#endif
#if defined(__linux__)
#define MULIB_PORT_LINUX 1
#else
#define MULIB_PORT_LINUX 0
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Return a static descriptive OS name, including "unknown". */
  const char *mulib_port_os (void);
  /** @brief Return a static descriptive architecture name. */
  const char *mulib_port_cpu (void);
  /** @brief Return nonzero on a little-endian host. */
  int mulib_port_little_endian (void);
  /** @brief Checked addition; failure leaves *out unchanged. */
  int mulib_port_add_size (size_t a, size_t b, size_t *out);
  /** @brief Checked multiplication; failure leaves *out unchanged. */
  int mulib_port_mul_size (size_t a, size_t b, size_t *out);
  /** @brief Round up to any nonzero alignment, checking for overflow. */
  int mulib_port_align_size (size_t size, size_t alignment, size_t *out);
  /** @brief Volatile byte erasure; NULL is accepted only for zero length. */
  void mulib_port_secure_zero (void *memory, size_t length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PORT_H */

#if defined(MULIB_IMPL_PORT) && !defined(MULIB_PORT_IMPLEMENTED)
#define MULIB_PORT_IMPLEMENTED
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  const char *
  mulib_port_os (void)
  {
#if MULIB_PORT_WINDOWS
    return "windows";
#elif MULIB_PORT_LINUX
  return "linux";
#elif defined(__APPLE__)
  return "darwin";
#elif defined(__FreeBSD__)
  return "freebsd";
#elif MULIB_PORT_POSIX
  return "posix";
#else
  return "unknown";
#endif
  }
  const char *
  mulib_port_cpu (void)
  {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
  return "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
  return "aarch64";
#elif defined(__arm__) || defined(_M_ARM)
  return "arm";
#elif defined(__riscv)
  return "riscv";
#elif defined(__powerpc64__)
  return "powerpc64";
#else
  return "unknown";
#endif
  }
  int
  mulib_port_little_endian (void)
  {
    const unsigned int one = 1;
    return *(const unsigned char *)&one == 1;
  }
  int
  mulib_port_add_size (size_t a, size_t b, size_t *out)
  {
    if (!out || b > SIZE_MAX - a)
      return 0;
    *out = a + b;
    return 1;
  }
  int
  mulib_port_mul_size (size_t a, size_t b, size_t *out)
  {
    if (!out || (a && b > SIZE_MAX / a))
      return 0;
    *out = a * b;
    return 1;
  }
  int
  mulib_port_align_size (size_t n, size_t alignment, size_t *out)
  {
    size_t remainder;
    if (!alignment || !out)
      return 0;
    remainder = n % alignment;
    return mulib_port_add_size (n, remainder ? alignment - remainder : 0, out);
  }
  void
  mulib_port_secure_zero (void *memory, size_t n)
  {
    volatile unsigned char *p = (volatile unsigned char *)memory;
    if (!p)
      return;
    while (n--)
      *p++ = 0;
  }
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PORT_IMPLEMENTED */
