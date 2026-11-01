/**
 * @file mulib-vfs.h
 * @brief Bounded absolute-path routing and an owned in-memory filesystem.
 * @details Define MULIB_IMPL_VFS once. No dependency implementation switches
 * are enabled. Paths are lexical, slash-separated byte strings, not native
 * paths. See manual/mulib-vfs.md for lifetime, error and callback contracts.
 */
#ifndef MULIB_VFS_H
#define MULIB_VFS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_VFS_PATH_CAP = 4096
  };
  typedef enum mulib_vfs_result
  {
    MULIB_VFS_OK = 0,
    MULIB_VFS_EINVAL = -1,
    MULIB_VFS_ENOENT = -2,
    MULIB_VFS_ENOMEM = -3,
    MULIB_VFS_ELIMIT = -4,
    MULIB_VFS_EEXIST = -5,
    MULIB_VFS_ENOTDIR = -6,
    MULIB_VFS_EISDIR = -7,
    MULIB_VFS_EROFS = -8,
    MULIB_VFS_EIO = -9,
    MULIB_VFS_ENOTSUP = -10,
    MULIB_VFS_ECANCELED = -11,
    MULIB_VFS_EFORMAT = -12
  } mulib_vfs_result;
  typedef enum mulib_vfs_kind
  {
    MULIB_VFS_FILE = 1,
    MULIB_VFS_DIRECTORY = 2
  } mulib_vfs_kind;
  typedef struct mulib_vfs_stat
  {
    mulib_vfs_kind kind;
    uint64_t size;
  } mulib_vfs_stat;
  /** @brief A borrowed immediate-child name/stat; nonzero return stops
   * listing. */
  typedef int (*mulib_vfs_visit) (void *user, const char *name,
                                  const mulib_vfs_stat *info);
  /** @brief Backend callbacks receive normalized mount-relative absolute
   * paths. Read/write counts start at zero; successful short reads mean
   * end-of-file. A NULL operation returns ENOTSUP (write returns EROFS).
   * Context is borrowed.
   */
  typedef struct mulib_vfs_ops
  {
    mulib_vfs_result (*read) (void *, const char *, size_t, void *, size_t,
                              size_t *);
    mulib_vfs_result (*write) (void *, const char *, size_t, const void *,
                               size_t, size_t *);
    mulib_vfs_result (*stat) (void *, const char *, mulib_vfs_stat *);
    mulib_vfs_result (*list) (void *, const char *, mulib_vfs_visit, void *);
  } mulib_vfs_ops;
  typedef struct mulib_vfs_mount_entry
  {
    char *path;
    mulib_vfs_ops ops;
    void *context;
  } mulib_vfs_mount_entry;
  typedef struct mulib_vfs
  {
    mulib_vfs_mount_entry *mounts;
    size_t count, max_mounts;
  } mulib_vfs;
  /** @brief Normalize an absolute path; reject above-root .., backslashes,
   * control bytes and inputs longer than 4095 bytes. In-place use is
   * supported. Output is unchanged on failure. Capacity includes the
   * terminating NUL.
   */
  mulib_vfs_result mulib_vfs_normalize (const char *path, char *out,
                                        size_t capacity);
  /** @brief Initialize an uninitialized table with a nonzero mount limit. */
  mulib_vfs_result mulib_vfs_init (mulib_vfs *vfs, size_t max_mounts);
  /** @brief Release mount names/table, never backend contexts; reset to zero.
   */
  void mulib_vfs_free (mulib_vfs *vfs);
  /** @brief Copy a mount name and operations; duplicate normalized mounts
   * fail. */
  mulib_vfs_result mulib_vfs_mount (mulib_vfs *vfs, const char *path,
                                    const mulib_vfs_ops *ops, void *context);
  /** @brief Remove an exact normalized mount, without freeing its context. */
  mulib_vfs_result mulib_vfs_unmount (mulib_vfs *vfs, const char *path);
  /** @brief Route to the longest component-prefix mount; buffer is
   * caller-owned. */
  mulib_vfs_result mulib_vfs_read (mulib_vfs *vfs, const char *path,
                                   size_t offset, void *data, size_t capacity,
                                   size_t *read_count);
  /** @brief Offset write, creating a file if the backend supports it; no
   * truncation. */
  mulib_vfs_result mulib_vfs_write (mulib_vfs *vfs, const char *path,
                                    size_t offset, const void *data,
                                    size_t length, size_t *write_count);
  /** @brief Query backend metadata; output changes only on success. */
  mulib_vfs_result mulib_vfs_getstat (mulib_vfs *vfs, const char *path,
                                      mulib_vfs_stat *info);
  /** @brief Enumerate backend children; mounts are not synthesized into
   * listings. */
  mulib_vfs_result mulib_vfs_list (mulib_vfs *vfs, const char *path,
                                   mulib_vfs_visit visit, void *user);
  typedef struct mulib_vfs_mem_node
  {
    char *path;
    unsigned char *data;
    size_t length;
    mulib_vfs_kind kind;
  } mulib_vfs_mem_node;
  typedef struct mulib_vfs_mem
  {
    mulib_vfs_mem_node *nodes;
    size_t count, max_nodes, max_file_bytes;
  } mulib_vfs_mem;
  /** @brief Initialize memory storage; root is implicit and not counted in
   * max_nodes. */
  mulib_vfs_result mulib_vfs_mem_init (mulib_vfs_mem *mem, size_t max_nodes,
                                       size_t max_file_bytes);
  /** @brief Free all owned paths/bytes; does not unmount borrowed references.
   */
  void mulib_vfs_mem_free (mulib_vfs_mem *mem);
  /** @brief Create one directory; its parent must already exist. */
  mulib_vfs_result mulib_vfs_mem_mkdir (mulib_vfs_mem *mem, const char *path);
  /** @brief Copy/replace all file bytes atomically on allocation failure;
   * parent required. */
  mulib_vfs_result mulib_vfs_mem_put (mulib_vfs_mem *mem, const char *path,
                                      const void *data, size_t length);
  /** @brief Remove a file or empty directory; root cannot be removed. */
  mulib_vfs_result mulib_vfs_mem_remove (mulib_vfs_mem *mem, const char *path);
  /** @brief Return the memory backend operations by value; context must be a
   * live mem. */
  mulib_vfs_ops mulib_vfs_mem_ops (void);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_VFS) && !defined(MULIB_VFS_IMPLEMENTED)
