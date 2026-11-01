/**
 * @file mulib-pathdb.h
 * @brief Owned sorted filesystem-path index, VFS snapshots and binary
 * persistence.
 * @details Define MULIB_IMPL_PATHDB and separately MULIB_IMPL_VFS. Paths and
 * metadata are copied, never live references into a backend. This is not a
 * filesystem watcher or an on-disk database engine. See
 * manual/mulib-pathdb.md.
 */
#ifndef MULIB_PATHDB_H
#define MULIB_PATHDB_H
#include "mulib-vfs.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_PATHDB_MAX_DEPTH = 64
  };
  typedef struct mulib_pathdb_entry
  {
    char *path;
    mulib_vfs_stat info;
  } mulib_pathdb_entry;
  typedef struct mulib_pathdb
  {
    mulib_pathdb_entry *entries;
    size_t count, max_entries;
  } mulib_pathdb;
  /** @brief Borrowed entry valid during callback; nonzero stops the query. */
  typedef int (*mulib_pathdb_visit) (void *user,
                                     const mulib_pathdb_entry *entry);
  /** @brief Initialize an uninitialized index with a limit in 1..UINT32_MAX.
   */
  mulib_vfs_result mulib_pathdb_init (mulib_pathdb *db, size_t max_entries);
  /** @brief Release all owned paths/table and reset to zero. */
  void mulib_pathdb_free (mulib_pathdb *db);
  /** @brief Insert/update a normalized absolute path, copying path and
   * metadata. */
  mulib_vfs_result mulib_pathdb_put (mulib_pathdb *db, const char *path,
                                     const mulib_vfs_stat *info);
  /** @brief Remove one exact normalized path, not its descendants. */
  mulib_vfs_result mulib_pathdb_remove (mulib_pathdb *db, const char *path);
  /** @brief Query component-prefix plus optional case-sensitive substring
   * filter. NULL prefix means root; NULL/empty fragment matches all. Results
   * are sorted bytewise. No mutation of db is allowed during callbacks.
   * Cancellation returns ECANCELED; visited includes the final callback. No
   * entry pointers are retained.
   */
  mulib_vfs_result mulib_pathdb_query (const mulib_pathdb *db,
                                       const char *prefix,
                                       const char *fragment,
                                       mulib_pathdb_visit visit, void *user,
                                       size_t *visited);
  /** @brief Transactionally replace index with a recursive VFS snapshot
   * including root. At most 64 component levels below root; only
   * file/directory entries. Traverses backend listings, not hidden mount-table
   * entries. No symlinks exist in the VFS model. Failure preserves the old
   * index.
   */
  mulib_vfs_result mulib_pathdb_scan (mulib_pathdb *db, mulib_vfs *vfs,
                                      const char *root);
  /** @brief Write canonical MPD1 binary records at the current stream
   * position. Stream stays open; caller owns flushing/closing/atomic
   * replacement.
   */
  mulib_vfs_result mulib_pathdb_save (const mulib_pathdb *db, FILE *stream);
  /** @brief Transactionally load MPD1 from current position through EOF.
   * Duplicate, unordered, non-normalized, truncated or trailing bytes fail.
   * Stream remains open; its position/error indicators may change even on
   * failure.
   */
  mulib_vfs_result mulib_pathdb_load (mulib_pathdb *db, FILE *stream);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_PATHDB) && !defined(MULIB_PATHDB_IMPLEMENTED)
