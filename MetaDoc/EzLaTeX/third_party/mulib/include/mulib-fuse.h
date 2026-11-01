/**
 * @file mulib-fuse.h
 * @brief Dependency-free userspace filesystem request dispatch to MuLIB VFS.
 * @details This is NOT libfuse, the Linux FUSE wire protocol, a /dev/fuse
 * transport, or a kernel mount implementation. Requests are synchronous typed
 * records useful for embedding, tests and transport adapters. Define
 * MULIB_IMPL_FUSE and separately provide MULIB_IMPL_VFS once.
 */
#ifndef MULIB_FUSE_H
#define MULIB_FUSE_H
#include "mulib-vfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_fuse_opcode
  {
    MULIB_FUSE_GETATTR = 1,
    MULIB_FUSE_READ = 2,
    MULIB_FUSE_WRITE = 3,
    MULIB_FUSE_READDIR = 4
  } mulib_fuse_opcode;
  typedef struct mulib_fuse_request
  {
    uint64_t id;
    mulib_fuse_opcode opcode;
    const char *path;
    size_t offset, length;
    const void *data;
  } mulib_fuse_request;
  typedef struct mulib_fuse_response
  {
    uint64_t id;
    mulib_vfs_result result;
    mulib_vfs_stat info;
    size_t transferred;
  } mulib_fuse_response;
  typedef struct mulib_fuse
  {
    mulib_vfs *vfs;
    int read_only;
  } mulib_fuse;
  /** @brief Initialize an adapter borrowing vfs; no allocation or mounting
   * occurs. */
  mulib_vfs_result mulib_fuse_init (mulib_fuse *adapter, mulib_vfs *vfs,
                                    int read_only);
  /** @brief Dispatch one request and initialize response even on failure.
   * READ requires output_capacity >= request.length; WRITE borrows
   * request.data. READDIR calls visit with borrowed immediate-child names and
   * counts delivered entries, including the one that requests cancellation.
   * Other operations ignore output/visit. Unknown opcodes return ENOTSUP. No
   * request pointer is retained.
   */
  mulib_vfs_result mulib_fuse_dispatch (mulib_fuse *adapter,
                                        const mulib_fuse_request *request,
                                        void *output, size_t output_capacity,
                                        mulib_vfs_visit visit, void *user,
                                        mulib_fuse_response *response);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_FUSE) && !defined(MULIB_FUSE_IMPLEMENTED)
#define MULIB_FUSE_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_vfs_result
  mulib_fuse_init (mulib_fuse *adapter, mulib_vfs *vfs, int read_only)
  {
    if (!adapter || !vfs || !vfs->max_mounts)
      return MULIB_VFS_EINVAL;
    adapter->vfs = vfs;
    adapter->read_only = read_only != 0;
    return MULIB_VFS_OK;
  }
  typedef struct mulib_fuse_listing_
  {
    mulib_vfs_visit visit;
    void *user;
    size_t count;
  } mulib_fuse_listing_;
  static int
  mulib_fuse_visit_ (void *context, const char *name,
                     const mulib_vfs_stat *info)
  {
    mulib_fuse_listing_ *listing = (mulib_fuse_listing_ *)context;
    ++listing->count;
    return listing->visit (listing->user, name, info);
  }
  mulib_vfs_result
  mulib_fuse_dispatch (mulib_fuse *adapter, const mulib_fuse_request *request,
                       void *output, size_t output_capacity,
                       mulib_vfs_visit visit, void *user,
                       mulib_fuse_response *response)
  {
    mulib_vfs_result r = MULIB_VFS_EINVAL;
    mulib_fuse_listing_ listing;
    if (!response)
      return r;
    memset (response, 0, sizeof (*response));
    response->result = r;
    if (request)
      response->id = request->id;
    if (!adapter || !adapter->vfs || !request || !request->path)
      return r;
    switch (request->opcode)
      {
      case MULIB_FUSE_GETATTR:
        r = mulib_vfs_getstat (adapter->vfs, request->path, &response->info);
        break;
      case MULIB_FUSE_READ:
        if (request->length > output_capacity)
          r = MULIB_VFS_ELIMIT;
        else
          r = mulib_vfs_read (adapter->vfs, request->path, request->offset,
                              output, request->length, &response->transferred);
        break;
      case MULIB_FUSE_WRITE:
        if (adapter->read_only)
          r = MULIB_VFS_EROFS;
        else
          r = mulib_vfs_write (adapter->vfs, request->path, request->offset,
                               request->data, request->length,
                               &response->transferred);
        break;
      case MULIB_FUSE_READDIR:
        if (!visit)
          break;
        listing.visit = visit;
        listing.user = user;
        listing.count = 0;
        r = mulib_vfs_list (adapter->vfs, request->path, mulib_fuse_visit_,
                            &listing);
        response->transferred = listing.count;
        break;
      default:
        r = MULIB_VFS_ENOTSUP;
        break;
      }
    response->result = r;
    return r;
  }
#ifdef __cplusplus
}
#endif
#endif
