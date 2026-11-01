/**
 * @file mulib-procfs.h
 * @brief Bounded Linux /proc process stat/status parsing and VFS/native
 * queries.
 * @details The stat parser locates the final closing parenthesis, not the
 * first, so comm may contain spaces and parentheses. Native queries are
 * Linux-only; pure parsing and VFS fixtures are portable. Define
 * MULIB_IMPL_PROCFS and separately MULIB_IMPL_VFS. No feature-test macros are
 * defined here.
 */
#ifndef MULIB_PROCFS_H
#define MULIB_PROCFS_H
#include "mulib-vfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_PROCFS_STAT_LIMIT = 16384,
    MULIB_PROCFS_STATUS_LIMIT = 65536,
    MULIB_PROCFS_COMM_CAP = 256
  };
  typedef struct mulib_procfs_stat
  {
    uint32_t pid, ppid;
    char comm[MULIB_PROCFS_COMM_CAP], state;
    uint64_t user_ticks, system_ticks, start_ticks, virtual_bytes;
    int64_t resident_pages;
  } mulib_procfs_stat;
  enum
  {
    MULIB_PROCFS_HAS_NAME = 1,
    MULIB_PROCFS_HAS_PID = 2,
    MULIB_PROCFS_HAS_PPID = 4,
    MULIB_PROCFS_HAS_UID = 8,
    MULIB_PROCFS_HAS_GID = 16,
    MULIB_PROCFS_HAS_THREADS = 32,
    MULIB_PROCFS_HAS_RSS = 64
  };
  typedef struct mulib_procfs_status
  {
    char name[MULIB_PROCFS_COMM_CAP];
    uint32_t pid, ppid, uid[4], gid[4], threads;
    uint64_t resident_kib;
    unsigned present;
  } mulib_procfs_status;
  typedef struct mulib_procfs_process
  {
    mulib_procfs_stat stat;
    mulib_procfs_status status;
  } mulib_procfs_process;
  /** @brief Parse non-NUL-terminated stat bytes, fields 1..24 required; later
   * numeric fields accepted. Length/comm/range overflow gives ELIMIT. Outputs
   * are unchanged on failure. Tick/page values are not converted to
   * time/bytes.
   */
  mulib_vfs_result mulib_procfs_parse_stat (const char *text, size_t length,
                                            mulib_procfs_stat *out);
  /** @brief Parse status lines. Name/Pid/PPid required; recognized duplicate
   * or malformed fields fail. Unknown colon-separated fields are ignored.
   * Optional values have presence bits; VmRSS must have the exact Linux unit
   * kB.
   */
  mulib_vfs_result mulib_procfs_parse_status (const char *text, size_t length,
                                              mulib_procfs_status *out);
  /** @brief Read root/<pid>/{stat,status} through borrowed VFS and parse both.
   * Output changes only on success; this is not an atomic process snapshot.
   */
  mulib_vfs_result mulib_procfs_query (mulib_vfs *vfs, const char *root,
                                       uint32_t pid,
                                       mulib_procfs_process *out);
  /** @brief Query native /proc on Linux; return ENOTSUP on other platforms. */
  mulib_vfs_result mulib_procfs_query_native (uint32_t pid,
                                              mulib_procfs_process *out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_PROCFS) && !defined(MULIB_PROCFS_IMPLEMENTED)
