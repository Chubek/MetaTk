/**
 * @file mulib-drm.h
 * @brief Linux Direct Rendering Manager version, capabilities and dumb
 * buffers. Native ioctl ABI, no libdrm. No modesetting, authentication, GPU
 * command APIs, or digital-rights-management. Define MULIB_IMPL_DRM in one
 * translation unit.
 */
#ifndef MULIB_DRM_H
#define MULIB_DRM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Zero is success; native ioctl errors are EIO with their errno
   * preserved. */
  enum mulib_drm_result
  {
    MULIB_DRM_OK = 0,
    MULIB_DRM_EINVAL = -1,
    MULIB_DRM_EIO = -2,
    MULIB_DRM_ENOTSUP = -3,
    MULIB_DRM_EOVERFLOW = -4,
    MULIB_DRM_ESTATE = -5
  };
  /** @brief Native DRM version ioctl layout, including native pointer/size_t
   * widths. */
  typedef struct mulib_drm_version_request
  {
    int32_t major, minor, patchlevel;
    size_t name_len;
    char *name;
    size_t date_len;
    char *date;
    size_t desc_len;
    char *desc;
  } mulib_drm_version_request_t;
  /** @brief Fixed-width DRM_GET_CAP ioctl layout. */
  typedef struct mulib_drm_cap_request
  {
    uint64_t capability, value;
  } mulib_drm_cap_request_t;
  /** @brief Fixed-width DRM_IOCTL_MODE_CREATE_DUMB ioctl layout. */
  typedef struct mulib_drm_create_request
  {
    uint32_t height, width, bpp, flags, handle, pitch;
    uint64_t size;
  } mulib_drm_create_request_t;
  /** @brief Fixed-width DRM_IOCTL_MODE_MAP_DUMB ioctl layout. */
  typedef struct mulib_drm_map_request
  {
    uint32_t handle, pad;
    uint64_t offset;
  } mulib_drm_map_request_t;
  /** @brief Fixed-width DRM_IOCTL_MODE_DESTROY_DUMB ioctl layout. */
  typedef struct mulib_drm_destroy_request
  {
    uint32_t handle;
  } mulib_drm_destroy_request_t;
  /** @brief The focused set of ioctl operations whose native request codes are
   * exposed. */
  enum mulib_drm_operation
  {
    MULIB_DRM_VERSION,
    MULIB_DRM_GET_CAP,
    MULIB_DRM_CREATE_DUMB,
    MULIB_DRM_MAP_DUMB,
    MULIB_DRM_DESTROY_DUMB
  };
  /** @brief Common capability numbers; get_cap also accepts other native
   * numbers. */
  enum mulib_drm_capability
  {
    MULIB_DRM_CAP_DUMB_BUFFER = 1,
    MULIB_DRM_CAP_DUMB_PREFERRED_DEPTH = 3,
    MULIB_DRM_CAP_DUMB_PREFER_SHADOW = 4,
    MULIB_DRM_CAP_PRIME = 5
  };
  /** @brief Owned device fd; initialize and never copy. */
  typedef struct mulib_drm_device
  {
    int fd;
  } mulib_drm_device_t;
#define MULIB_DRM_DEVICE_INIT { -1 }
  /** @brief Version strings are terminated, possibly truncated; lengths are
   * full native lengths. */
  typedef struct mulib_drm_version
  {
    int32_t major, minor, patchlevel;
    char name[128], date[64], description[256];
    size_t name_length, date_length, description_length;
  } mulib_drm_version_t;
  /** @brief Owned dumb-buffer handle, mapping and duplicated fd. Never copy or
   * edit. The duplicate keeps the originating device open independently of
   * device_close.
   */
  typedef struct mulib_drm_dumb
  {
    uint32_t handle, width, height, bpp, pitch;
    uint64_t size;
    void *data;
    size_t mapped_size;
    int fd;
  } mulib_drm_dumb_t;
#define MULIB_DRM_DUMB_INIT { 0, 0, 0, 0, 0, 0, NULL, 0, -1 }
  /** @brief Native ioctl request code, or zero for invalid
   * operation/non-Linux. */
  unsigned long mulib_drm_ioctl_request (enum mulib_drm_operation operation);
  /** @brief Open an explicit device path read/write and close-on-exec; no
   * default GPU. */
  int mulib_drm_open (mulib_drm_device_t *device, const char *path);
  /** @brief Close/reset a device, idempotently. EIO may be reported after the
   * fd closes. */
  int mulib_drm_close (mulib_drm_device_t *device);
  /** @brief Fetch version; output unchanged on error. */
  int mulib_drm_get_version (const mulib_drm_device_t *device,
                             mulib_drm_version_t *version);
  /** @brief Fetch any native uint64 capability; output unchanged on error. */
  int mulib_drm_get_cap (const mulib_drm_device_t *device, uint64_t capability,
                         uint64_t *value);
  /** @brief Create a dumb buffer for nonzero dimensions and 1..64 bits/pixel.
   * Driver support/permissions still determine success. Output unchanged on
   * error.
   */
  int mulib_drm_dumb_create (const mulib_drm_device_t *device, uint32_t width,
                             uint32_t height, uint32_t bpp,
                             mulib_drm_dumb_t *buffer);
  /** @brief Map the entire buffer shared read/write; repeated map is a no-op.
   */
  int mulib_drm_dumb_map (mulib_drm_dumb_t *buffer);
  /** @brief Unmap if mapped, retaining the mapping on an OS error. */
  int mulib_drm_dumb_unmap (mulib_drm_dumb_t *buffer);
  /** @brief Unmap, destroy the handle, close its fd and reset, idempotently.
   * On unmap/ioctl failure retain remaining resources for retry; on close
   * failure resources have already been released. EIO from these APIs
   * preserves errno.
   */
  int mulib_drm_dumb_destroy (mulib_drm_dumb_t *buffer);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DRM_H */

