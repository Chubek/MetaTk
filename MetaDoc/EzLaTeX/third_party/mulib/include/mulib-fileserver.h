/** @file mulib-fileserver.h
 * @brief Strict root-relative static file mapping and bounded HTTP responses.
 * The portable loader is explicit; optional POSIX openat loader denies
 * symlinks.
 */
#ifndef MULIB_FILESERVER_H
#define MULIB_FILESERVER_H
#include "mulib-webframework.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Loader result, mapped to 200/404/403/413/500 responses. */
  typedef enum mulib_fileserver_result
  {
    MULIB_FILESERVER_FOUND = 1,
    MULIB_FILESERVER_NOT_FOUND = 0,
    MULIB_FILESERVER_FORBIDDEN = -1,
    MULIB_FILESERVER_LIMIT = -2,
    MULIB_FILESERVER_IO = -3,
    MULIB_FILESERVER_NOMEM = -4
  } mulib_fileserver_result;
  /** @brief Load a sanitized relative path beneath a trusted root. The loader
   * MUST reject symlinks in all components and non-regular files, and enforce
   * max. HEAD sets metadata_only: return length but no data. GET returns
   * malloc-owned data of length bytes. Any returned data transfers to the
   * server on all results. user/path are borrowed. Do not decode the path
   * again.
   */
  typedef mulib_fileserver_result (*mulib_fileserver_load_fn) (
      void *user, const char *path, size_t max_bytes, int metadata_only,
      unsigned char **data, size_t *length);
  /** @brief Zero max selects 16 MiB; NULL index selects index.html; NULL type
   * selects application/octet-stream. Index must be a single safe non-hidden
   * component.
   */
  typedef struct mulib_fileserver_options
  {
    size_t max_bytes;
    const char *index_name, *content_type;
  } mulib_fileserver_options_t;
  /** @brief Map origin target to malloc-owned relative path (output unchanged
   * on failure). Strip query; reject traversal, dotfiles, encoded separators,
   * percent, backslashes, repeated slashes and all filename bytes outside
   * [A-Za-z0-9_.-]. Trailing slash appends index_name (NULL selects
   * index.html).
   */
  mulib_http_result mulib_fileserver_path (const char *target,
                                           const char *index_name,
                                           char **path);
  /** @brief Serve GET/HEAD. Malformed targets become 403, other methods 405.
   * Response is owned and unchanged on library failure. Loader failures become
   * empty status responses; allocation failure remains MULIB_HTTP_NOMEM.
   */
  mulib_http_result
  mulib_fileserver_serve (const mulib_http_message_t *request,
                          const mulib_fileserver_options_t *options,
                          mulib_fileserver_load_fn load, void *user,
                          mulib_http_message_t *response);
#if defined(MULIB_FILESERVER_POSIX)
  /** @brief POSIX.1-2008 loader. user points to a borrowed int root directory
   * fd. Define _POSIX_C_SOURCE=200809L before system headers and enable
   * MULIB_FILESERVER_POSIX alongside MULIB_IMPL_FILESERVER. Uses
   * openat/O_NOFOLLOW for every component. Root fd must already refer to a
   * trusted directory.
   */
  mulib_fileserver_result
  mulib_fileserver_posix_load (void *user, const char *path, size_t max_bytes,
                               int metadata_only, unsigned char **data,
                               size_t *length);
