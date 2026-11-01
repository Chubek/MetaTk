/**
 * @file mulib-dynload.h
 * @brief Eager, local OS shared-library loading and symbol lookup.
 * Define MULIB_IMPL_DYNLOAD once. Linux/other libdl systems link with -ldl.
 */
#ifndef MULIB_DYNLOAD_H
#define MULIB_DYNLOAD_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Owning library handle; initialize and do not copy. */
  typedef struct mulib_dynload
  {
    void *handle;
    int owned;
  } mulib_dynload_t;
#define MULIB_DYNLOAD_INIT { NULL, 0 }
  /** @brief Zero is success; negative results distinguish argument, loader and
   * state errors. */
  enum mulib_dynload_result
  {
    MULIB_DYNLOAD_OK = 0,
    MULIB_DYNLOAD_EINVAL = -1,
    MULIB_DYNLOAD_EIO = -2,
    MULIB_DYNLOAD_ENOTSUP = -3,
    MULIB_DYNLOAD_ESTATE = -4
  };
  /** @brief Load a library, or the current process when path is NULL.
   * Optional error buffer receives a terminated message (empty on success).
   * Failure leaves output unchanged; path resolution is the native loader's
   * policy.
   */
  int mulib_dynload_open (mulib_dynload_t *library, const char *path,
                          char *error, size_t capacity);
  /** @brief Look up a symbol, permitting a successful NULL-valued symbol on
   * POSIX. Output is unchanged on failure. It is borrowed until the library
   * closes. Function-pointer conversion follows the host ABI, not an ISO C
   * guarantee.
   */
  int mulib_dynload_symbol (const mulib_dynload_t *library, const char *name,
                            void **symbol, char *error, size_t capacity);
  /** @brief Release the handle; idempotent, retaining a handle if native close
   * fails. */
  int mulib_dynload_close (mulib_dynload_t *library, char *error,
                           size_t capacity);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DYNLOAD_H */

#if defined(MULIB_IMPL_DYNLOAD) && !defined(MULIB_DYNLOAD_IMPLEMENTED)
#define MULIB_DYNLOAD_IMPLEMENTED
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  static void
  mulib_dynload_message_ (char *out, size_t cap, const char *message)
  {
    size_t n;
    if (!out || !cap)
      return;
    n = strlen (message);
    if (n >= cap)
      n = cap - 1;
    if (n)
      memcpy (out, message, n);
    out[n] = '\0';
  }
#ifdef _WIN32
  static void
  mulib_dynload_winerror_ (char *out, size_t cap)
  {
    char text[80];
    (void)snprintf (text, sizeof (text), "Windows loader error %lu",
                    (unsigned long)GetLastError ());
    mulib_dynload_message_ (out, cap, text);
  }
#endif
  int
  mulib_dynload_open (mulib_dynload_t *d, const char *path, char *error,
                      size_t cap)
  {
    mulib_dynload_message_ (error, cap, "");
    if (!d)
      {
        mulib_dynload_message_ (error, cap, "invalid library output");
        return MULIB_DYNLOAD_EINVAL;
      }
    if (d->handle)
      {
        mulib_dynload_message_ (error, cap, "library already open");
        return MULIB_DYNLOAD_ESTATE;
      }
#if defined(_WIN32)
    {
      HMODULE h = path ? LoadLibraryA (path) : GetModuleHandleA (NULL);
      if (!h)
        {
          mulib_dynload_winerror_ (error, cap);
          return MULIB_DYNLOAD_EIO;
        }
      d->handle = (void *)h;
      d->owned = path != NULL;
      return MULIB_DYNLOAD_OK;
    }
#elif defined(__unix__) || defined(__APPLE__)
  {
    void *h = dlopen (path, RTLD_NOW | RTLD_LOCAL);
    if (!h)
      {
        const char *message = dlerror ();
        mulib_dynload_message_ (error, cap,
                                message ? message : "dlopen failed");
        return MULIB_DYNLOAD_EIO;
      }
    d->handle = h;
    d->owned = 1;
    return MULIB_DYNLOAD_OK;
  }
#else
  (void)path;
  mulib_dynload_message_ (error, cap, "dynamic loading unsupported");
  return MULIB_DYNLOAD_ENOTSUP;
#endif
  }
  int
  mulib_dynload_symbol (const mulib_dynload_t *d, const char *name,
                        void **symbol, char *error, size_t cap)
  {
    mulib_dynload_message_ (error, cap, "");
    if (!d || !d->handle || !name || !symbol)
      {
        mulib_dynload_message_ (error, cap, "invalid symbol lookup");
        return MULIB_DYNLOAD_EINVAL;
      }
#if defined(_WIN32)
    {
      FARPROC address = GetProcAddress ((HMODULE)d->handle, name);
      if (!address)
        {
          mulib_dynload_winerror_ (error, cap);
          return MULIB_DYNLOAD_EIO;
        }
      if (sizeof (address) != sizeof (*symbol))
        {
          mulib_dynload_message_ (error, cap,
                                  "symbol representation unsupported");
          return MULIB_DYNLOAD_ENOTSUP;
        }
      memcpy (symbol, &address, sizeof (*symbol));
      return MULIB_DYNLOAD_OK;
    }
#elif defined(__unix__) || defined(__APPLE__)
  {
    void *address;
    const char *message;
    (void)dlerror ();
    address = dlsym (d->handle, name);
    message = dlerror ();
    if (message)
      {
        mulib_dynload_message_ (error, cap, message);
        return MULIB_DYNLOAD_EIO;
      }
    *symbol = address;
    return MULIB_DYNLOAD_OK;
  }
#else
  mulib_dynload_message_ (error, cap, "dynamic loading unsupported");
  return MULIB_DYNLOAD_ENOTSUP;
#endif
  }
  int
  mulib_dynload_close (mulib_dynload_t *d, char *error, size_t cap)
  {
    mulib_dynload_message_ (error, cap, "");
    if (!d)
      {
        mulib_dynload_message_ (error, cap, "invalid library");
        return MULIB_DYNLOAD_EINVAL;
      }
    if (!d->handle)
      return MULIB_DYNLOAD_OK;
#if defined(_WIN32)
    if (d->owned && !FreeLibrary ((HMODULE)d->handle))
      {
        mulib_dynload_winerror_ (error, cap);
        return MULIB_DYNLOAD_EIO;
      }
#elif defined(__unix__) || defined(__APPLE__)
  if (d->owned && dlclose (d->handle) != 0)
    {
      const char *message = dlerror ();
      mulib_dynload_message_ (error, cap,
                              message ? message : "dlclose failed");
      return MULIB_DYNLOAD_EIO;
    }
#else
  mulib_dynload_message_ (error, cap, "dynamic loading unsupported");
  return MULIB_DYNLOAD_ENOTSUP;
#endif
    d->handle = NULL;
    d->owned = 0;
    return MULIB_DYNLOAD_OK;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