#define MULIB_VFS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static char *
  mulib_vfs_copy_ (const char *s)
  {
    size_t n = strlen (s) + 1;
    char *p = (char *)malloc (n);
    if (p)
      memcpy (p, s, n);
    return p;
  }
  mulib_vfs_result
  mulib_vfs_normalize (const char *path, char *out, size_t capacity)
  {
    char temp[MULIB_VFS_PATH_CAP];
    size_t n = 0, i = 1, used = 1;
    if (!path || !out || !capacity || path[0] != '/')
      return MULIB_VFS_EINVAL;
    while (path[n])
      {
        unsigned char c = (unsigned char)path[n];
        if (n >= MULIB_VFS_PATH_CAP - 1)
          return MULIB_VFS_ELIMIT;
        if (c < 32 || c == 127 || c == '\\')
          return MULIB_VFS_EINVAL;
        ++n;
      }
    temp[0] = '/';
    while (i < n)
      {
        size_t start, length;
        while (i < n && path[i] == '/')
          ++i;
        start = i;
        while (i < n && path[i] != '/')
          ++i;
        length = i - start;
        if (!length || (length == 1 && path[start] == '.'))
          continue;
        if (length == 2 && path[start] == '.' && path[start + 1] == '.')
          {
            if (used == 1)
              return MULIB_VFS_EINVAL;
            while (used > 1 && temp[used - 1] != '/')
              --used;
            if (used > 1)
              --used;
          }
        else
          {
            if (used > 1)
              temp[used++] = '/';
            memcpy (temp + used, path + start, length);
            used += length;
          }
      }
    if (used >= capacity)
      return MULIB_VFS_ELIMIT;
    temp[used] = 0;
    memcpy (out, temp, used + 1);
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_vfs_init (mulib_vfs *vfs, size_t max_mounts)
  {
    if (!vfs || !max_mounts || max_mounts > SIZE_MAX / sizeof (*vfs->mounts))
      return MULIB_VFS_EINVAL;
    vfs->mounts = NULL;
    vfs->count = 0;
    vfs->max_mounts = max_mounts;
    return MULIB_VFS_OK;
  }
  void
  mulib_vfs_free (mulib_vfs *vfs)
  {
    size_t i;
    if (!vfs)
      return;
    for (i = 0; i < vfs->count; ++i)
      free (vfs->mounts[i].path);
    free (vfs->mounts);
    memset (vfs, 0, sizeof (*vfs));
  }
  mulib_vfs_result
  mulib_vfs_mount (mulib_vfs *vfs, const char *path, const mulib_vfs_ops *ops,
                   void *context)
  {
    char normalized[MULIB_VFS_PATH_CAP], *copy;
    mulib_vfs_mount_entry *next;
    mulib_vfs_ops copied_ops;
    mulib_vfs_result r;
    size_t i;
    if (!vfs || !vfs->max_mounts || !ops)
      return MULIB_VFS_EINVAL;
    copied_ops = *ops;
    r = mulib_vfs_normalize (path, normalized, sizeof (normalized));
    if (r)
      return r;
    for (i = 0; i < vfs->count; ++i)
      if (!strcmp (vfs->mounts[i].path, normalized))
        return MULIB_VFS_EEXIST;
    if (vfs->count == vfs->max_mounts)
      return MULIB_VFS_ELIMIT;
    copy = mulib_vfs_copy_ (normalized);
    if (!copy)
      return MULIB_VFS_ENOMEM;
    next = (mulib_vfs_mount_entry *)realloc (
        vfs->mounts, (vfs->count + 1) * sizeof (*next));
    if (!next)
      {
        free (copy);
        return MULIB_VFS_ENOMEM;
      }
    vfs->mounts = next;
    next[vfs->count].path = copy;
    next[vfs->count].ops = copied_ops;
    next[vfs->count++].context = context;
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_vfs_unmount (mulib_vfs *vfs, const char *path)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    size_t i;
    mulib_vfs_result r;
    if (!vfs)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_normalize (path, normalized, sizeof (normalized));
    if (r)
      return r;
    for (i = 0; i < vfs->count; ++i)
      if (!strcmp (vfs->mounts[i].path, normalized))
        {
          free (vfs->mounts[i].path);
          memmove (vfs->mounts + i, vfs->mounts + i + 1,
                   (vfs->count - i - 1) * sizeof (*vfs->mounts));
          --vfs->count;
          return MULIB_VFS_OK;
        }
    return MULIB_VFS_ENOENT;
  }
  static int
  mulib_vfs_prefix_ (const char *prefix, const char *path)
  {
    size_t n = strlen (prefix);
    return !strncmp (prefix, path, n)
           && (n == 1 || !path[n] || path[n] == '/');
  }
  static mulib_vfs_result
  mulib_vfs_route_ (mulib_vfs *vfs, const char *path, char *normalized,
                    mulib_vfs_mount_entry **mount, const char **relative)
  {
    size_t i, best = 0;
    mulib_vfs_result r;
    if (!vfs)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_normalize (path, normalized, MULIB_VFS_PATH_CAP);
    if (r)
      return r;
    *mount = NULL;
    for (i = 0; i < vfs->count; ++i)
      {
        size_t n = strlen (vfs->mounts[i].path);
        if (n > best && mulib_vfs_prefix_ (vfs->mounts[i].path, normalized))
          {
            best = n;
            *mount = vfs->mounts + i;
          }
      }
    if (!*mount)
      return MULIB_VFS_ENOENT;
    *relative = best == 1 ? normalized : normalized + best;
    if (!**relative)
      *relative = "/";
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_vfs_read (mulib_vfs *vfs, const char *path, size_t offset, void *data,
                  size_t capacity, size_t *count)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    const char *relative;
    mulib_vfs_mount_entry *m;
    mulib_vfs_result r;
    size_t got = 0;
    if (count)
      *count = 0;
    if (!count || (!data && capacity) || capacity > SIZE_MAX - offset)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_route_ (vfs, path, normalized, &m, &relative);
    if (r)
      return r;
    if (!m->ops.read)
      return MULIB_VFS_ENOTSUP;
    r = m->ops.read (m->context, relative, offset, data, capacity, &got);
    if (got > capacity)
      return MULIB_VFS_EIO;
    if (!r)
      *count = got;
    return r;
  }
  mulib_vfs_result
  mulib_vfs_write (mulib_vfs *vfs, const char *path, size_t offset,
                   const void *data, size_t length, size_t *count)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    const char *relative;
    mulib_vfs_mount_entry *m;
    mulib_vfs_result r;
    size_t got = 0;
    if (count)
      *count = 0;
    if (!count || (!data && length) || length > SIZE_MAX - offset)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_route_ (vfs, path, normalized, &m, &relative);
    if (r)
      return r;
    if (!m->ops.write)
      return MULIB_VFS_EROFS;
    r = m->ops.write (m->context, relative, offset, data, length, &got);
    if (got > length)
      return MULIB_VFS_EIO;
    if (!r)
      *count = got;
    return r;
  }
  mulib_vfs_result
  mulib_vfs_getstat (mulib_vfs *vfs, const char *path, mulib_vfs_stat *info)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    const char *relative;
    mulib_vfs_mount_entry *m;
    mulib_vfs_stat temp = { MULIB_VFS_FILE, 0 };
    mulib_vfs_result r;
    if (!info)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_route_ (vfs, path, normalized, &m, &relative);
    if (r)
      return r;
    if (!m->ops.stat)
      return MULIB_VFS_ENOTSUP;
    r = m->ops.stat (m->context, relative, &temp);
    if (!r)
      *info = temp;
    return r;
  }
  mulib_vfs_result
  mulib_vfs_list (mulib_vfs *vfs, const char *path, mulib_vfs_visit visit,
                  void *user)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    const char *relative;
    mulib_vfs_mount_entry *m;
    mulib_vfs_result r;
    if (!visit)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_route_ (vfs, path, normalized, &m, &relative);
    if (r)
      return r;
    if (!m->ops.list)
      return MULIB_VFS_ENOTSUP;
    return m->ops.list (m->context, relative, visit, user);
  }
  mulib_vfs_result
  mulib_vfs_mem_init (mulib_vfs_mem *mem, size_t max_nodes,
                      size_t max_file_bytes)
  {
    if (!mem || !max_nodes || max_nodes > SIZE_MAX / sizeof (*mem->nodes))
      return MULIB_VFS_EINVAL;
    mem->nodes = NULL;
    mem->count = 0;
    mem->max_nodes = max_nodes;
    mem->max_file_bytes = max_file_bytes;
    return MULIB_VFS_OK;
  }
  void
  mulib_vfs_mem_free (mulib_vfs_mem *mem)
  {
    size_t i;
    if (!mem)
      return;
    for (i = 0; i < mem->count; ++i)
      {
        free (mem->nodes[i].path);
        free (mem->nodes[i].data);
      }
    free (mem->nodes);
    memset (mem, 0, sizeof (*mem));
  }
  static mulib_vfs_mem_node *
  mulib_vfs_mem_find_ (mulib_vfs_mem *mem, const char *path)
  {
    size_t i;
    for (i = 0; i < mem->count; ++i)
      if (!strcmp (mem->nodes[i].path, path))
        return mem->nodes + i;
    return NULL;
  }
  static mulib_vfs_result
  mulib_vfs_mem_parent_ (mulib_vfs_mem *mem, const char *path)
  {
    char parent[MULIB_VFS_PATH_CAP];
    char *slash;
    mulib_vfs_mem_node *node;
    memcpy (parent, path, strlen (path) + 1);
    slash = strrchr (parent, '/');
    if (slash == parent)
      return MULIB_VFS_OK;
    *slash = 0;
    node = mulib_vfs_mem_find_ (mem, parent);
    return !node                               ? MULIB_VFS_ENOENT
           : node->kind == MULIB_VFS_DIRECTORY ? MULIB_VFS_OK
                                               : MULIB_VFS_ENOTDIR;
  }
  static mulib_vfs_result
  mulib_vfs_mem_replace_ (mulib_vfs_mem *mem, const char *path,
                          mulib_vfs_kind kind, const void *data, size_t length)
  {
    mulib_vfs_mem_node *node, *next;
    unsigned char *bytes = NULL;
    char *copy;
    mulib_vfs_result r;
    if (!strcmp (path, "/"))
      return kind == MULIB_VFS_DIRECTORY ? MULIB_VFS_EEXIST : MULIB_VFS_EISDIR;
    node = mulib_vfs_mem_find_ (mem, path);
    if (node && node->kind == MULIB_VFS_DIRECTORY)
      return kind == MULIB_VFS_DIRECTORY ? MULIB_VFS_EEXIST : MULIB_VFS_EISDIR;
    if (node && kind == MULIB_VFS_DIRECTORY)
      return MULIB_VFS_EEXIST;
    if (!node && mem->count == mem->max_nodes)
      return MULIB_VFS_ELIMIT;
    r = mulib_vfs_mem_parent_ (mem, path);
    if (r)
      return r;
    if (length)
      {
        bytes = (unsigned char *)malloc (length);
        if (!bytes)
          return MULIB_VFS_ENOMEM;
        memcpy (bytes, data, length);
      }
    if (node)
      {
        free (node->data);
        node->data = bytes;
        node->length = length;
        return MULIB_VFS_OK;
      }
    copy = mulib_vfs_copy_ (path);
    if (!copy)
      {
        free (bytes);
        return MULIB_VFS_ENOMEM;
      }
    next = (mulib_vfs_mem_node *)realloc (mem->nodes,
                                          (mem->count + 1) * sizeof (*next));
    if (!next)
      {
        free (bytes);
        free (copy);
        return MULIB_VFS_ENOMEM;
      }
    mem->nodes = next;
    node = next + mem->count++;
    node->path = copy;
    node->data = bytes;
    node->length = length;
    node->kind = kind;
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_vfs_mem_mkdir (mulib_vfs_mem *mem, const char *path)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    mulib_vfs_result r;
    if (!mem || !mem->max_nodes)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_normalize (path, normalized, sizeof (normalized));
    return r ? r
             : mulib_vfs_mem_replace_ (mem, normalized, MULIB_VFS_DIRECTORY,
                                       NULL, 0);
  }
  mulib_vfs_result
  mulib_vfs_mem_put (mulib_vfs_mem *mem, const char *path, const void *data,
                     size_t length)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    mulib_vfs_result r;
    if (!mem || !mem->max_nodes || (!data && length))
      return MULIB_VFS_EINVAL;
    if (length > mem->max_file_bytes)
      return MULIB_VFS_ELIMIT;
    r = mulib_vfs_normalize (path, normalized, sizeof (normalized));
    return r ? r
             : mulib_vfs_mem_replace_ (mem, normalized, MULIB_VFS_FILE, data,
                                       length);
  }
  mulib_vfs_result
  mulib_vfs_mem_remove (mulib_vfs_mem *mem, const char *path)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    size_t i, index = SIZE_MAX;
    mulib_vfs_result r;
    if (!mem || !mem->max_nodes)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_normalize (path, normalized, sizeof (normalized));
    if (r)
      return r;
    if (!strcmp (normalized, "/"))
      return MULIB_VFS_EINVAL;
    for (i = 0; i < mem->count; ++i)
      {
        if (!strcmp (mem->nodes[i].path, normalized))
          index = i;
        else if (mulib_vfs_prefix_ (normalized, mem->nodes[i].path))
          return MULIB_VFS_EEXIST;
      }
    if (index == SIZE_MAX)
      return MULIB_VFS_ENOENT;
    free (mem->nodes[index].path);
    free (mem->nodes[index].data);
    memmove (mem->nodes + index, mem->nodes + index + 1,
             (mem->count - index - 1) * sizeof (*mem->nodes));
    --mem->count;
    return MULIB_VFS_OK;
  }
  static mulib_vfs_result
  mulib_vfs_mem_stat_ (void *context, const char *path, mulib_vfs_stat *info)
  {
    mulib_vfs_mem *mem = (mulib_vfs_mem *)context;
    mulib_vfs_mem_node *node;
    if (!mem || !mem->max_nodes)
      return MULIB_VFS_EINVAL;
    if (!strcmp (path, "/"))
      {
        info->kind = MULIB_VFS_DIRECTORY;
        info->size = 0;
        return MULIB_VFS_OK;
      }
    node = mulib_vfs_mem_find_ (mem, path);
    if (!node)
      return MULIB_VFS_ENOENT;
    info->kind = node->kind;
    info->size = (uint64_t)node->length;
    return MULIB_VFS_OK;
  }
  static mulib_vfs_result
  mulib_vfs_mem_read_ (void *context, const char *path, size_t offset,
                       void *data, size_t capacity, size_t *count)
  {
    mulib_vfs_mem *mem = (mulib_vfs_mem *)context;
    mulib_vfs_mem_node *node;
    if (!mem || !mem->max_nodes)
      return MULIB_VFS_EINVAL;
    if (!strcmp (path, "/"))
      return MULIB_VFS_EISDIR;
    node = mulib_vfs_mem_find_ (mem, path);
    if (!node)
      return MULIB_VFS_ENOENT;
    if (node->kind != MULIB_VFS_FILE)
      return MULIB_VFS_EISDIR;
    if (offset < node->length)
      {
        *count = node->length - offset;
        if (*count > capacity)
          *count = capacity;
        if (*count)
          memcpy (data, node->data + offset, *count);
      }
    return MULIB_VFS_OK;
  }
  static mulib_vfs_result
  mulib_vfs_mem_write_ (void *context, const char *path, size_t offset,
                        const void *data, size_t length, size_t *count)
  {
    mulib_vfs_mem *mem = (mulib_vfs_mem *)context;
    mulib_vfs_mem_node *node;
    unsigned char *bytes;
    size_t total;
    mulib_vfs_result r;
    if (!mem || !mem->max_nodes)
      return MULIB_VFS_EINVAL;
    if (!strcmp (path, "/"))
      return MULIB_VFS_EISDIR;
    node = mulib_vfs_mem_find_ (mem, path);
    if (node && node->kind != MULIB_VFS_FILE)
      return MULIB_VFS_EISDIR;
    total = offset + length;
    if (node && total < node->length)
      total = node->length;
    if (total > mem->max_file_bytes)
      return MULIB_VFS_ELIMIT;
    bytes = total ? (unsigned char *)calloc (total, 1) : NULL;
    if (total && !bytes)
      return MULIB_VFS_ENOMEM;
    if (node && node->length)
      memcpy (bytes, node->data, node->length);
    if (length)
      memcpy (bytes + offset, data, length);
    r = mulib_vfs_mem_replace_ (mem, path, MULIB_VFS_FILE, bytes, total);
    free (bytes);
    if (!r)
      *count = length;
    return r;
  }
  static mulib_vfs_result
  mulib_vfs_mem_list_ (void *context, const char *path, mulib_vfs_visit visit,
                       void *user)
  {
    mulib_vfs_mem *mem = (mulib_vfs_mem *)context;
    mulib_vfs_stat info;
    mulib_vfs_result r = mulib_vfs_mem_stat_ (context, path, &info);
    size_t i, prefix = strlen (path);
    if (r)
      return r;
    if (info.kind != MULIB_VFS_DIRECTORY)
      return MULIB_VFS_ENOTDIR;
    for (i = 0; i < mem->count; ++i)
      {
        const char *child;
        if (!mulib_vfs_prefix_ (path, mem->nodes[i].path)
            || !strcmp (path, mem->nodes[i].path))
          continue;
        child = mem->nodes[i].path + prefix + (prefix == 1 ? 0 : 1);
        if (strchr (child, '/'))
          continue;
        info.kind = mem->nodes[i].kind;
        info.size = (uint64_t)mem->nodes[i].length;
        if (visit (user, child, &info))
          return MULIB_VFS_ECANCELED;
      }
    return MULIB_VFS_OK;
  }
  mulib_vfs_ops
  mulib_vfs_mem_ops (void)
  {
    mulib_vfs_ops ops;
    ops.read = mulib_vfs_mem_read_;
    ops.write = mulib_vfs_mem_write_;
    ops.stat = mulib_vfs_mem_stat_;
    ops.list = mulib_vfs_mem_list_;
    return ops;
  }
#ifdef __cplusplus
}
#endif
#endif
