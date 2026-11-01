#define MULIB_IMPL_KVM
#include "mulib-kvm.h"
#include <assert.h>
int main(void) {
    mulib_kvm_t device = MULIB_KVM_INIT; mulib_kvm_vm_t vm = MULIB_KVM_VM_INIT;
    mulib_kvm_vcpu_t cpu = MULIB_KVM_VCPU_INIT;
    /* No implicit hardware access. A native application explicitly calls:
       mulib_kvm_open(&device, NULL); mulib_kvm_vm_create(&device, &vm);
       It must register RAM and configure architecture-specific vCPU registers. */
    assert(mulib_kvm_vcpu_close(&cpu) == 0);
    assert(mulib_kvm_vm_close(&vm) == 0 && mulib_kvm_close(&device) == 0);
    return 0;
}
