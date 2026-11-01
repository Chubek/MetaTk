# mulib-kvm

Linux **Kernel-based Virtual Machine** helpers, not keyboard/video/mouse sharing.
Provides device/VM/vCPU ownership, capability queries, borrowed RAM registration
and one-step execution. Not a complete VMM, firmware loader or architecture setup.

## Integration and lifetime

Define `MULIB_IMPL_KVM` in one C11 translation unit. Requires Linux UAPI headers
on Linux; other platforms compile and return ENOTSUP for native operations.
Initialize handles with `MULIB_KVM_INIT`, `MULIB_KVM_VM_INIT`,
`MULIB_KVM_VCPU_INIT`; never copy or edit owning fields. There is no allocation
of guest RAM. `open(&device,path)` uses `/dev/kvm` for NULL, verifies API version
12 and sets close-on-exec. `check(&device,capability,&value)` returns the native
extension count/boolean. Success is zero; errors are EINVAL=-1, EIO=-2,
ENOTSUP=-3, ESTATE=-4. Native EIO preserves errno.

`vm_create(&device,&vm)` creates machine type zero. `memory(&vm,slot,gpa,host,
size,readonly)` registers caller-owned RAM with page-aligned host/GPA/size and
overflow checks; size zero removes a slot. readonly must be 0/1; check native
KVM_CAP_READONLY_MEM before enabling it. No slot overlap/capacity tracking is
performed; the kernel validates slots and capability support.

`vcpu_create(&device,&vm,id,&cpu)` creates a vCPU fd and maps its native
`struct kvm_run`. Configure architecture-specific registers and devices using
native Linux ioctls on the exposed fds. `run(&cpu,&exit_reason)` invokes KVM_RUN
once: zero success, INTERRUPTED=1 on EINTR/EAGAIN (not retried), otherwise EIO.
Native exit-specific fields are accessible by casting `cpu.run` with Linux headers.

Close vCPUs before releasing RAM; VM and vCPU fds retain kernel VM references
independently. RAM must survive until its slot is removed or **all** references
are gone. `vcpu_close` retains resources on unmap failure for retry. Other close
calls reset even if close reports failure; do not retry an old fd. Use one owning
thread per vCPU and coordinate concurrent teardown. No automatic signal handling.

## Example and verification limits

`cc -std=c11 -Iinclude examples/modules/kvm.c -o kvm` is a hardware-free lifetime
example. Tests cover invalid state and `/dev/null` ioctl failure/cleanup, not
successful guest execution. Real execution requires hardware virtualization,
kernel support, permissions and architecture-specific setup supplied by the caller.