#if defined(MULIB_IMPL_DRM) && !defined(MULIB_DRM_IMPLEMENTED)
#define MULIB_DRM_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  unsigned long
  mulib_drm_ioctl_request (enum mulib_drm_operation operation)
  {
#ifdef __linux__
    switch (operation)
      {
      case MULIB_DRM_VERSION:
        return _IOWR ('d', 0x00, mulib_drm_version_request_t);
      case MULIB_DRM_GET_CAP:
        return _IOWR ('d', 0x0c, mulib_drm_cap_request_t);
      case MULIB_DRM_CREATE_DUMB:
        return _IOWR ('d', 0xb2, mulib_drm_create_request_t);
      case MULIB_DRM_MAP_DUMB:
        return _IOWR ('d', 0xb3, mulib_drm_map_request_t);
      case MULIB_DRM_DESTROY_DUMB:
        return _IOWR ('d', 0xb4, mulib_drm_destroy_request_t);
      default:
        return 0;
      }
#else
  (void)operation;
  return 0;
#endif
  }
#ifdef __linux__
  static int
  mulib_drm_ioctl_ (int fd, enum mulib_drm_operation operation, void *request)
  {
    int rc;
    do
      {
        rc = ioctl (fd, mulib_drm_ioctl_request (operation), request);
      }
    while (rc < 0 && errno == EINTR);
    return rc;
  }
#endif
  int
  mulib_drm_open (mulib_drm_device_t *device, const char *path)
  {
    if (!device || !path)
      return MULIB_DRM_EINVAL;
    if (device->fd != -1)
      return MULIB_DRM_ESTATE;
#ifdef __linux__
    {
      int fd = open (path, O_RDWR), saved;
      if (fd < 0)
        return MULIB_DRM_EIO;
      if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0)
        {
          saved = errno;
          (void)close (fd);
          errno = saved;
          return MULIB_DRM_EIO;
        }
      device->fd = fd;
      return MULIB_DRM_OK;
    }
#else
  return MULIB_DRM_ENOTSUP;