#endif
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FILESERVER) && !defined(MULIB_FILESERVER_IMPLEMENTED)
#define MULIB_FILESERVER_IMPLEMENTED
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(MULIB_FILESERVER_POSIX)
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_fileserver_byte_ (unsigned char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
           || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
  }
  static int
  mulib_fileserver_component_ (const char *s, size_t n)
  {
    if (!n || s[0] == '.')
      return 0;
    for (size_t i = 0; i < n; ++i)
      if (!mulib_fileserver_byte_ ((unsigned char)s[i]))
        return 0;
    return 1;
  }
  static int
  mulib_fileserver_hex_ (unsigned char c)
  {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  }
  mulib_http_result
  mulib_fileserver_path (const char *target, const char *index, char **out)
  {
    mulib_uri_t uri;
    mulib_strbuf_t path;
    size_t component = 0;
    if (!target || !out)
      return MULIB_HTTP_INVALID;
    if (!index)
      index = "index.html";
    if (strlen (target) > 32768 || strlen (index) > 32768)
      return MULIB_HTTP_LIMIT;
    if (!mulib_fileserver_component_ (index, strlen (index))
        || target[0] != '/' || !mulib_uri_parse (target, strlen (target), &uri)
        || uri.has_authority || uri.scheme.length || uri.has_fragment)
      return MULIB_HTTP_INVALID;
    mulib_strbuf_init (&path);
    for (size_t i = 1; i < uri.path.length; ++i)
      {
        unsigned char c = (unsigned char)uri.path.data[i];
        if (c == '/')
          {
            if (!mulib_fileserver_component_ (path.data + component,
                                              path.length - component))
              goto invalid;
            if (!mulib_strbuf_putc (&path, '/'))
              goto memory;
            component = path.length;
          }
        else
          {
            if (c == '%')
              {
                int a, b;
                if (uri.path.length - i < 3
                    || (a = mulib_fileserver_hex_ (
                            (unsigned char)uri.path.data[i + 1]))
                           < 0
                    || (b = mulib_fileserver_hex_ (
                            (unsigned char)uri.path.data[i + 2]))
                           < 0)
                  goto invalid;
                c = (unsigned char)(a * 16 + b);
                i += 2;
              }
            if (!mulib_fileserver_byte_ (c))
              goto invalid;
            if (!mulib_strbuf_putc (&path, (char)c))
              goto memory;
          }
      }
    if (!path.length || path.data[path.length - 1] == '/')
      {
        if (!mulib_strbuf_append (&path, index))
          goto memory;
      }
    else if (!mulib_fileserver_component_ (path.data + component,
                                           path.length - component))
      goto invalid;
    *out = path.data;
    return MULIB_HTTP_OK;
  invalid:
    mulib_strbuf_free (&path);
    return MULIB_HTTP_INVALID;
  memory:
    mulib_strbuf_free (&path);
    return MULIB_HTTP_NOMEM;
  }
  mulib_http_result
  mulib_fileserver_serve (const mulib_http_message_t *request,
                          const mulib_fileserver_options_t *options,
                          mulib_fileserver_load_fn load, void *user,
                          mulib_http_message_t *out)
  {
    char *path = NULL;
    unsigned char *data = NULL, *wire = NULL;
    size_t length = 0, n = 0, used = 0,
           max = options && options->max_bytes ? options->max_bytes
                                               : 16u * 1024u * 1024u;
    int head;
    unsigned status = 200;
    mulib_fileserver_result loaded;
    mulib_http_result rc;
    mulib_http_message_t m;
    mulib_http_header_t fields[3];
    mulib_http_options_t limits = { 0, 0, 0, 0 };
    char number[32];
    if (!request || !out || !load || request->is_response || !request->method
        || !request->target)
      return MULIB_HTTP_INVALID;
    rc = mulib_http_serialize (request, NULL, &wire, &n);
    free (wire);
    wire = NULL;
    if (rc != MULIB_HTTP_OK)
      return rc;
    head = !strcmp (request->method, "HEAD");
    if (!head && strcmp (request->method, "GET"))
      {
        /* Include the required Allow field for a method rejection. */
        memset (&m, 0, sizeof (m));
        m.is_response = 1;
        m.status = 405;
        m.reason = (char *)"Method Not Allowed";
        fields[0].name = (char *)"Allow";
        fields[0].value = (char *)"GET, HEAD";
        m.headers = fields;
        m.header_count = 1;
        rc = mulib_http_serialize (&m, NULL, &wire, &n);
        limits.flags = MULIB_HTTP_EOF;
        if (rc == MULIB_HTTP_OK)
          rc = mulib_http_parse (wire, n, 1, &limits, out, &used);
        free (wire);
        return rc;
      }
    rc = mulib_fileserver_path (request->target,
                                options ? options->index_name : NULL, &path);
    if (rc == MULIB_HTTP_NOMEM)
      return rc;
    if (rc != MULIB_HTTP_OK)
      return mulib_webframework_reply (out, 403, NULL, NULL, 0);
    loaded = load (user, path, max, head, &data, &length);
    free (path);
    if (loaded == MULIB_FILESERVER_NOMEM)
      {
        free (data);
        return MULIB_HTTP_NOMEM;
      }
    if (loaded == MULIB_FILESERVER_NOT_FOUND)
      status = 404;
    else if (loaded == MULIB_FILESERVER_FORBIDDEN)
      status = 403;
    else if (loaded == MULIB_FILESERVER_LIMIT
             || (loaded == MULIB_FILESERVER_FOUND && length > max))
      status = 413;
    else if (loaded != MULIB_FILESERVER_FOUND || (!head && length && !data))
      status = 500;
    if (status != 200)
      {
        free (data);
        return mulib_webframework_reply (out, status, NULL, NULL, 0);
      }
    memset (&m, 0, sizeof (m));
    m.is_response = 1;
    m.status = 200;
    m.reason = (char *)"OK";
    (void)snprintf (number, sizeof (number), "%zu", length);
    fields[0].name = (char *)"Content-Length";
    fields[0].value = number;
    fields[1].name = (char *)"Content-Type";
    fields[1].value = (char *)(options && options->content_type
                                   ? options->content_type
                                   : "application/octet-stream");
    fields[2].name = (char *)"X-Content-Type-Options";
    fields[2].value = (char *)"nosniff";
    m.headers = fields;
    m.header_count = 3;
    m.body = head ? NULL : data;
    m.body_length = head ? 0 : length;
    limits.max_body_bytes = max;
    limits.flags = head ? MULIB_HTTP_HEAD_RESPONSE : 0;
    rc = mulib_http_serialize (&m, &limits, &wire, &n);
    limits.flags |= MULIB_HTTP_EOF;
    if (rc == MULIB_HTTP_OK)
      rc = mulib_http_parse (wire, n, 1, &limits, out, &used);
    free (wire);
    free (data);
    return rc;
  }
