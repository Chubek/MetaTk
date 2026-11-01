/** @file mulib-minincat.h
 * @brief Allocation-free, blocking, binary one-way stream relay with explicit
 * IO.
 */
#ifndef MULIB_MININCAT_H
#define MULIB_MININCAT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Return 1 and a count <= capacity; count zero is EOF; otherwise
   * error. Buffer is borrowed and the callback must never write beyond
   * capacity.
   */
  typedef int (*mulib_minincat_read_fn) (void *user, void *buffer,
                                         size_t capacity, size_t *count);
  /** @brief Return 1 and a positive count <= length (partial writes
   * supported). Data is borrowed until return. Zero progress is an IO error.
   */
  typedef int (*mulib_minincat_write_fn) (void *user, const void *data,
                                          size_t length, size_t *count);
  /** @brief LIMIT is a successful bounded stop, not an IO failure. */
  typedef enum mulib_minincat_result
  {
    MULIB_MININCAT_OK = 1,
    MULIB_MININCAT_LIMIT = 2,
    MULIB_MININCAT_INVALID = -1,
    MULIB_MININCAT_IO = -2
  } mulib_minincat_result;
  /** @brief Relay until EOF/error or max_bytes have been written. No
   * allocation, lookahead, flushing, closing or sockets. A zero budget returns
   * LIMIT without IO. transferred (required) records successfully written
   * bytes even on error. EOF exactly at the budget still returns LIMIT,
   * because no extra read occurs.
   */
  mulib_minincat_result
  mulib_minincat_relay (mulib_minincat_read_fn read_fn, void *reader,
                        mulib_minincat_write_fn write_fn, void *writer,
                        void *buffer, size_t capacity, size_t max_bytes,
                        size_t *transferred);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MININCAT) && !defined(MULIB_MININCAT_IMPLEMENTED)
#define MULIB_MININCAT_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_minincat_result
  mulib_minincat_relay (mulib_minincat_read_fn read_fn, void *reader,
                        mulib_minincat_write_fn write_fn, void *writer,
                        void *buffer, size_t capacity, size_t max_bytes,
                        size_t *transferred)
  {
    size_t total = 0;
    if (transferred)
      *transferred = 0;
    if (!read_fn || !write_fn || !buffer || !capacity || !transferred)
      return MULIB_MININCAT_INVALID;
    while (total < max_bytes)
      {
        size_t n = 0, done = 0, want = max_bytes - total;
        if (want > capacity)
          want = capacity;
        if (read_fn (reader, buffer, want, &n) != 1 || n > want)
          return MULIB_MININCAT_IO;
        if (!n)
          return MULIB_MININCAT_OK;
        while (done < n)
          {
            size_t written = 0;
            if (write_fn (writer, (unsigned char *)buffer + done, n - done,
                          &written)
                    != 1
                || !written || written > n - done)
              return MULIB_MININCAT_IO;
            done += written;
            total += written;
            *transferred = total;
          }
      }
    return MULIB_MININCAT_LIMIT;
  }
#ifdef __cplusplus
}
#endif
#endif