#define MULIB_PATHDB_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_vfs_result
  mulib_pathdb_init (mulib_pathdb *db, size_t max_entries)
  {
    if (!db || !max_entries || max_entries > UINT32_MAX
        || max_entries > SIZE_MAX / sizeof (*db->entries))
      return MULIB_VFS_EINVAL;
    db->entries = NULL;
    db->count = 0;
    db->max_entries = max_entries;
    return MULIB_VFS_OK;
  }
  void
  mulib_pathdb_free (mulib_pathdb *db)
  {
    size_t i;
    if (!db)
      return;
    for (i = 0; i < db->count; ++i)
      free (db->entries[i].path);
    free (db->entries);
    memset (db, 0, sizeof (*db));
  }
  static size_t
  mulib_pathdb_lower_ (const mulib_pathdb *db, const char *path)
  {
    size_t low = 0, high = db->count;
    while (low < high)
      {
        size_t middle = low + (high - low) / 2;
        if (strcmp (db->entries[middle].path, path) < 0)
          low = middle + 1;
        else
          high = middle;
      }
    return low;
  }
  mulib_vfs_result
  mulib_pathdb_put (mulib_pathdb *db, const char *path,
                    const mulib_vfs_stat *info)
  {
    char normalized[MULIB_VFS_PATH_CAP], *copy;
    mulib_pathdb_entry *next;
    mulib_vfs_stat copied_info;
    size_t index, length;
    mulib_vfs_result r;
    if (!db || !db->max_entries || !info
        || (info->kind != MULIB_VFS_FILE && info->kind != MULIB_VFS_DIRECTORY))
      return MULIB_VFS_EINVAL;
    copied_info = *info;
    r = mulib_vfs_normalize (path, normalized, sizeof (normalized));
    if (r)
      return r;
    index = mulib_pathdb_lower_ (db, normalized);
    if (index < db->count && !strcmp (db->entries[index].path, normalized))
      {
        db->entries[index].info = copied_info;
        return MULIB_VFS_OK;
      }
    if (db->count == db->max_entries)
      return MULIB_VFS_ELIMIT;
    length = strlen (normalized) + 1;
    copy = (char *)malloc (length);
    if (!copy)
      return MULIB_VFS_ENOMEM;
    memcpy (copy, normalized, length);
    next = (mulib_pathdb_entry *)realloc (db->entries,
                                          (db->count + 1) * sizeof (*next));
    if (!next)
      {
        free (copy);
        return MULIB_VFS_ENOMEM;
      }
    db->entries = next;
    memmove (next + index + 1, next + index,
             (db->count - index) * sizeof (*next));
    next[index].path = copy;
    next[index].info = copied_info;
    ++db->count;
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_pathdb_remove (mulib_pathdb *db, const char *path)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    size_t index;
    mulib_vfs_result r;
    if (!db || !db->max_entries)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_normalize (path, normalized, sizeof (normalized));
    if (r)
      return r;
    index = mulib_pathdb_lower_ (db, normalized);
    if (index == db->count || strcmp (db->entries[index].path, normalized))
      return MULIB_VFS_ENOENT;
    free (db->entries[index].path);
    memmove (db->entries + index, db->entries + index + 1,
             (db->count - index - 1) * sizeof (*db->entries));
    --db->count;
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_pathdb_query (const mulib_pathdb *db, const char *prefix,
                      const char *fragment, mulib_pathdb_visit visit,
                      void *user, size_t *visited)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    size_t i, n;
    mulib_vfs_result r;
    if (visited)
      *visited = 0;
    if (!db || !db->max_entries || !visit || !visited)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_normalize (prefix ? prefix : "/", normalized,
                             sizeof (normalized));
    if (r)
      return r;
    n = strlen (normalized);
    for (i = mulib_pathdb_lower_ (db, normalized); i < db->count; ++i)
      {
        const char *path = db->entries[i].path;
        if (strncmp (path, normalized, n))
          break;
        if (n != 1 && path[n] && path[n] != '/')
          continue;
        if (fragment && !strstr (path, fragment))
          continue;
        ++*visited;
        if (visit (user, db->entries + i))
          return MULIB_VFS_ECANCELED;
      }
    return MULIB_VFS_OK;
  }
  typedef struct mulib_pathdb_walk_
  {
    mulib_pathdb *db;
    mulib_vfs *vfs;
    const char *parent;
    size_t depth;
    mulib_vfs_result result;
  } mulib_pathdb_walk_;
  static mulib_vfs_result mulib_pathdb_walk_node_ (mulib_pathdb *, mulib_vfs *,
                                                   const char *,
                                                   const mulib_vfs_stat *,
                                                   size_t);
  static int
  mulib_pathdb_walk_child_ (void *context, const char *name,
                            const mulib_vfs_stat *info)
  {
    mulib_pathdb_walk_ *walk = (mulib_pathdb_walk_ *)context;
    char path[MULIB_VFS_PATH_CAP], normalized[MULIB_VFS_PATH_CAP];
    mulib_vfs_stat routed_info;
    size_t a, b = 0, sep;
    if (!name || !*name || !info)
      {
        walk->result = MULIB_VFS_EFORMAT;
        return 1;
      }
    while (b < MULIB_VFS_PATH_CAP && name[b])
      ++b;
    if (b == MULIB_VFS_PATH_CAP)
      {
        walk->result = MULIB_VFS_ELIMIT;
        return 1;
      }
    if (strchr (name, '/') || !strcmp (name, ".") || !strcmp (name, ".."))
      {
        walk->result = MULIB_VFS_EFORMAT;
        return 1;
      }
    if (walk->depth == MULIB_PATHDB_MAX_DEPTH)
      {
        walk->result = MULIB_VFS_ELIMIT;
        return 1;
      }
    a = strlen (walk->parent);
    sep = a == 1 ? 0 : 1;
    if (a + b + sep >= sizeof (path))
      {
        walk->result = MULIB_VFS_ELIMIT;
        return 1;
      }
    memcpy (path, walk->parent, a);
    if (sep)
      path[a++] = '/';
    memcpy (path + a, name, b + 1);
    walk->result = mulib_vfs_normalize (path, normalized, sizeof (normalized));
    if (walk->result)
      {
        walk->result = MULIB_VFS_EFORMAT;
        return 1;
      }
    if (strcmp (path, normalized))
      {
        walk->result = MULIB_VFS_EFORMAT;
        return 1;
      }
    if (info->kind != MULIB_VFS_FILE && info->kind != MULIB_VFS_DIRECTORY)
      {
        walk->result = MULIB_VFS_EFORMAT;
        return 1;
      }
    walk->result = mulib_vfs_getstat (walk->vfs, path, &routed_info);
    if (walk->result)
      return 1;
    walk->result = mulib_pathdb_walk_node_ (walk->db, walk->vfs, path,
                                            &routed_info, walk->depth + 1);
    return walk->result != MULIB_VFS_OK;
  }
  static mulib_vfs_result
  mulib_pathdb_walk_node_ (mulib_pathdb *db, mulib_vfs *vfs, const char *path,
                           const mulib_vfs_stat *info, size_t depth)
  {
    mulib_pathdb_walk_ walk;
    mulib_vfs_result r;
    size_t index = mulib_pathdb_lower_ (db, path);
    if (index < db->count && !strcmp (db->entries[index].path, path))
      return MULIB_VFS_EFORMAT;
    if (info->kind != MULIB_VFS_FILE && info->kind != MULIB_VFS_DIRECTORY)
      return MULIB_VFS_EFORMAT;
    r = mulib_pathdb_put (db, path, info);
    if (r || info->kind == MULIB_VFS_FILE)
      return r;
    walk.db = db;
    walk.vfs = vfs;
    walk.parent = path;
    walk.depth = depth;
    walk.result = MULIB_VFS_OK;
    r = mulib_vfs_list (vfs, path, mulib_pathdb_walk_child_, &walk);
    return walk.result ? walk.result : r;
  }
  mulib_vfs_result
  mulib_pathdb_scan (mulib_pathdb *db, mulib_vfs *vfs, const char *root)
  {
    char normalized[MULIB_VFS_PATH_CAP];
    mulib_pathdb next;
    mulib_vfs_stat info;
    mulib_vfs_result r;
    if (!db || !db->max_entries || !vfs)
      return MULIB_VFS_EINVAL;
    r = mulib_vfs_normalize (root, normalized, sizeof (normalized));
    if (r)
      return r;
    r = mulib_vfs_getstat (vfs, normalized, &info);
    if (r)
      return r;
    r = mulib_pathdb_init (&next, db->max_entries);
    if (r)
      return r;
    r = mulib_pathdb_walk_node_ (&next, vfs, normalized, &info, 0);
    if (r)
      {
        mulib_pathdb_free (&next);
        return r;
      }
    mulib_pathdb_free (db);
    *db = next;
    return MULIB_VFS_OK;
  }
  static int
  mulib_pathdb_write_le_ (FILE *stream, uint64_t value, size_t bytes)
  {
    unsigned char buffer[8];
    size_t i;
    for (i = 0; i < bytes; ++i)
      {
        buffer[i] = (unsigned char)(value & 255);
        value >>= 8;
      }
    return fwrite (buffer, 1, bytes, stream) == bytes;
  }
  static mulib_vfs_result
  mulib_pathdb_read_le_ (FILE *stream, size_t bytes, uint64_t *value)
  {
    unsigned char buffer[8];
    size_t i;
    if (fread (buffer, 1, bytes, stream) != bytes)
      return ferror (stream) ? MULIB_VFS_EIO : MULIB_VFS_EFORMAT;
    *value = 0;
    for (i = 0; i < bytes; ++i)
      *value |= (uint64_t)buffer[i] << (i * 8);
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_pathdb_save (const mulib_pathdb *db, FILE *stream)
  {
    size_t i;
    if (!db || !db->max_entries || !stream)
      return MULIB_VFS_EINVAL;
    if (fwrite ("MPD1", 1, 4, stream) != 4
        || !mulib_pathdb_write_le_ (stream, db->count, 4))
      return MULIB_VFS_EIO;
    for (i = 0; i < db->count; ++i)
      {
        const mulib_pathdb_entry *entry = db->entries + i;
        size_t length = strlen (entry->path);
        if (!mulib_pathdb_write_le_ (stream, length, 2)
            || !mulib_pathdb_write_le_ (stream, entry->info.kind, 1)
            || !mulib_pathdb_write_le_ (stream, entry->info.size, 8)
            || fwrite (entry->path, 1, length, stream) != length)
          return MULIB_VFS_EIO;
      }
    return MULIB_VFS_OK;
  }
  mulib_vfs_result
  mulib_pathdb_load (mulib_pathdb *db, FILE *stream)
  {
    char magic[4], path[MULIB_VFS_PATH_CAP], normalized[MULIB_VFS_PATH_CAP];
    uint64_t count = 0, length = 0, kind = 0, size = 0, i;
    mulib_pathdb next;
    mulib_vfs_result r;
    if (!db || !db->max_entries || !stream)
      return MULIB_VFS_EINVAL;
    if (fread (magic, 1, 4, stream) != 4)
      return ferror (stream) ? MULIB_VFS_EIO : MULIB_VFS_EFORMAT;
    if (memcmp (magic, "MPD1", 4))
      return MULIB_VFS_EFORMAT;
    r = mulib_pathdb_read_le_ (stream, 4, &count);
    if (r)
      return r;
    if (count > db->max_entries)
      return MULIB_VFS_ELIMIT;
    r = mulib_pathdb_init (&next, db->max_entries);
    if (r)
      return r;
    for (i = 0; i < count && !r; ++i)
      {
        mulib_vfs_stat info;
        r = mulib_pathdb_read_le_ (stream, 2, &length);
        if (!r)
          r = mulib_pathdb_read_le_ (stream, 1, &kind);
        if (!r)
          r = mulib_pathdb_read_le_ (stream, 8, &size);
        if (r)
          break;
        if (!length || length >= sizeof (path))
          {
            r = MULIB_VFS_ELIMIT;
            break;
          }
        if (kind != MULIB_VFS_FILE && kind != MULIB_VFS_DIRECTORY)
          {
            r = MULIB_VFS_EFORMAT;
            break;
          }
        if (fread (path, 1, (size_t)length, stream) != length)
          {
            r = ferror (stream) ? MULIB_VFS_EIO : MULIB_VFS_EFORMAT;
            break;
          }
        if (memchr (path, 0, (size_t)length))
          {
            r = MULIB_VFS_EFORMAT;
            break;
          }
        path[length] = 0;
        r = mulib_vfs_normalize (path, normalized, sizeof (normalized));
        if (r)
          {
            r = MULIB_VFS_EFORMAT;
            break;
          }
        if (strcmp (path, normalized)
            || (next.count
                && strcmp (next.entries[next.count - 1].path, path) >= 0))
          {
            r = MULIB_VFS_EFORMAT;
            break;
          }
        info.kind = (mulib_vfs_kind)kind;
        info.size = size;
        r = mulib_pathdb_put (&next, path, &info);
      }
    if (!r)
      {
        if (fgetc (stream) != EOF)
          r = MULIB_VFS_EFORMAT;
        else if (ferror (stream))
          r = MULIB_VFS_EIO;
      }
    if (r)
      {
        mulib_pathdb_free (&next);
        return r;
      }
    mulib_pathdb_free (db);
    *db = next;
    return MULIB_VFS_OK;
  }
#ifdef __cplusplus
}
#endif
#endif
