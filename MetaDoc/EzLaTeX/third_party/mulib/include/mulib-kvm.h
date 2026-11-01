/**
 * @file mulib-kvm.h
 * @brief Linux Kernel-based Virtual Machine fd, memory-slot and vCPU
 * lifetimes. Define MULIB_IMPL_KVM once. Native architecture setup remains
 * caller-owned.
 */
#ifndef MULIB_KVM_H
#define MULIB_KVM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_KVM_OK = 0,
    MULIB_KVM_EINVAL = -1,
    MULIB_KVM_EIO = -2,
    MULIB_KVM_ENOTSUP = -3,
    MULIB_KVM_ESTATE = -4,
    MULIB_KVM_INTERRUPTED = 1
  };
  /** @brief Owning /dev/kvm or VM fd; initialize and do not copy. */
  typedef struct mulib_kvm
  {
    int fd;
  } mulib_kvm_t;
  typedef struct mulib_kvm_vm
  {
    int fd;
  } mulib_kvm_vm_t;
#define MULIB_KVM_INIT { -1 }
#define MULIB_KVM_VM_INIT { -1 }
  /** @brief Owning vCPU fd and shared struct kvm_run mapping; do not copy or
   * edit fields. */
  typedef struct mulib_kvm_vcpu
  {
    int fd;
    void *run;
    size_t run_size;
  } mulib_kvm_vcpu_t;
#define MULIB_KVM_VCPU_INIT { -1, NULL, 0 }
  /** @brief Open path (NULL = /dev/kvm), require API version 12,
   * close-on-exec. */
  int mulib_kvm_open (mulib_kvm_t *device, const char *path);
  /** @brief Close/reset idempotently; existing VMs remain valid. */
  int mulib_kvm_close (mulib_kvm_t *device);
  /** @brief KVM_CHECK_EXTENSION; zero means unsupported, positive values are
   * native. */
  int mulib_kvm_check (const mulib_kvm_t *device, unsigned capability,
                       int *value);
  /** @brief Create native machine type zero; unchanged on failure. */
  int mulib_kvm_vm_create (const mulib_kvm_t *device, mulib_kvm_vm_t *vm);
  /** @brief Close VM fd; vCPU fds still retain the VM. */
  int mulib_kvm_vm_close (mulib_kvm_vm_t *vm);
  /** @brief Register page-aligned borrowed host RAM; size zero unregisters a
   * slot. RAM must remain mapped until slot removal or destruction of ALL VM
   * references. readonly is 0/1; caller must check KVM_CAP_READONLY_MEM before
   * using 1.
   */
  int mulib_kvm_memory (const mulib_kvm_vm_t *vm, uint32_t slot,
                        uint64_t guest_address, void *host_address,
                        size_t size, int readonly);
  /** @brief Create vCPU and map kvm_run. device must originate from the same
   * KVM backend. */
  int mulib_kvm_vcpu_create (const mulib_kvm_t *device,
                             const mulib_kvm_vm_t *vm, unsigned id,
                             mulib_kvm_vcpu_t *vcpu);
  /** @brief Unmap and close/reset; retains resources on unmap failure. */
  int mulib_kvm_vcpu_close (mulib_kvm_vcpu_t *vcpu);
  /** @brief Run once, returning native exit_reason on success. EINTR/EAGAIN
   * return INTERRUPTED, not retried. Caller configures registers/devices via
   * native ioctls.
   */
  int mulib_kvm_run (mulib_kvm_vcpu_t *vcpu, uint32_t *exit_reason);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_KVM) && !defined(MULIB_KVM_IMPLEMENTED)
#define MULIB_KVM_IMPLEMENTED
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <fcntl.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
#ifdef __linux__
  static int
  mulib_kvm_ioctl_ (int fd, unsigned long request, uintptr_t arg)
  {
    int rc;
    do
      {
        rc = ioctl (fd, request, arg);
      }
    while (rc < 0 && errno == EINTR);
    return rc;
  }
  static int
  mulib_kvm_cloexec_ (int fd)
  {
    int error;
    if (fcntl (fd, F_SETFD, FD_CLOEXEC) == 0)
      return fd;
    error = errno;
    (void)close (fd);
    errno = error;
    return -1;
  }