#define MULIB_PROCFS_IMPLEMENTED
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_procfs_space_ (char c)
  {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v'
           || c == '\f';
  }
  static mulib_vfs_result
  mulib_procfs_number_ (const char **cursor, const char *end, uint64_t *value,
                        int *negative)
  {
    const char *p = *cursor;
    uint64_t n = 0;
    int digits = 0;
    *negative = 0;
    while (p < end && mulib_procfs_space_ (*p))
      ++p;
    if (p < end && (*p == '-' || *p == '+'))
      {
        *negative = *p == '-';
        ++p;
      }
    while (p < end && *p >= '0' && *p <= '9')
      {
        unsigned digit = (unsigned)(*p++ - '0');
        if (n > (UINT64_MAX - digit) / 10)
          return MULIB_VFS_ELIMIT;
        n = n * 10 + digit;
        digits = 1;
      }
    if (!digits || (p < end && !mulib_procfs_space_ (*p)))
      return MULIB_VFS_EFORMAT;
    if (*negative && n > (uint64_t)INT64_MAX + 1)
      return MULIB_VFS_ELIMIT;
    *cursor = p;
    *value = n;
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_procfs_parse_stat (const char *text, size_t length,
                           mulib_procfs_stat *out)
  {
    mulib_procfs_stat temp;
    const char *p, *end, *open, *close = NULL, *q;
    uint64_t n;
    int negative;
    size_t field = 4, comm_length;
    mulib_vfs_result r;
    if (!text || !out)
      return MULIB_VFS_EINVAL;
    if (length > MULIB_PROCFS_STAT_LIMIT)
      return MULIB_VFS_ELIMIT;
    if (!length || memchr (text, 0, length))
      return MULIB_VFS_EFORMAT;
    memset (&temp, 0, sizeof (temp));
    p = text;
    end = text + length;
    r = mulib_procfs_number_ (&p, end, &n, &negative);
    if (r)
      return r;
    if (negative || !n)
      return MULIB_VFS_EFORMAT;
    if (n > UINT32_MAX)
      return MULIB_VFS_ELIMIT;
    temp.pid = (uint32_t)n;
    if (p == end || !mulib_procfs_space_ (*p))
      return MULIB_VFS_EFORMAT;
    while (p < end && mulib_procfs_space_ (*p))
      ++p;
    if (p == end || *p != '(')
      return MULIB_VFS_EFORMAT;
    open = p++;
    for (q = p; q < end; ++q)
      if (*q == ')')
        close = q;
    if (!close)
      return MULIB_VFS_EFORMAT;
    comm_length = (size_t)(close - open - 1);
    if (comm_length >= sizeof (temp.comm))
      return MULIB_VFS_ELIMIT;
    memcpy (temp.comm, open + 1, comm_length);
    temp.comm[comm_length] = 0;
    p = close + 1;
    if (p == end || !mulib_procfs_space_ (*p))
      return MULIB_VFS_EFORMAT;
    while (p < end && mulib_procfs_space_ (*p))
      ++p;
    if (p == end || !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')))
      return MULIB_VFS_EFORMAT;
    temp.state = *p++;
    if (p == end || !mulib_procfs_space_ (*p))
      return MULIB_VFS_EFORMAT;
    for (;;)
      {
        while (p < end && mulib_procfs_space_ (*p))
          ++p;
        if (p == end)
          break;
        r = mulib_procfs_number_ (&p, end, &n, &negative);
        if (r)
          return r;
        if (((field >= 9 && field <= 15) || field == 22 || field == 23)
            && negative)
          return MULIB_VFS_EFORMAT;
        if (((field >= 5 && field <= 8) || (field >= 16 && field <= 21))
            && !negative && n > INT64_MAX)
          return MULIB_VFS_ELIMIT;
        if (field == 4)
          {
            if (negative || n > UINT32_MAX)
              return MULIB_VFS_ELIMIT;
            temp.ppid = (uint32_t)n;
          }
        else if (field == 14 || field == 15 || field == 22 || field == 23)
          {
            if (negative)
              return MULIB_VFS_EFORMAT;
            if (field == 14)
              temp.user_ticks = n;
            if (field == 15)
              temp.system_ticks = n;
            if (field == 22)
              temp.start_ticks = n;
            if (field == 23)
              temp.virtual_bytes = n;
          }
        else if (field == 24)
          {
            if (!negative && n > INT64_MAX)
              return MULIB_VFS_ELIMIT;
            temp.resident_pages
                = negative ? (n == (uint64_t)INT64_MAX + 1 ? INT64_MIN
                                                           : -(int64_t)n)
                           : (int64_t)n;
          }
        ++field;
      }
    if (field <= 24)
      return MULIB_VFS_EFORMAT;
    *out = temp;
    return MULIB_VFS_OK;
  }
  static int
  mulib_procfs_key_ (const char *start, const char *end, const char *key)
  {
    size_t n = strlen (key);
    return (size_t)(end - start) == n && !memcmp (start, key, n);
  }
  static mulib_vfs_result
  mulib_procfs_unsigned_ (const char **p, const char *end, uint64_t *n)
  {
    int negative;
    mulib_vfs_result r = mulib_procfs_number_ (p, end, n, &negative);
    return !r && negative ? MULIB_VFS_EFORMAT : r;
  }
  mulib_vfs_result
  mulib_procfs_parse_status (const char *text, size_t length,
                             mulib_procfs_status *out)
  {
    mulib_procfs_status temp;
    const char *p, *end;
    if (!text || !out)
      return MULIB_VFS_EINVAL;
    if (length > MULIB_PROCFS_STATUS_LIMIT)
      return MULIB_VFS_ELIMIT;
    if (!length || memchr (text, 0, length))
      return MULIB_VFS_EFORMAT;
    memset (&temp, 0, sizeof (temp));
    p = text;
    end = text + length;
    while (p < end)
      {
        const char *line = p, *last, *colon, *value;
        uint64_t n = 0;
        unsigned bit = 0;
        mulib_vfs_result r = MULIB_VFS_OK;
        while (p < end && *p != '\n')
          ++p;
        last = p;
        if (p < end)
          ++p;
        if (last == line)
          continue;
        colon = (const char *)memchr (line, ':', (size_t)(last - line));
        if (!colon || colon == line)
          return MULIB_VFS_EFORMAT;
        value = colon + 1;
        while (value < last && mulib_procfs_space_ (*value))
          ++value;
        while (last > value && mulib_procfs_space_ (last[-1]))
          --last;
        if (mulib_procfs_key_ (line, colon, "Name"))
          bit = MULIB_PROCFS_HAS_NAME;
        else if (mulib_procfs_key_ (line, colon, "Pid"))
          bit = MULIB_PROCFS_HAS_PID;
        else if (mulib_procfs_key_ (line, colon, "PPid"))
          bit = MULIB_PROCFS_HAS_PPID;
        else if (mulib_procfs_key_ (line, colon, "Uid"))
          bit = MULIB_PROCFS_HAS_UID;
        else if (mulib_procfs_key_ (line, colon, "Gid"))
          bit = MULIB_PROCFS_HAS_GID;
        else if (mulib_procfs_key_ (line, colon, "Threads"))
          bit = MULIB_PROCFS_HAS_THREADS;
        else if (mulib_procfs_key_ (line, colon, "VmRSS"))
          bit = MULIB_PROCFS_HAS_RSS;
        if (!bit)
          continue;
        if (temp.present & bit)
          return MULIB_VFS_EFORMAT;
        temp.present |= bit;
        if (bit == MULIB_PROCFS_HAS_NAME)
          {
            size_t nbytes = (size_t)(last - value);
            if (nbytes >= sizeof (temp.name))
              return MULIB_VFS_ELIMIT;
            memcpy (temp.name, value, nbytes);
            temp.name[nbytes] = 0;
            continue;
          }
        if (bit == MULIB_PROCFS_HAS_UID || bit == MULIB_PROCFS_HAS_GID)
          {
            size_t i;
            uint32_t *ids = bit == MULIB_PROCFS_HAS_UID ? temp.uid : temp.gid;
            for (i = 0; i < 4; ++i)
              {
                r = mulib_procfs_unsigned_ (&value, last, &n);
                if (r)
                  return r;
                if (n > UINT32_MAX)
                  return MULIB_VFS_ELIMIT;
                ids[i] = (uint32_t)n;
              }
          }
        else
          {
            r = mulib_procfs_unsigned_ (&value, last, &n);
            if (r)
              return r;
            if (bit == MULIB_PROCFS_HAS_RSS)
              {
                while (value < last && mulib_procfs_space_ (*value))
                  ++value;
                if ((size_t)(last - value) != 2 || memcmp (value, "kB", 2))
                  return MULIB_VFS_EFORMAT;
                temp.resident_kib = n;
                value = last;
              }
            else
              {
                if (n > UINT32_MAX)
                  return MULIB_VFS_ELIMIT;
                if (bit == MULIB_PROCFS_HAS_PID)
                  {
                    if (!n)
                      return MULIB_VFS_EFORMAT;
                    temp.pid = (uint32_t)n;
                  }
                if (bit == MULIB_PROCFS_HAS_PPID)
                  temp.ppid = (uint32_t)n;
                if (bit == MULIB_PROCFS_HAS_THREADS)
                  {
                    if (!n)
                      return MULIB_VFS_EFORMAT;
                    temp.threads = (uint32_t)n;
                  }
              }
          }
        while (value < last && mulib_procfs_space_ (*value))
          ++value;
        if (value != last)
          return MULIB_VFS_EFORMAT;
      }
    if ((temp.present & 7) != 7)
      return MULIB_VFS_EFORMAT;
    *out = temp;
    return MULIB_VFS_OK;
  }
  static mulib_vfs_result
  mulib_procfs_read_ (mulib_vfs *vfs, const char *path, char *buffer,
                      size_t limit, size_t *length)
  {
    size_t got = 0, more = 0;
    char extra;
    mulib_vfs_result r;
    if (vfs)
      {
        r = mulib_vfs_read (vfs, path, 0, buffer, limit, &got);
        if (!r && got == limit)
          r = mulib_vfs_read (vfs, path, got, &extra, 1, &more);
      }
    else
      {
#if defined(__linux__)
        FILE *file = fopen (path, "rb");
        if (!file)
          return errno == ENOENT ? MULIB_VFS_ENOENT : MULIB_VFS_EIO;
        got = fread (buffer, 1, limit, file);
        if (got == limit)
          {
            int c = fgetc (file);
            more = c == EOF ? 0 : 1;
          }
        r = ferror (file) ? MULIB_VFS_EIO : MULIB_VFS_OK;
        if (fclose (file) && !r)
          r = MULIB_VFS_EIO;
#else
      (void)path;
      (void)buffer;
      (void)limit;
      (void)extra;
      r = MULIB_VFS_ENOTSUP;
#endif
      }
    if (!r && more)
      r = MULIB_VFS_ELIMIT;
    if (!r)
      *length = got;
    return r;
  }
  static mulib_vfs_result
  mulib_procfs_query_ (mulib_vfs *vfs, const char *root, uint32_t pid,
                       mulib_procfs_process *out)
  {
    char path[MULIB_VFS_PATH_CAP], normalized[MULIB_VFS_PATH_CAP];
    char *buffer;
    size_t n;
    int path_length;
    mulib_procfs_process temp;
    mulib_vfs_result r;
    if (!out || !pid)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_normalize (root, normalized, sizeof (normalized));
    if (r)
      return r;
    buffer = (char *)malloc (MULIB_PROCFS_STATUS_LIMIT);
    if (!buffer)
      return MULIB_VFS_ENOMEM;
    path_length
        = snprintf (path, sizeof (path), "%s%s%" PRIu32 "/stat", normalized,
                    strcmp (normalized, "/") ? "/" : "", pid);
    if (path_length < 0 || (size_t)path_length >= sizeof (path))
      {
        free (buffer);
        return MULIB_VFS_ELIMIT;
      }
    r = mulib_procfs_read_ (vfs, path, buffer, MULIB_PROCFS_STAT_LIMIT, &n);
    if (!r)
      r = mulib_procfs_parse_stat (buffer, n, &temp.stat);
    if (!r)
      {
        path_length
            = snprintf (path, sizeof (path), "%s%s%" PRIu32 "/status",
                        normalized, strcmp (normalized, "/") ? "/" : "", pid);
        if (path_length < 0 || (size_t)path_length >= sizeof (path))
          r = MULIB_VFS_ELIMIT;
      }
    if (!r)
      r = mulib_procfs_read_ (vfs, path, buffer, MULIB_PROCFS_STATUS_LIMIT,
                              &n);
    if (!r)
      r = mulib_procfs_parse_status (buffer, n, &temp.status);
    if (!r && (temp.stat.pid != pid || temp.status.pid != pid))
      r = MULIB_VFS_EFORMAT;
    free (buffer);
    if (!r)
      *out = temp;
    return r;
  }
  mulib_vfs_result
  mulib_procfs_query (mulib_vfs *vfs, const char *root, uint32_t pid,
                      mulib_procfs_process *out)
  {
    if (!vfs)
      return MULIB_VFS_EINVAL;
    return mulib_procfs_query_ (vfs, root, pid, out);
  }
  mulib_vfs_result
  mulib_procfs_query_native (uint32_t pid, mulib_procfs_process *out)
  {
#if defined(__linux__)
    return mulib_procfs_query_ (NULL, "/proc", pid, out);
#else
  (void)pid;
  (void)out;
  return MULIB_VFS_ENOTSUP;
#endif
  }
#ifdef __cplusplus
}
#endif
#endif