#if defined(MULIB_FILESERVER_POSIX)
  mulib_fileserver_result
  mulib_fileserver_posix_load (void *user, const char *path, size_t max,
                               int head, unsigned char **data, size_t *length)
  {
    int dir, file = -1;
    struct stat st;
    char *copy, *part, *slash;
    mulib_fileserver_result result = MULIB_FILESERVER_IO;
    mulib_buffer_t buffer;
    unsigned char block[4096];
    size_t npath;
    if (!user || !path || !data || !length || !*path)
      return result;
    npath = strlen (path);
    if (npath == SIZE_MAX)
      return result;
    copy = (char *)malloc (npath + 1);
    if (!copy)
      return MULIB_FILESERVER_NOMEM;
    memcpy (copy, path, npath + 1);
    part = copy;
    dir = dup (*(int *)user);
    mulib_buffer_init (&buffer);
    if (dir < 0)
      goto done;
    for (;;)
      {
        slash = strchr (part, '/');
        if (slash)
          *slash = 0;
        if (!mulib_fileserver_component_ (part, strlen (part)))
          {
            result = MULIB_FILESERVER_FORBIDDEN;
            goto done;
          }
        /* O_NONBLOCK prevents a malicious FIFO from blocking before fstat. */
        file = openat (dir, part,
                       O_RDONLY | O_NOFOLLOW | O_NONBLOCK
                           | (slash ? O_DIRECTORY : 0));
        if (file < 0)
          {
            if (errno == ENOENT)
              result = MULIB_FILESERVER_NOT_FOUND;
            else if (errno == ELOOP || errno == ENOTDIR || errno == EACCES)
              result = MULIB_FILESERVER_FORBIDDEN;
            goto done;
          }
        close (dir);
        dir = -1;
        if (!slash)
          break;
        dir = file;
        file = -1;
        part = slash + 1;
      }
    if (fstat (file, &st) != 0)
      goto done;
    if (!S_ISREG (st.st_mode))
      {
        result = MULIB_FILESERVER_FORBIDDEN;
        goto done;
      }
    if (st.st_size < 0 || (uintmax_t)st.st_size > (uintmax_t)max)
      {
        result = MULIB_FILESERVER_LIMIT;
        goto done;
      }
    if (head)
      {
        *length = (size_t)st.st_size;
        *data = NULL;
        result = MULIB_FILESERVER_FOUND;
        goto done;
      }
    for (;;)
      {
        ssize_t got = read (file, block, sizeof (block));
        if (got < 0)
          {
            if (errno == EINTR)
              continue;
            goto done;
          }
        if (!got)
          break;
        if ((size_t)got > max - buffer.length)
          {
            result = MULIB_FILESERVER_LIMIT;
            goto done;
          }
        if (!mulib_buffer_append (&buffer, block, (size_t)got))
          {
            result = MULIB_FILESERVER_NOMEM;
            goto done;
          }
      }
    *data = buffer.data;
    *length = buffer.length;
    memset (&buffer, 0, sizeof (buffer));
    result = MULIB_FILESERVER_FOUND;
  done:
    if (file >= 0)
      close (file);
    if (dir >= 0)
      close (dir);
    free (copy);
    mulib_buffer_free (&buffer);
    return result;
  }
#endif
#ifdef __cplusplus
}
#endif
#endif