#endif
  int
  mulib_kvm_open (mulib_kvm_t *d, const char *path)
  {
    if (!d)
      return MULIB_KVM_EINVAL;
    if (d->fd != -1)
      return MULIB_KVM_ESTATE;
#ifdef __linux__
    {
      int fd = open (path ? path : "/dev/kvm", O_RDWR), version, error;
      if (fd < 0 || (fd = mulib_kvm_cloexec_ (fd)) < 0)
        return MULIB_KVM_EIO;
      version = mulib_kvm_ioctl_ (fd, KVM_GET_API_VERSION, 0);
      if (version != 12)
        {
          error = errno;
          (void)close (fd);
          errno = error;
          return version < 0 ? MULIB_KVM_EIO : MULIB_KVM_ENOTSUP;
        }
      d->fd = fd;
      return MULIB_KVM_OK;
    }
#else
  (void)path;
  return MULIB_KVM_ENOTSUP;
#endif
  }
  int
  mulib_kvm_close (mulib_kvm_t *d)
  {
    if (!d)
      return MULIB_KVM_EINVAL;
    if (d->fd == -1)
      return MULIB_KVM_OK;
#ifdef __linux__
    {
      int fd = d->fd;
      d->fd = -1;
      return close (fd) < 0 ? MULIB_KVM_EIO : MULIB_KVM_OK;
    }
#else
  return MULIB_KVM_ENOTSUP;
#endif
  }
  int
  mulib_kvm_check (const mulib_kvm_t *d, unsigned capability, int *out)
  {
    if (!d || d->fd < 0 || !out)
      return MULIB_KVM_EINVAL;
#ifdef __linux__
    {
      int rc = mulib_kvm_ioctl_ (d->fd, KVM_CHECK_EXTENSION, capability);
      if (rc < 0)
        return MULIB_KVM_EIO;
      *out = rc;
      return MULIB_KVM_OK;
    }
#else
  (void)capability;
  return MULIB_KVM_ENOTSUP;
#endif
  }
  int
  mulib_kvm_vm_create (const mulib_kvm_t *d, mulib_kvm_vm_t *vm)
  {
    if (!d || d->fd < 0 || !vm)
      return MULIB_KVM_EINVAL;
    if (vm->fd != -1)
      return MULIB_KVM_ESTATE;
#ifdef __linux__
    {
      int fd = mulib_kvm_ioctl_ (d->fd, KVM_CREATE_VM, 0);
      if (fd < 0 || (fd = mulib_kvm_cloexec_ (fd)) < 0)
        return MULIB_KVM_EIO;
      vm->fd = fd;
      return MULIB_KVM_OK;
    }
#else
  return MULIB_KVM_ENOTSUP;
#endif
  }
  int
  mulib_kvm_vm_close (mulib_kvm_vm_t *vm)
  {
    if (!vm)
      return MULIB_KVM_EINVAL;
    if (vm->fd == -1)
      return MULIB_KVM_OK;
#ifdef __linux__
    {
      int fd = vm->fd;
      vm->fd = -1;
      return close (fd) < 0 ? MULIB_KVM_EIO : MULIB_KVM_OK;
    }
#else
  return MULIB_KVM_ENOTSUP;
#endif
  }
  int
  mulib_kvm_memory (const mulib_kvm_vm_t *vm, uint32_t slot, uint64_t guest,
                    void *host, size_t size, int readonly)
  {
    if (!vm || vm->fd < 0 || (readonly != 0 && readonly != 1)
        || (!host && size) || size > UINT64_MAX - guest
        || size > UINTPTR_MAX - (uintptr_t)host)
      return MULIB_KVM_EINVAL;
#ifdef __linux__
    {
      struct kvm_userspace_memory_region region;
      long page = sysconf (_SC_PAGESIZE);
      if (page <= 0)
        return MULIB_KVM_EIO;
      if (guest % (uint64_t)page || (uintptr_t)host % (uintptr_t)page
          || size % (size_t)page)
        return MULIB_KVM_EINVAL;
      memset (&region, 0, sizeof (region));
      region.slot = slot;
      region.guest_phys_addr = guest;
      region.userspace_addr = (uint64_t)(uintptr_t)host;
      region.memory_size = size;
#ifdef KVM_MEM_READONLY
      region.flags = readonly ? KVM_MEM_READONLY : 0;
#else
      if (readonly)
        return MULIB_KVM_ENOTSUP;
#endif
      return mulib_kvm_ioctl_ (vm->fd, KVM_SET_USER_MEMORY_REGION,
                               (uintptr_t)&region)
                     < 0
                 ? MULIB_KVM_EIO
                 : MULIB_KVM_OK;
    }
#else
  (void)slot;
  return MULIB_KVM_ENOTSUP;
#endif
  }
  int
  mulib_kvm_vcpu_create (const mulib_kvm_t *d, const mulib_kvm_vm_t *vm,
                         unsigned id, mulib_kvm_vcpu_t *cpu)
  {
    if (!d || d->fd < 0 || !vm || vm->fd < 0 || !cpu)
      return MULIB_KVM_EINVAL;
    if (cpu->fd != -1 || cpu->run || cpu->run_size)
      return MULIB_KVM_ESTATE;
#ifdef __linux__
    {
      int size = mulib_kvm_ioctl_ (d->fd, KVM_GET_VCPU_MMAP_SIZE, 0), fd,
          error;
      void *run;
      if (size < 0)
        return MULIB_KVM_EIO;
      if ((size_t)size < sizeof (struct kvm_run))
        return MULIB_KVM_ENOTSUP;
      fd = mulib_kvm_ioctl_ (vm->fd, KVM_CREATE_VCPU, id);
      if (fd < 0 || (fd = mulib_kvm_cloexec_ (fd)) < 0)
        return MULIB_KVM_EIO;
      run = mmap (NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                  0);
      if (run == MAP_FAILED)
        {
          error = errno;
          (void)close (fd);
          errno = error;
          return MULIB_KVM_EIO;
        }
      cpu->fd = fd;
      cpu->run = run;
      cpu->run_size = (size_t)size;
      return MULIB_KVM_OK;
    }
#else
  (void)id;
  return MULIB_KVM_ENOTSUP;
#endif
  }
  int
  mulib_kvm_vcpu_close (mulib_kvm_vcpu_t *cpu)
  {
    if (!cpu)
      return MULIB_KVM_EINVAL;
#ifdef __linux__
    if (cpu->run && munmap (cpu->run, cpu->run_size) < 0)
      return MULIB_KVM_EIO;
    cpu->run = NULL;
    cpu->run_size = 0;
    {
      int fd = cpu->fd;
      cpu->fd = -1;
      return fd < 0 || close (fd) == 0 ? MULIB_KVM_OK : MULIB_KVM_EIO;
    }
#else
  return cpu->fd == -1 && !cpu->run ? MULIB_KVM_OK : MULIB_KVM_ENOTSUP;
#endif
  }
  int
  mulib_kvm_run (mulib_kvm_vcpu_t *cpu, uint32_t *reason)
  {
    if (!cpu || cpu->fd < 0 || !cpu->run || !reason)
      return MULIB_KVM_EINVAL;
#ifdef __linux__
    {
      int rc = ioctl (cpu->fd, KVM_RUN, 0);
      if (rc < 0)
        return errno == EINTR || errno == EAGAIN ? MULIB_KVM_INTERRUPTED
                                                 : MULIB_KVM_EIO;
      *reason = ((struct kvm_run *)cpu->run)->exit_reason;
      return MULIB_KVM_OK;
    }
#else
  return MULIB_KVM_ENOTSUP;
#endif
  }
#ifdef __cplusplus
}
#endif
#endif
