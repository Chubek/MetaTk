/**
 * @file mulib-termutils.h
 * @brief POSIX raw-terminal lifetime, window size and complete-write helpers.
 * Define MULIB_IMPL_TERMUTILS once. No process-global terminal state.
 */
#ifndef MULIB_TERMUTILS_H
#define MULIB_TERMUTILS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_TERMUTILS_OK = 0,
    MULIB_TERMUTILS_EINVAL = -1,
    MULIB_TERMUTILS_EIO = -2,
    MULIB_TERMUTILS_ENOTSUP = -3,
    MULIB_TERMUTILS_ESTATE = -4,
    MULIB_TERMUTILS_NOMEM = -5
  };
  /** @brief Owned saved attributes, borrowed fd; initialize and never copy. */
  typedef struct mulib_termutils_raw
  {
    int fd;
    void *saved;
  } mulib_termutils_raw_t;
#define MULIB_TERMUTILS_RAW_INIT { -1, NULL }
  /** @brief Enter raw 8-bit mode, VMIN=1/VTIME=0; unchanged on error. fd must
   * stay open. */
  int mulib_termutils_raw_begin (mulib_termutils_raw_t *state, int fd);
  /** @brief Restore/free saved attributes, idempotently; retain state on
   * failure for retry. */
  int mulib_termutils_raw_end (mulib_termutils_raw_t *state);
  /** @brief Query columns/rows with TIOCGWINSZ; outputs unchanged on failure.
   */
  int mulib_termutils_size (int fd, unsigned *columns, unsigned *rows);
  /** @brief Write all bytes, retrying EINTR. On error written reports partial
   * progress. Nonblocking EAGAIN is EIO (errno retained); caller may resume at
   * written.
   */
  int mulib_termutils_write (int fd, const void *bytes, size_t length,
                             size_t *written);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TERMUTILS) && !defined(MULIB_TERMUTILS_IMPLEMENTED)
#define MULIB_TERMUTILS_IMPLEMENTED
#include <errno.h>
#include <stdlib.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_termutils_raw_begin (mulib_termutils_raw_t *s, int fd)
  {
    if (!s || fd < 0)
      return MULIB_TERMUTILS_EINVAL;
    if (s->saved || s->fd != -1)
      return MULIB_TERMUTILS_ESTATE;
#if defined(__unix__) || defined(__APPLE__)
    {
      struct termios *saved = (struct termios *)malloc (sizeof (*saved)), raw;
      int rc, error;
      if (!saved)
        return MULIB_TERMUTILS_NOMEM;
      if (tcgetattr (fd, saved) < 0)
        {
          error = errno;
          free (saved);
          errno = error;
          return MULIB_TERMUTILS_EIO;
        }
      raw = *saved;
      raw.c_iflag &= (tcflag_t) ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR
                                  | IGNCR | ICRNL | IXON);
      raw.c_oflag &= (tcflag_t)~OPOST;
      raw.c_lflag &= (tcflag_t) ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
      raw.c_cflag = (raw.c_cflag & (tcflag_t) ~(CSIZE | PARENB)) | CS8;
      raw.c_cc[VMIN] = 1;
      raw.c_cc[VTIME] = 0;
      do
        {
          rc = tcsetattr (fd, TCSANOW, &raw);
        }
      while (rc < 0 && errno == EINTR);
      if (rc < 0)
        {
          error = errno;
          free (saved);
          errno = error;
          return MULIB_TERMUTILS_EIO;
        }
      s->fd = fd;
      s->saved = saved;
      return MULIB_TERMUTILS_OK;
    }
#else
  return MULIB_TERMUTILS_ENOTSUP;
#endif
  }
  int
  mulib_termutils_raw_end (mulib_termutils_raw_t *s)
  {
    if (!s)
      return MULIB_TERMUTILS_EINVAL;
    if (!s->saved)
      return MULIB_TERMUTILS_OK;
#if defined(__unix__) || defined(__APPLE__)
    {
      int rc;
      do
        {
          rc = tcsetattr (s->fd, TCSANOW, (const struct termios *)s->saved);
        }
      while (rc < 0 && errno == EINTR);
      if (rc < 0)
        return MULIB_TERMUTILS_EIO;
      free (s->saved);
      s->saved = NULL;
      s->fd = -1;
      return MULIB_TERMUTILS_OK;
    }
#else
  return MULIB_TERMUTILS_ENOTSUP;
#endif
  }
  int
  mulib_termutils_size (int fd, unsigned *columns, unsigned *rows)
  {
    if (fd < 0 || !columns || !rows)
      return MULIB_TERMUTILS_EINVAL;
#if defined(__unix__) || defined(__APPLE__)
    {
      struct winsize size;
      int rc;
      do
        {
          rc = ioctl (fd, TIOCGWINSZ, &size);
        }
      while (rc < 0 && errno == EINTR);
      if (rc < 0)
        return MULIB_TERMUTILS_EIO;
      *columns = size.ws_col;
      *rows = size.ws_row;
      return MULIB_TERMUTILS_OK;
    }
#else
  return MULIB_TERMUTILS_ENOTSUP;
#endif
  }
  int
  mulib_termutils_write (int fd, const void *bytes, size_t n, size_t *written)
  {
    if (written)
      *written = 0;
    if (fd < 0 || (!bytes && n))
      return MULIB_TERMUTILS_EINVAL;
#if defined(__unix__) || defined(__APPLE__)
    {
      size_t pos = 0;
      while (pos < n)
        {
          size_t chunk = n - pos > 0x7fffffffu ? 0x7fffffffu : n - pos;
          ssize_t rc = write (fd, (const unsigned char *)bytes + pos, chunk);
          if (rc < 0 && errno == EINTR)
            continue;
          if (rc <= 0)
            {
              if (!rc)
                errno = EIO;
              if (written)
                *written = pos;
              return MULIB_TERMUTILS_EIO;
            }
          pos += (size_t)rc;
        }
      if (written)
        *written = pos;
      return MULIB_TERMUTILS_OK;
    }
#else
  return MULIB_TERMUTILS_ENOTSUP;
#endif
  }
#ifdef __cplusplus
}
#endif
#endif