#endif
  }
  int
  mulib_drm_close (mulib_drm_device_t *device)
  {
    if (!device)
      return MULIB_DRM_EINVAL;
#ifdef __linux__
    {
      int fd = device->fd;
      if (fd == -1)
        return MULIB_DRM_OK;
      device->fd = -1;
      return close (fd) < 0 ? MULIB_DRM_EIO : MULIB_DRM_OK;
    }
#else
  return MULIB_DRM_ENOTSUP;
#endif
  }
  int
  mulib_drm_get_version (const mulib_drm_device_t *device,
                         mulib_drm_version_t *out)
  {
    if (!device || device->fd < 0 || !out)
      return MULIB_DRM_EINVAL;
#ifdef __linux__
    {
      mulib_drm_version_request_t q;
      mulib_drm_version_t v;
      memset (&q, 0, sizeof (q));
      memset (&v, 0, sizeof (v));
      q.name = v.name;
      q.name_len = sizeof (v.name) - 1;
      q.date = v.date;
      q.date_len = sizeof (v.date) - 1;
      q.desc = v.description;
      q.desc_len = sizeof (v.description) - 1;
      if (mulib_drm_ioctl_ (device->fd, MULIB_DRM_VERSION, &q) < 0)
        return MULIB_DRM_EIO;
      v.major = q.major;
      v.minor = q.minor;
      v.patchlevel = q.patchlevel;
      v.name_length = q.name_len;
      v.date_length = q.date_len;
      v.description_length = q.desc_len;
      *out = v;
      return MULIB_DRM_OK;
    }
#else
  return MULIB_DRM_ENOTSUP;
#endif
  }
  int
  mulib_drm_get_cap (const mulib_drm_device_t *device, uint64_t capability,
                     uint64_t *out)
  {
    if (!device || device->fd < 0 || !out)
      return MULIB_DRM_EINVAL;
#ifdef __linux__
    {
      mulib_drm_cap_request_t q;
      q.capability = capability;
      q.value = 0;
      if (mulib_drm_ioctl_ (device->fd, MULIB_DRM_GET_CAP, &q) < 0)
        return MULIB_DRM_EIO;
      *out = q.value;
      return MULIB_DRM_OK;
    }
#else
  (void)capability;
  return MULIB_DRM_ENOTSUP;
#endif
  }
  int
  mulib_drm_dumb_create (const mulib_drm_device_t *device, uint32_t width,
                         uint32_t height, uint32_t bpp,
                         mulib_drm_dumb_t *buffer)
  {
    if (!device || device->fd < 0 || !buffer || !width || !height || !bpp
        || bpp > 64)
      return MULIB_DRM_EINVAL;
    if (buffer->fd != -1 || buffer->data || buffer->mapped_size)
      return MULIB_DRM_ESTATE;
#ifdef __linux__
    {
      mulib_drm_create_request_t q;
      int fd, saved, rc = MULIB_DRM_EIO;
      uint64_t minimum_pitch = ((uint64_t)width * bpp + 7) / 8;
      if (minimum_pitch > UINT32_MAX)
        return MULIB_DRM_EOVERFLOW;
      fd = dup (device->fd);
      if (fd < 0)
        return MULIB_DRM_EIO;
      if (fcntl (fd, F_SETFD, FD_CLOEXEC) < 0)
        {
          saved = errno;
          (void)close (fd);
          errno = saved;
          return MULIB_DRM_EIO;
        }
      memset (&q, 0, sizeof (q));
      q.width = width;
      q.height = height;
      q.bpp = bpp;
      if (mulib_drm_ioctl_ (fd, MULIB_DRM_CREATE_DUMB, &q) < 0)
        {
          saved = errno;
          (void)close (fd);
          errno = saved;
          return MULIB_DRM_EIO;
        }
      if (!q.size || q.pitch < minimum_pitch
          || q.size < (uint64_t)q.pitch * height || q.size > SIZE_MAX)
        {
          mulib_drm_destroy_request_t destroy;
          destroy.handle = q.handle;
          if (q.size > SIZE_MAX)
            rc = MULIB_DRM_EOVERFLOW;
          (void)mulib_drm_ioctl_ (fd, MULIB_DRM_DESTROY_DUMB, &destroy);
          (void)close (fd);
          errno = EIO;
          return rc;
        }
      buffer->fd = fd;
      buffer->handle = q.handle;
      buffer->width = width;
      buffer->height = height;
      buffer->bpp = bpp;
      buffer->pitch = q.pitch;
      buffer->size = q.size;
      buffer->data = NULL;
      buffer->mapped_size = 0;
      return MULIB_DRM_OK;
    }
#else
  return MULIB_DRM_ENOTSUP;
#endif
  }
  int
  mulib_drm_dumb_map (mulib_drm_dumb_t *buffer)
  {
    if (!buffer || buffer->fd < 0 || !buffer->size)
      return MULIB_DRM_EINVAL;
#ifdef __linux__
    {
      mulib_drm_map_request_t q;
      off_t offset;
      void *data;
      if (buffer->mapped_size)
        return MULIB_DRM_OK;
      if (buffer->size > SIZE_MAX)
        return MULIB_DRM_EOVERFLOW;
      memset (&q, 0, sizeof (q));
      q.handle = buffer->handle;
      if (mulib_drm_ioctl_ (buffer->fd, MULIB_DRM_MAP_DUMB, &q) < 0)
        return MULIB_DRM_EIO;
      offset = (off_t)q.offset;
      if (offset < 0 || (uint64_t)offset != q.offset)
        return MULIB_DRM_EOVERFLOW;
      data = mmap (NULL, (size_t)buffer->size, PROT_READ | PROT_WRITE,
                   MAP_SHARED, buffer->fd, offset);
      if (data == MAP_FAILED)
        return MULIB_DRM_EIO;
      buffer->data = data;
      buffer->mapped_size = (size_t)buffer->size;
      return MULIB_DRM_OK;
    }
#else
  return MULIB_DRM_ENOTSUP;
#endif
  }
  int
  mulib_drm_dumb_unmap (mulib_drm_dumb_t *buffer)
  {
    if (!buffer)
      return MULIB_DRM_EINVAL;
#ifdef __linux__
    if (buffer->data || buffer->mapped_size)
      {
        if (munmap (buffer->data, buffer->mapped_size) < 0)
          return MULIB_DRM_EIO;
        buffer->data = NULL;
        buffer->mapped_size = 0;
      }
    return MULIB_DRM_OK;
#else
  return MULIB_DRM_ENOTSUP;
#endif
  }
  int
  mulib_drm_dumb_destroy (mulib_drm_dumb_t *buffer)
  {
    if (!buffer)
      return MULIB_DRM_EINVAL;
#ifdef __linux__
    {
      mulib_drm_destroy_request_t q;
      int fd, rc = mulib_drm_dumb_unmap (buffer);
      if (rc)
        return rc;
      if (buffer->fd == -1)
        return MULIB_DRM_OK;
      q.handle = buffer->handle;
      if (mulib_drm_ioctl_ (buffer->fd, MULIB_DRM_DESTROY_DUMB, &q) < 0)
        return MULIB_DRM_EIO;
      fd = buffer->fd;
      memset (buffer, 0, sizeof (*buffer));
      buffer->fd = -1;
      return close (fd) < 0 ? MULIB_DRM_EIO : MULIB_DRM_OK;
    }
#else
  return MULIB_DRM_ENOTSUP;
#endif
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
