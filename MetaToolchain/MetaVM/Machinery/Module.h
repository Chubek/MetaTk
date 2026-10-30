#ifndef METAVM_MODULE_H
#define METAVM_MODULE_H
#include "AbstractMachine.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mvm_function_info {
    const char *name;
    uint32_t arity, registers;
    const mvm_instruction *code;
    size_t code_count;
} mvm_function_info;
/* Modules own copies of names and instructions. NULL-safe destroy. Modules
 * must outlive execution; no mutation during execution or concurrent reads.
 * Builders return errors without changing the module; indices start at zero. */
mvm_module *mvm_module_create(void);
void mvm_module_destroy(mvm_module *module);
mvm_status mvm_module_add_function(mvm_module *, const char *name,
    uint32_t arity, uint32_t registers, uint32_t *index, mvm_error *);
mvm_status mvm_module_add_import(mvm_module *, const char *name,
    uint32_t arity, uint32_t *index, mvm_error *);
mvm_status mvm_module_emit(mvm_module *, uint32_t function,
    mvm_instruction instruction, mvm_error *);
size_t mvm_module_function_count(const mvm_module *);
size_t mvm_module_import_count(const mvm_module *);
/* Views are borrowed until mutation/destruction. */
mvm_status mvm_module_function(const mvm_module *, uint32_t index, mvm_function_info *);
mvm_status mvm_module_import(const mvm_module *, uint32_t index,
    const char **name, uint32_t *arity);
mvm_status mvm_module_find(const mvm_module *, const char *name, uint32_t *index);
mvm_status mvm_module_verify(const mvm_module *, mvm_error *);
mvm_status mvm_module_clone(const mvm_module *, mvm_module **output, mvm_error *);
#ifdef __cplusplus
}
#endif
#endif
