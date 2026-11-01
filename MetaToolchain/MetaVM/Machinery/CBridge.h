#ifndef METAVM_CBRIDGE_H
#define METAVM_CBRIDGE_H
#include "Value.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Arguments are borrowed for the call. Return an owned-by-this-VM value or a
 * scalar through result. No arbitrary native ABI casts or dynamic loading.
 * A callback may allocate, pin, and collect, but may not destroy/reenter the VM
 * or mutate the running module. user remains owned by the caller. */
typedef mvm_status (*mvm_native)(mvm_vm *, const mvm_value *arguments,
    size_t count, mvm_value *result, void *user, mvm_error *);
/* Duplicate names and registration while executing are errors. */
mvm_status mvm_register_native(mvm_vm *, const char *name, uint32_t arity,
    mvm_native callback, void *user, mvm_error *);
/* Atomic registration: either every binding is installed or none is.
 * Entry names/callbacks are validated before allocation. Input entries and names
 * are borrowed only for this call. user pointers remain caller-owned.
 * count==0 is a no-op for a valid, idle VM. No duplicate names are allowed,
 * including duplicates within the batch. Not thread-safe; synchronize the VM. */
typedef struct mvm_native_binding {
    const char *name;
    uint32_t arity;
    mvm_native callback;
    void *user;
} mvm_native_binding;
mvm_status mvm_register_natives(mvm_vm *, const mvm_native_binding *, size_t count,
    mvm_error *);
#ifdef __cplusplus
}
#endif
#endif
