/* MetaVM: ISO C11, standard-library-only. See README.md for ownership rules. */
#ifndef METAVM_SYSTEM_H
#define METAVM_SYSTEM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum mvm_status {
    MVM_OK = 0, MVM_INVALID, MVM_OOM, MVM_LIMIT, MVM_IO, MVM_FORMAT,
    MVM_VERIFY, MVM_TYPE, MVM_BOUNDS, MVM_DIV_ZERO, MVM_NOT_FOUND,
    MVM_HOST_ERROR, MVM_BUSY
} mvm_status;
typedef struct mvm_error {
    mvm_status status;
    size_t function, instruction, line;
    char message[256];
} mvm_error;
typedef struct mvm_limits {
    size_t max_frames, max_registers, max_heap_bytes, max_module_bytes;
    uint64_t max_steps;
} mvm_limits;
typedef struct mvm_vm mvm_vm;
typedef struct mvm_module mvm_module;
typedef struct mvm_object mvm_object;
typedef struct mvm_root mvm_root;

/* Pure functions; no global mutable state. */
mvm_limits mvm_default_limits(void);
const char *mvm_status_string(mvm_status status);
void mvm_error_clear(mvm_error *error);
#ifdef __cplusplus
}
#endif
#endif
